#include "detail.hpp"

#include <algorithm>
#include <stdexcept>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/value.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"

namespace real_medium::handlers::playlists::detail {

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
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return R"({"error": "playlist not found"})";
    }

    int limit = pl::kDetailFirstPageDefault;
    const auto limit_str = request.GetArg("limit");
    if (!limit_str.empty()) {
        try {
            limit = std::clamp(std::stoi(limit_str), 1, pl::kDetailFirstPageMax);
        } catch (const std::exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "limit must be an integer in [1, 100]"})";
        }
    }

    try {
        auto meta = pc::LoadPlaylistById(session_, playlist_id);
        if (!meta.has_value() || meta->user_id != auth.user_id) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "playlist not found"})";
        }

        // Fetch first page of items in ascending added_at order. We pull
        // limit+1 to detect "more pages" cheaply.
        std::vector<pl::ItemSnapshot> items;
        items.reserve(limit + 1);
        bool has_more = false;
        {
            auto rows = session_->Execute(
                "SELECT video_id, title, storage_url, thumbnail_url, storyboard_url, mime, "
                "       size_bytes, duration_ms, author, description, added_at "
                "FROM playlist_items WHERE playlist_id = ? "
                "ORDER BY added_at ASC, video_id ASC LIMIT ?",
                std::vector<Value>{Value{playlist_id}, Value{static_cast<int32_t>(limit + 1)}}
            );
            for (const auto& row : rows) {
                if (static_cast<int>(items.size()) == limit) {
                    has_more = true;
                    break;
                }
                items.push_back(pl::ItemFromRow(row));
            }
        }

        userver::formats::json::ValueBuilder items_arr{userver::formats::common::Type::kArray};
        for (const auto& it : items) {
            items_arr.PushBack(pl::BuildItemJson(it));
        }

        userver::formats::json::Value next_cursor = pl::NullJson();
        if (has_more && !items.empty()) {
            pl::ItemCursor c;
            c.added_at_ms = items.back().added_at_ms;
            c.video_id = items.back().video_id;
            next_cursor = userver::formats::json::ValueBuilder{pl::EncodeItemCursor(c)}.ExtractValue();
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
        response["items"] = items_arr.ExtractValue();
        response["next_cursor"] = next_cursor;
        response["total"] = meta->item_count;

        LOG_INFO() << "GET /v1/playlists/" << playlist_id << " user_id=" << auth.user_id
                   << " items=" << items.size() << " total=" << meta->item_count;
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/playlists/" << playlist_id << " user_id=" << auth.user_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::detail
