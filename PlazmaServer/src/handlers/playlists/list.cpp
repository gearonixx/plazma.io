#include "list.hpp"

#include <algorithm>
#include <stdexcept>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"

namespace real_medium::handlers::playlists::list {

namespace pl = real_medium::utils::playlist;
namespace pc = real_medium::handlers::playlists::common;

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerBase(config, context),
      session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    request.GetHttpResponse().SetContentType("application/json");

    const auto auth = real_medium::utils::ExtractAuth(request);
    if (auth.result == real_medium::utils::AuthResult::kInvalid) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "invalid or expired token"})";
    }
    if (auth.result != real_medium::utils::AuthResult::kAuthenticated) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "authentication required"})";
    }

    int limit = pl::kDefaultListLimit;
    const auto limit_str = request.GetArg("limit");
    if (!limit_str.empty()) {
        try {
            limit = std::clamp(std::stoi(limit_str), 1, pl::kMaxListLimit);
        } catch (const std::exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "limit must be an integer in [1, 200]"})";
        }
    }

    enum class Sort { kName, kRecent };
    Sort sort = Sort::kName;
    const auto sort_str = request.GetArg("sort");
    if (!sort_str.empty()) {
        if (sort_str == "name") {
            sort = Sort::kName;
        } else if (sort_str == "recent") {
            sort = Sort::kRecent;
        } else {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "sort must be one of: name, recent"})";
        }
    }

    std::vector<pc::PlaylistMeta> playlists;
    try {
        playlists = pc::LoadAllPlaylistsForUser(session_, auth.user_id);
    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/playlists user_id=" << auth.user_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }

    // In-memory sort. The owner-side index is partitioned by user_id so we
    // already loaded a single partition; ordering it here is cheap (≤ 200).
    if (sort == Sort::kName) {
        std::sort(playlists.begin(), playlists.end(), [](const auto& a, const auto& b) {
            const auto al = pl::CaseFold(a.name);
            const auto bl = pl::CaseFold(b.name);
            if (al != bl) return al < bl;
            return a.playlist_id < b.playlist_id;
        });
    } else {
        std::sort(playlists.begin(), playlists.end(), [](const auto& a, const auto& b) {
            if (a.updated_at_ms != b.updated_at_ms) return a.updated_at_ms > b.updated_at_ms;
            return a.playlist_id < b.playlist_id;
        });
    }

    if (static_cast<int>(playlists.size()) > limit) playlists.resize(limit);

    userver::formats::json::ValueBuilder arr{userver::formats::common::Type::kArray};
    for (const auto& p : playlists) {
        arr.PushBack(pl::BuildPlaylistJson(
            p.playlist_id, p.name, p.created_at_ms, p.updated_at_ms, p.item_count, p.cover_thumbnails
        ));
    }

    userver::formats::json::ValueBuilder response;
    response["playlists"] = arr.ExtractValue();
    response["next_cursor"] = pl::NullJson();
    LOG_INFO() << "GET /v1/playlists user_id=" << auth.user_id << " count=" << playlists.size() << " sort=" << sort_str;
    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace real_medium::handlers::playlists::list
