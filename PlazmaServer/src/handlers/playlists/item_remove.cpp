#include "item_remove.hpp"

#include <stdexcept>

#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::playlists::item_remove {

namespace pl = real_medium::utils::playlist;
namespace pc = real_medium::handlers::playlists::common;
namespace ops = userver::storages::scylla::operations;

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

    const auto playlist_id = request.GetPathArg("id");
    const auto video_id = request.GetPathArg("video_id");
    if (playlist_id.empty() || video_id.empty() || !pl::IsValidId(playlist_id)) {
        // Idempotent: caller treats it as already-removed.
        request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
        return "";
    }

    try {
        auto meta = pc::LoadPlaylistById(session_, playlist_id);
        if (!meta.has_value() || meta->user_id != auth.user_id) {
            // Don't leak ownership; idempotent 204.
            request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
            return "";
        }

        const auto membership = pc::LookupVideoMembership(session_, auth.user_id, video_id, playlist_id);
        if (!membership.has_value()) {
            // Already removed — 204 per spec §3.8.
            request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
            return "";
        }

        // ── Delete from playlist_items (using the stored added_at).
        try {
            auto table = session_->GetTable("playlist_items");
            ops::DeleteOne d;
            d.WhereString("playlist_id", playlist_id);
            d.WhereInt64("added_at", membership->added_at_ms);
            d.WhereString("video_id", video_id);
            table.Execute(d);
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id
                          << " items delete failed: " << ex.what();
        }
        // ── Delete from membership index.
        try {
            auto table = session_->GetTable("playlists_by_video");
            ops::DeleteOne d;
            d.WhereInt64("user_id", auth.user_id);
            d.WhereString("video_id", video_id);
            d.WhereString("playlist_id", playlist_id);
            table.Execute(d);
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id
                          << " by_video delete failed: " << ex.what();
        }

        // ── Recompute covers + decrement count + bump updated_at.
        const int64_t now_ms = real_medium::utils::video::NowMs();
        std::vector<std::string> new_covers;
        try {
            new_covers = pc::RecomputeCoverThumbnails(session_, playlist_id);
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id
                          << " recompute covers failed: " << ex.what();
            new_covers = meta->cover_thumbnails;  // fall back to old set
        }
        const int new_count = (meta->item_count > 0) ? meta->item_count - 1 : 0;
        try {
            pc::UpdateCountAndCovers(session_, auth.user_id, playlist_id, new_count, new_covers, now_ms);
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id
                          << " update meta failed: " << ex.what();
        }

        LOG_INFO() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id
                   << " user_id=" << auth.user_id << " count=" << new_count;
        request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
        return "";

    } catch (const std::exception& ex) {
        LOG_ERROR() << "DELETE /v1/playlists/" << playlist_id << "/items/" << video_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::item_remove
