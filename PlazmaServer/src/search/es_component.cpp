#include "es_component.hpp"

#include <chrono>
#include <stdexcept>
#include <string>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace real_medium::search {

namespace {

int64_t NowMs() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()
           )
        .count();
}

}  // namespace

EsComponent::EsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : LoggableComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      url_(config["url"].As<std::string>("http://localhost:9200")),
      index_(config["index"].As<std::string>("videos")) {}

userver::yaml_config::Schema EsComponent::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::LoggableComponentBase>(R"(
type: object
description: ElasticSearch / OpenSearch search component
additionalProperties: false
properties:
    url:
        type: string
        description: Base URL of the ES/OpenSearch node
        defaultDescription: http://localhost:9200
    index:
        type: string
        description: Name of the search index alias
        defaultDescription: videos
)");
}

bool EsComponent::IsBreakerOpen() const {
    const int64_t since = open_since_ms_.load(std::memory_order_relaxed);
    if (since == 0) return false;
    if (NowMs() - since > kHalfOpenDelayMs) {
        // Half-open: reset so one probe goes through; if it fails, RecordFailure re-opens.
        open_since_ms_.store(0, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void EsComponent::RecordSuccess() const {
    consecutive_failures_.store(0, std::memory_order_relaxed);
    open_since_ms_.store(0, std::memory_order_relaxed);
}

void EsComponent::RecordFailure() const {
    const int n = consecutive_failures_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n >= kBreakerThreshold) {
        int64_t expected = 0;
        const int64_t now = NowMs();
        // Only open if still closed (CAS avoids repeatedly rewriting the open timestamp).
        open_since_ms_.compare_exchange_strong(expected, now, std::memory_order_relaxed);
        LOG_WARNING() << "es-search: circuit breaker opened after " << n << " consecutive failures";
    }
}

std::string EsComponent::BuildQueryJson(
    const std::string& q,
    int limit,
    SortMode sort,
    std::optional<int64_t> author_filter,
    bool owner_context,
    std::optional<std::string> search_after_json
) const {
    namespace fj = userver::formats::json;
    fj::ValueBuilder body;
    body["size"] = limit;
    body["track_total_hits"] = 10000;

    // ── bool.must: multi_match across indexed fields ──────────────────────
    fj::ValueBuilder mm;
    mm["query"] = q;
    mm["type"] = "best_fields";
    {
        fj::ValueBuilder fields{userver::formats::common::Type::kArray};
        fields.PushBack("title^3");
        fields.PushBack("author^2");
        fields.PushBack("description^1");
        fields.PushBack("tags^1.5");
        mm["fields"] = fields.ExtractValue();
    }
    mm["operator"] = "and";
    mm["fuzziness"] = "AUTO:4,7";
    mm["prefix_length"] = 1;

    fj::ValueBuilder must_arr{userver::formats::common::Type::kArray};
    {
        fj::ValueBuilder w;
        w["multi_match"] = mm.ExtractValue();
        must_arr.PushBack(w.ExtractValue());
    }

    // ── bool.should: phrase boosts ────────────────────────────────────────
    fj::ValueBuilder should_arr{userver::formats::common::Type::kArray};
    {
        fj::ValueBuilder ph;
        ph["match_phrase"]["title"]["query"] = q;
        ph["match_phrase"]["title"]["boost"] = 4;
        should_arr.PushBack(ph.ExtractValue());
    }
    {
        fj::ValueBuilder ph;
        ph["match_phrase"]["author"]["query"] = q;
        ph["match_phrase"]["author"]["boost"] = 2;
        should_arr.PushBack(ph.ExtractValue());
    }

    // ── bool.filter: visibility + optional author ─────────────────────────
    fj::ValueBuilder filter_arr{userver::formats::common::Type::kArray};
    if (!owner_context) {
        fj::ValueBuilder vf;
        vf["term"]["visibility"] = "public";
        filter_arr.PushBack(vf.ExtractValue());
    }
    if (author_filter.has_value()) {
        fj::ValueBuilder af;
        af["term"]["user_id"] = *author_filter;
        filter_arr.PushBack(af.ExtractValue());
    }

    fj::ValueBuilder bool_q;
    bool_q["must"] = must_arr.ExtractValue();
    bool_q["should"] = should_arr.ExtractValue();
    bool_q["filter"] = filter_arr.ExtractValue();

    // ── function_score: recency decay + view-count boost ─────────────────
    fj::ValueBuilder fs;
    {
        fj::ValueBuilder bw;
        bw["bool"] = bool_q.ExtractValue();
        fs["query"] = bw.ExtractValue();
    }
    {
        fj::ValueBuilder fns{userver::formats::common::Type::kArray};
        {
            fj::ValueBuilder fn;
            fn["gauss"]["created_at"]["origin"] = "now";
            fn["gauss"]["created_at"]["scale"] = "14d";
            fn["gauss"]["created_at"]["offset"] = "1d";
            fn["gauss"]["created_at"]["decay"] = 0.5;
            fns.PushBack(fn.ExtractValue());
        }
        {
            fj::ValueBuilder fn;
            fn["field_value_factor"]["field"] = "views_7d";
            fn["field_value_factor"]["modifier"] = "log1p";
            fn["field_value_factor"]["missing"] = 0;
            fns.PushBack(fn.ExtractValue());
        }
        fs["functions"] = fns.ExtractValue();
    }
    fs["score_mode"] = "sum";
    fs["boost_mode"] = "multiply";

    fj::ValueBuilder query_node;
    query_node["function_score"] = fs.ExtractValue();
    body["query"] = query_node.ExtractValue();

    // ── sort ──────────────────────────────────────────────────────────────
    {
        fj::ValueBuilder sort_arr{userver::formats::common::Type::kArray};
        if (sort == SortMode::kRelevance) {
            sort_arr.PushBack("_score");
        } else if (sort == SortMode::kPopular) {
            fj::ValueBuilder s;
            s["views_7d"] = "desc";
            sort_arr.PushBack(s.ExtractValue());
        }
        {
            fj::ValueBuilder s;
            s["created_at"] = "desc";
            sort_arr.PushBack(s.ExtractValue());
        }
        {
            fj::ValueBuilder s;
            s["video_id"] = "desc";
            sort_arr.PushBack(s.ExtractValue());
        }
        body["sort"] = sort_arr.ExtractValue();
    }

    // ── search_after (cursor pagination) ─────────────────────────────────
    if (search_after_json.has_value()) {
        try {
            body["search_after"] = fj::FromString(*search_after_json);
        } catch (...) {
            // Caller already validated the cursor; silently skip on malform.
        }
    }

    return fj::ToString(body.ExtractValue());
}

std::optional<EsSearchResult> EsComponent::Search(
    const std::string& query,
    int limit,
    SortMode sort,
    std::optional<int64_t> author_filter,
    bool owner_context,
    std::optional<std::string> search_after_json
) const {
    if (IsBreakerOpen()) return std::nullopt;

    const std::string endpoint = url_ + "/" + index_ + "/_search";
    const std::string body = BuildQueryJson(query, limit, sort, author_filter, owner_context, search_after_json);
    const int64_t t0 = NowMs();

    try {
        auto response = http_.CreateRequest()
                            .post(endpoint, body)
                            .headers({{"Content-Type", "application/json"}})
                            .timeout(std::chrono::milliseconds{500})
                            .perform();

        const int64_t elapsed = NowMs() - t0;

        if (response->status_code() != 200) {
            LOG_WARNING() << "es-search: HTTP " << response->status_code() << " from ES";
            RecordFailure();
            return std::nullopt;
        }

        const auto json = userver::formats::json::FromString(response->body());

        EsSearchResult result;
        result.query_time_ms = elapsed;

        const auto total = json["hits"]["total"];
        if (!total.IsMissing() && !total.IsNull()) {
            result.total_estimate = total["value"].As<int64_t>(0);
        }

        const auto hits_arr = json["hits"]["hits"];
        if (!hits_arr.IsMissing() && hits_arr.IsArray()) {
            for (const auto& hit : hits_arr) {
                EsHit h;
                h.score = hit["_score"].As<double>(0.0);

                const auto src = hit["_source"];
                h.video_id = src["video_id"].As<std::string>("");
                h.user_id = src["user_id"].As<int64_t>(0);
                h.author = src["author"].As<std::string>("");
                h.title = src["title"].As<std::string>("");
                h.storage_url = src["storage_url"].As<std::string>("");
                h.mime = src["mime"].As<std::string>("");
                h.size_bytes = src["size_bytes"].As<int64_t>(0);
                h.visibility = src["visibility"].As<std::string>("public");
                h.created_at_ms = src["created_at"].As<int64_t>(0);
                h.thumbnail_url = src["thumbnail_url"].As<std::string>("");
                h.storyboard_url = src["storyboard_url"].As<std::string>("");

                const auto dur = src["duration_ms"];
                if (!dur.IsMissing() && !dur.IsNull()) {
                    h.duration_ms = dur.As<int64_t>(0);
                }

                const auto sort_vals = hit["sort"];
                if (!sort_vals.IsMissing()) {
                    h.sort_values_json = userver::formats::json::ToString(sort_vals);
                }

                result.hits.push_back(std::move(h));
            }
        }

        RecordSuccess();
        return result;

    } catch (const std::exception& ex) {
        LOG_WARNING() << "es-search: request failed: " << ex.what();
        RecordFailure();
        return std::nullopt;
    }
}

}  // namespace real_medium::search
