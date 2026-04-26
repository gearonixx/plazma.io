#include "create.hpp"

#include <stdexcept>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/utils/uuid7.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::playlists::create {

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

    // ── Parse body ────────────────────────────────────────────────────────
    userver::formats::json::Value body;
    try {
        body = userver::formats::json::FromString(request.RequestBody());
    } catch (const std::exception&) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnprocessableEntity);
        return R"({"error": "invalid JSON"})";
    }

    const std::string raw_name = body["name"].As<std::string>("");
    const std::string trimmed_name = pl::TrimName(raw_name);
    if (trimmed_name.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "name is required"})";
    }
    if (trimmed_name.size() > pl::kMaxNameLen) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "name too long"})";
    }
    const std::string name_lower = pl::CaseFold(trimmed_name);

    std::string client_id = body["id"].As<std::string>("");
    if (!client_id.empty() && !pl::IsValidId(client_id)) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "invalid id"})";
    }
    const std::string playlist_id = client_id.empty() ? userver::utils::generators::GenerateUuidV7() : client_id;

    try {
        // ── Idempotency on (user_id, playlist_id) ─────────────────────────
        if (auto existing = pc::LoadPlaylistById(session_, playlist_id); existing.has_value()) {
            if (existing->user_id != auth.user_id) {
                request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
                return R"({"error": "id collision"})";
            }
            const auto existing_name_lower = pl::CaseFold(existing->name);
            if (existing_name_lower == name_lower) {
                userver::formats::json::ValueBuilder response;
                response["playlist"] = pl::BuildPlaylistJson(
                    existing->playlist_id,
                    existing->name,
                    existing->created_at_ms,
                    existing->updated_at_ms,
                    existing->item_count,
                    existing->cover_thumbnails
                );
                request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
                LOG_INFO() << "POST /v1/playlists user_id=" << auth.user_id
                           << " playlist_id=" << playlist_id << " idempotent_hit=1";
                return userver::formats::json::ToString(response.ExtractValue());
            }
            request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
            return R"({"error": "id collision"})";
        }

        // ── Per-user count cap ────────────────────────────────────────────
        {
            auto table = session_->GetTable("playlists_by_user");
            ops::Count cnt;
            cnt.WhereInt64("user_id", auth.user_id);
            const int64_t n = table.Execute(cnt);
            if (n >= pl::kMaxPlaylistsPerUser) {
                request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
                return R"({"error": "playlist limit reached"})";
            }
        }

        // ── Reserve the case-folded name ──────────────────────────────────
        std::string existing_pid;
        if (!pc::TryReserveName(session_, auth.user_id, name_lower, playlist_id, &existing_pid)) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kConflict);
            return R"({"error": "name already exists"})";
        }

        // ── Write metadata ────────────────────────────────────────────────
        const int64_t now_ms = real_medium::utils::video::NowMs();
        pc::PlaylistMeta meta;
        meta.playlist_id = playlist_id;
        meta.user_id = auth.user_id;
        meta.name = trimmed_name;
        meta.created_at_ms = now_ms;
        meta.updated_at_ms = now_ms;
        meta.item_count = 0;
        // cover_thumbnails left empty
        pc::InsertMetadataBoth(session_, meta);

        userver::formats::json::ValueBuilder response;
        response["playlist"] = pl::BuildPlaylistJson(
            meta.playlist_id, meta.name, meta.created_at_ms, meta.updated_at_ms, meta.item_count, meta.cover_thumbnails
        );
        request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
        LOG_INFO() << "POST /v1/playlists user_id=" << auth.user_id << " playlist_id=" << playlist_id << " created=1";
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "POST /v1/playlists user_id=" << auth.user_id << " playlist_id=" << playlist_id
                    << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::create
