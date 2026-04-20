#include "video_list.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/row.hpp>

#include "utils/auth.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::videos::list {

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : HttpHandlerBase(config, context),
    session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {
}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    request.GetHttpResponse().SetContentType("application/json");

    // Optional auth to determine visibility scope
    const auto auth = utils::ExtractAuth(request);
    if (auth.result == utils::AuthResult::kInvalid) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "invalid or expired token"})";
    }

    // Parse ?limit= [1, 100]
    int limit = 20;
    const auto limit_str = request.GetArg("limit");
    if (!limit_str.empty()) {
        try {
            limit = std::clamp(std::stoi(limit_str), 1, 100);
        } catch (const std::exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "limit must be an integer in [1, 100]"})";
        }
    }

    // Parse optional case-insensitive substring filter
    std::string q_lower = request.GetArg("q");
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);

    // Parse optional ?author=<user_id>
    const auto author_str = request.GetArg("author");
    const bool has_author = !author_str.empty();
    int64_t author_id = 0;
    if (has_author) {
        try {
            author_id = std::stoll(author_str);
        } catch (const std::exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "author must be a valid user_id integer"})";
        }
        if (author_id <= 0) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "author must be a positive user_id"})";
        }
    }

    userver::formats::json::ValueBuilder videos_arr{userver::formats::common::Type::kArray};

    try {
        if (has_author) {
            // Per-user listing from `videos` partition (video_id DESC = newest first).
            // Owners see all their videos; everyone else sees only public.
            const bool is_owner = (auth.result == utils::AuthResult::kAuthenticated
                                   && auth.user_id == author_id);

            auto table = session_->GetTable("videos");
            userver::storages::scylla::operations::SelectMany select;
            select.AddAllColumns();
            select.WhereInt64("user_id", author_id);

            int collected = 0;
            for (const auto& row : table.Execute(select)) {
                if (collected >= limit) break;

                const auto visibility = row.IsNull("visibility")
                    ? std::string{"public"} : row.Get<std::string>("visibility");

                // Non-owners cannot see private or unlisted videos
                if (!is_owner && visibility != "public") continue;

                const auto title = row.Get<std::string>("title");
                if (!q_lower.empty()) {
                    std::string t = title;
                    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                    if (t.find(q_lower) == std::string::npos) continue;
                }

                const int64_t created_at_ms = row.IsNull("created_at_ms")
                    ? 0LL : row.Get<int64_t>("created_at_ms");

                videos_arr.PushBack(utils::video::BuildVideoJson(
                    row.Get<std::string>("video_id"),
                    author_id,
                    title,
                    row.Get<std::string>("storage_url"),
                    row.IsNull("mime")       ? std::string{} : row.Get<std::string>("mime"),
                    row.IsNull("size_bytes") ? 0LL           : row.Get<int64_t>("size_bytes"),
                    std::nullopt,
                    row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url"),
                    visibility,
                    created_at_ms,
                    row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url")
                ));
                ++collected;
            }
        } else {
            // Global feed from `videos_by_day`: only public videos are in this table
            // (private/unlisted are excluded at write time in video_create).
            const int64_t now_ms = utils::video::NowMs();
            auto table = session_->GetTable("videos_by_day");
            int collected = 0;

            for (int day_offset = 0; day_offset < 30 && collected < limit; ++day_offset) {
                const int64_t day_ms = static_cast<int64_t>(day_offset) * 86'400'000LL;
                const auto day = utils::video::DayString(now_ms - day_ms);

                userver::storages::scylla::operations::SelectMany select;
                select.AddAllColumns();
                select.WhereString("day", day);
                // Bound the per-partition scan; 4x over-fetch to absorb any
                // filtered-out rows while keeping partition reads reasonable.
                select.SetLimit(static_cast<uint32_t>((limit - collected) * 4));

                for (const auto& row : table.Execute(select)) {
                    if (collected >= limit) break;

                    const auto title = row.Get<std::string>("title");
                    if (!q_lower.empty()) {
                        std::string t = title;
                        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                        if (t.find(q_lower) == std::string::npos) continue;
                    }

                    videos_arr.PushBack(utils::video::BuildVideoJson(
                        row.Get<std::string>("video_id"),
                        row.Get<int64_t>("user_id"),
                        title,
                        row.Get<std::string>("storage_url"),
                        row.IsNull("mime")       ? std::string{} : row.Get<std::string>("mime"),
                        row.IsNull("size_bytes") ? 0LL           : row.Get<int64_t>("size_bytes"),
                        std::nullopt,
                        row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url"),
                        "public",  // only public videos land in videos_by_day
                        row.IsNull("created_at") ? 0LL : row.Get<int64_t>("created_at"),
                        row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url")
                    ));
                    ++collected;
                }
            }
        }
    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/videos failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        userver::formats::json::ValueBuilder err;
        err["error"] = std::string{ex.what()};
        return userver::formats::json::ToString(err.ExtractValue());
    }

    userver::formats::json::ValueBuilder response;
    response["videos"] = videos_arr.ExtractValue();
    // Cursor pagination is not yet implemented; clients should re-request with a
    // narrower time window or use the author= parameter for personal feeds.
    response["next_cursor"] = userver::formats::json::ValueBuilder{
        userver::formats::common::Type::kNull}.ExtractValue();
    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace real_medium::handlers::videos::list
