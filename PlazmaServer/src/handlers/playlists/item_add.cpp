#include "item_add.hpp"

#include <algorithm>
#include <stdexcept>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/value.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"
#include "utils/playlist.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::playlists::item_add {

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

namespace {

// Read the canonical video row. Returns nullopt if no row exists.
// The fields below are merged with the optional client snapshot per spec §3.7
// (canonical wins where they disagree).
struct CanonicalVideo {
    int64_t user_id = 0;
    std::string title;
    std::string storage_url;
    std::string thumbnail_url;
    std::string storyboard_url;
    std::string mime;
    int64_t size_bytes = 0;
    std::optional<int64_t> duration_ms;
};

std::optional<CanonicalVideo> LoadCanonicalVideo(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& video_id
) {
    auto table = session->GetTable("video_by_id");
    ops::SelectOne sel;
    sel.AddAllColumns();
    sel.WhereString("video_id", video_id);
    auto row = table.Execute(sel);
    if (row.Empty()) return std::nullopt;
    CanonicalVideo v;
    v.user_id = row.IsNull("user_id") ? 0LL : row.Get<int64_t>("user_id");
    v.title = row.IsNull("title") ? std::string{} : row.Get<std::string>("title");
    v.storage_url = row.IsNull("storage_url") ? std::string{} : row.Get<std::string>("storage_url");
    v.thumbnail_url = row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url");
    v.storyboard_url = row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url");
    v.mime = row.IsNull("mime") ? std::string{} : row.Get<std::string>("mime");
    v.size_bytes = row.IsNull("size_bytes") ? 0LL : row.Get<int64_t>("size_bytes");
    if (!row.IsNull("duration_ms")) v.duration_ms = row.Get<int64_t>("duration_ms");
    return v;
}

// Read an existing playlist_items row by (playlist_id, added_at, video_id) and
// fill an ItemSnapshot — used by the idempotent "already added" path.
std::optional<pl::ItemSnapshot> LoadExistingItem(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& playlist_id,
    int64_t added_at_ms,
    const std::string& video_id
) {
    auto table = session->GetTable("playlist_items");
    ops::SelectOne sel;
    sel.AddAllColumns();
    sel.WhereString("playlist_id", playlist_id);
    sel.WhereInt64("added_at", added_at_ms);
    sel.WhereString("video_id", video_id);
    auto row = table.Execute(sel);
    if (row.Empty()) return std::nullopt;
    return pl::ItemFromRow(row);
}

// Optional client-supplied snapshot from the request body. Each field may be
// missing; missing fields are filled from the canonical video row.
struct ClientSnapshot {
    std::optional<std::string> title;
    std::optional<std::string> url;
    std::optional<std::string> thumbnail;
    std::optional<std::string> storyboard;
    std::optional<std::string> mime;
    std::optional<int64_t> size;
    std::optional<int64_t> duration_ms;
    std::optional<std::string> author;
    std::optional<std::string> description;
};

template <typename T>
std::optional<T> ReadOptional(const userver::formats::json::Value& v) {
    if (v.IsMissing() || v.IsNull()) return std::nullopt;
    try {
        return v.As<T>();
    } catch (...) {
        return std::nullopt;
    }
}

ClientSnapshot ParseSnapshot(const userver::formats::json::Value& body) {
    ClientSnapshot s;
    s.title = ReadOptional<std::string>(body["title"]);
    s.url = ReadOptional<std::string>(body["url"]);
    s.thumbnail = ReadOptional<std::string>(body["thumbnail"]);
    s.storyboard = ReadOptional<std::string>(body["storyboard"]);
    s.mime = ReadOptional<std::string>(body["mime"]);
    s.size = ReadOptional<int64_t>(body["size"]);
    s.duration_ms = ReadOptional<int64_t>(body["duration_ms"]);
    s.author = ReadOptional<std::string>(body["author"]);
    s.description = ReadOptional<std::string>(body["description"]);
    return s;
}

}  // namespace

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

    const std::string video_id = body["video_id"].As<std::string>("");
    if (video_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "video_id is required"})";
    }

    try {
        auto meta = pc::LoadPlaylistById(session_, playlist_id);
        if (!meta.has_value() || meta->user_id != auth.user_id) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "playlist not found"})";
        }

        // ── Idempotency on (playlist_id, video_id) via membership LWT ────
        // First try a cheap existence check, then fall back to the LWT.
        if (auto existing = pc::LookupVideoMembership(session_, auth.user_id, video_id, playlist_id);
            existing.has_value()) {
            // Re-add — return the existing row with its original added_at.
            if (auto item = LoadExistingItem(session_, playlist_id, existing->added_at_ms, video_id);
                item.has_value()) {
                userver::formats::json::ValueBuilder response;
                response["item"] = pl::BuildItemJson(*item);
                response["playlist"] = pl::BuildPlaylistJson(
                    meta->playlist_id, meta->name, meta->created_at_ms, meta->updated_at_ms,
                    meta->item_count, meta->cover_thumbnails
                );
                request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
                LOG_INFO() << "POST /v1/playlists/" << playlist_id << "/items video_id=" << video_id
                           << " idempotent_hit=1 user_id=" << auth.user_id;
                return userver::formats::json::ToString(response.ExtractValue());
            }
            // Membership index points at a non-existent row — treat as fresh
            // add but reuse the existing added_at timestamp to keep stable.
        }

        // ── Item count cap ───────────────────────────────────────────────
        if (meta->item_count >= pl::kMaxItemsPerPlaylist) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kPayloadTooLarge);
            return R"({"error": "playlist is full"})";
        }

        // ── Verify the video exists & merge with optional client snapshot
        const auto canonical = LoadCanonicalVideo(session_, video_id);
        if (!canonical.has_value()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "video not found"})";
        }
        const auto client = ParseSnapshot(body);

        pl::ItemSnapshot snap;
        snap.video_id = video_id;
        snap.title = client.title.value_or(canonical->title);
        snap.storage_url = canonical->storage_url;  // canonical wins for storage URL
        snap.thumbnail_url = canonical->thumbnail_url;
        snap.storyboard_url = canonical->storyboard_url;
        snap.mime = canonical->mime;
        snap.size_bytes = canonical->size_bytes;
        snap.duration_ms = canonical->duration_ms.has_value() ? canonical->duration_ms : client.duration_ms;
        snap.author = client.author.value_or(std::string{});
        snap.description = client.description.value_or(std::string{});

        // ── LWT-protect "first add wins" on (user_id, video_id, playlist_id)
        const int64_t now_ms = real_medium::utils::video::NowMs();
        snap.added_at_ms = now_ms;
        {
            auto table = session_->GetTable("playlists_by_video");
            ops::InsertOne ins;
            ins.BindInt64("user_id", auth.user_id);
            ins.BindString("video_id", video_id);
            ins.BindString("playlist_id", playlist_id);
            ins.BindInt64("added_at", now_ms);
            ins.IfNotExists();
            const auto res = table.ExecuteLwt(ins);
            if (!res.applied) {
                // Race lost — another writer added it just now. Pick up the
                // existing added_at from the LwtResult and load the row.
                int64_t existing_added_at = now_ms;
                if (!res.previous.IsNull("added_at")) {
                    existing_added_at = res.previous.Get<int64_t>("added_at");
                }
                snap.added_at_ms = existing_added_at;
                if (auto item = LoadExistingItem(session_, playlist_id, existing_added_at, video_id);
                    item.has_value()) {
                    snap = *item;
                }
                userver::formats::json::ValueBuilder response;
                response["item"] = pl::BuildItemJson(snap);
                response["playlist"] = pl::BuildPlaylistJson(
                    meta->playlist_id, meta->name, meta->created_at_ms, meta->updated_at_ms,
                    meta->item_count, meta->cover_thumbnails
                );
                request.SetResponseStatus(userver::server::http::HttpStatus::kOk);
                LOG_INFO() << "POST /v1/playlists/" << playlist_id << "/items video_id=" << video_id
                           << " race_lost=1 user_id=" << auth.user_id;
                return userver::formats::json::ToString(response.ExtractValue());
            }
        }

        // ── Insert into playlist_items.
        {
            auto table = session_->GetTable("playlist_items");
            ops::InsertOne ins;
            ins.BindString("playlist_id", playlist_id);
            ins.BindInt64("added_at", now_ms);
            ins.BindString("video_id", video_id);
            ins.BindString("title", snap.title);
            ins.BindString("storage_url", snap.storage_url);
            ins.BindString("thumbnail_url", snap.thumbnail_url);
            ins.BindString("storyboard_url", snap.storyboard_url);
            ins.BindString("mime", snap.mime);
            ins.BindInt64("size_bytes", snap.size_bytes);
            if (snap.duration_ms.has_value()) {
                ins.BindInt64("duration_ms", *snap.duration_ms);
            } else {
                ins.BindNull("duration_ms");
            }
            ins.BindString("author", snap.author);
            ins.BindString("description", snap.description);
            table.Execute(ins);
        }

        // ── Refresh the metadata row: count + cover_thumbnails + updated_at.
        // The new thumbnail (if any) is the most-recent and thus the head of
        // the cover list. Existing covers shift right and are truncated to 4.
        std::vector<std::string> new_covers;
        if (!snap.thumbnail_url.empty()) new_covers.push_back(snap.thumbnail_url);
        for (const auto& u : meta->cover_thumbnails) {
            if (new_covers.size() >= pl::kPreviewThumbs) break;
            if (u.empty()) continue;
            // Avoid duplicating the just-added thumb if it was already there.
            if (std::find(new_covers.begin(), new_covers.end(), u) != new_covers.end()) continue;
            new_covers.push_back(u);
        }

        const int new_count = meta->item_count + 1;
        pc::UpdateCountAndCovers(session_, auth.user_id, playlist_id, new_count, new_covers, now_ms);
        meta->item_count = new_count;
        meta->cover_thumbnails = new_covers;
        meta->updated_at_ms = now_ms;

        userver::formats::json::ValueBuilder response;
        response["item"] = pl::BuildItemJson(snap);
        response["playlist"] = pl::BuildPlaylistJson(
            meta->playlist_id, meta->name, meta->created_at_ms, meta->updated_at_ms, meta->item_count,
            meta->cover_thumbnails
        );
        request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
        LOG_INFO() << "POST /v1/playlists/" << playlist_id << "/items video_id=" << video_id
                   << " user_id=" << auth.user_id << " count=" << new_count;
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "POST /v1/playlists/" << playlist_id << "/items video_id=" << video_id
                    << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::item_add
