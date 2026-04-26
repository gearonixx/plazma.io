#include "rename.hpp"

#include <stdexcept>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::playlists::rename {

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

    const auto playlist_id = request.GetPathArg("id");
    if (playlist_id.empty() || !pl::IsValidId(playlist_id)) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return R"({"error": "playlist not found"})";
    }

    userver::formats::json::Value body;
    try {
        body = userver::formats::json::FromString(request.RequestBody());
    } catch (const std::exception&) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnprocessableEntity);
        return R"({"error": "invalid JSON"})";
    }

    const std::string raw_name = body["name"].As<std::string>("");
    const std::string trimmed = pl::TrimName(raw_name);
    if (trimmed.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "name is required"})";
    }
    if (trimmed.size() > pl::kMaxNameLen) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "name too long"})";
    }
    const std::string new_name_lower = pl::CaseFold(trimmed);

    try {
        auto meta = pc::LoadPlaylistById(session_, playlist_id);
        if (!meta.has_value() || meta->user_id != auth.user_id) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "playlist not found"})";
        }

        const std::string old_name_lower = pl::CaseFold(meta->name);
        const int64_t now_ms = real_medium::utils::video::NowMs();

        if (old_name_lower == new_name_lower) {
            // Idempotent rename: same case-folded form. Update display name if
            // it changed (e.g. "Mix" → "MIX"); leave updated_at fresh either way.
            if (meta->name != trimmed) {
                pc::UpdateName(session_, auth.user_id, playlist_id, trimmed, now_ms);
                meta->name = trimmed;
                meta->updated_at_ms = now_ms;
            }
        } else {
            // Reserve the new name first; if that fails, surface 409.
            std::string existing_pid;
            if (!pc::TryReserveName(session_, auth.user_id, new_name_lower, playlist_id, &existing_pid)) {
                request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
                return R"({"error": "name already exists"})";
            }
            pc::UpdateName(session_, auth.user_id, playlist_id, trimmed, now_ms);
            // Best-effort release of the old name index. If this fails the
            // user keeps both reservations until next rename — annoying but
            // never wrong.
            try {
                pc::ReleaseName(session_, auth.user_id, old_name_lower);
            } catch (const std::exception& ex) {
                LOG_WARNING() << "PATCH /v1/playlists/" << playlist_id
                              << " release old name failed: " << ex.what();
            }
            meta->name = trimmed;
            meta->updated_at_ms = now_ms;
        }

        userver::formats::json::ValueBuilder response;
        response["playlist"] = pl::BuildPlaylistJson(
            meta->playlist_id,
            meta->name,
            meta->created_at_ms,
            meta->updated_at_ms,
            meta->item_count,
            meta->cover_thumbnails
        );
        LOG_INFO() << "PATCH /v1/playlists/" << playlist_id << " user_id=" << auth.user_id;
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "PATCH /v1/playlists/" << playlist_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::rename
