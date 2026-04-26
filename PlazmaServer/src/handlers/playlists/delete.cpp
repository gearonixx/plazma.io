#include "delete.hpp"

#include <stdexcept>
#include <vector>

#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/value.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"

namespace real_medium::handlers::playlists::del {

namespace pl = real_medium::utils::playlist;
namespace pc = real_medium::handlers::playlists::common;
namespace ops = userver::storages::scylla::operations;
using userver::storages::scylla::Value;

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
        // Idempotent: caller already considers it gone — return 204 either way.
        request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
        return "";
    }

    try {
        auto meta = pc::LoadPlaylistById(session_, playlist_id);
        if (!meta.has_value()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
            return "";
        }
        if (meta->user_id != auth.user_id) {
            // Don't leak ownership; return idempotent 204 like the spec.
            request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
            return "";
        }

        // ── 1. Drop owner-side index first so the playlist disappears from the
        //     user's list right away. (Spec §5.2 ordering: visible-disappearance
        //     beats ghost-row.)
        {
            auto table = session_->GetTable("playlists_by_user");
            ops::DeleteOne d;
            d.WhereInt64("user_id", auth.user_id);
            d.WhereString("playlist_id", playlist_id);
            table.Execute(d);
        }
        // ── 2. Point lookup gone too.
        {
            auto table = session_->GetTable("playlist_by_id");
            ops::DeleteOne d;
            d.WhereString("playlist_id", playlist_id);
            table.Execute(d);
        }
        // ── 3. Release the case-folded name index entry.
        try {
            pc::ReleaseName(session_, auth.user_id, pl::CaseFold(meta->name));
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id
                          << " release name failed: " << ex.what();
        }
        // ── 4. Read all member video_ids so we can purge the membership index.
        std::vector<std::string> member_video_ids;
        try {
            auto rows = session_->Execute(
                "SELECT video_id FROM playlist_items WHERE playlist_id = ?",
                std::vector<Value>{Value{playlist_id}}
            );
            member_video_ids.reserve(rows.size());
            for (const auto& row : rows) {
                if (!row.IsNull("video_id")) {
                    member_video_ids.push_back(row.Get<std::string>("video_id"));
                }
            }
        } catch (const std::exception& ex) {
            LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id << " read members failed: " << ex.what();
        }
        // ── 4b. Delete the membership index entries one by one.
        for (const auto& vid : member_video_ids) {
            try {
                auto table = session_->GetTable("playlists_by_video");
                ops::DeleteOne d;
                d.WhereInt64("user_id", auth.user_id);
                d.WhereString("video_id", vid);
                d.WhereString("playlist_id", playlist_id);
                table.Execute(d);
            } catch (const std::exception& ex) {
                LOG_WARNING() << "DELETE /v1/playlists/" << playlist_id
                              << " purge by_video vid=" << vid << " failed: " << ex.what();
            }
        }
        // ── 5. Drop the entire items partition with a single CQL statement.
        try {
            session_->ExecuteVoid(
                "DELETE FROM playlist_items WHERE playlist_id = ?",
                std::vector<Value>{Value{playlist_id}}
            );
        } catch (const std::exception& ex) {
            LOG_ERROR() << "DELETE /v1/playlists/" << playlist_id << " drop items failed: " << ex.what();
        }

        LOG_INFO() << "DELETE /v1/playlists/" << playlist_id << " user_id=" << auth.user_id
                   << " items=" << member_video_ids.size();
        request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
        return "";

    } catch (const std::exception& ex) {
        LOG_ERROR() << "DELETE /v1/playlists/" << playlist_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::del
