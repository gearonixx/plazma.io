#include "playlists_common.hpp"

#include <algorithm>

#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/row.hpp>
#include <userver/storages/scylla/value.hpp>

#include "utils/playlist.hpp"

namespace real_medium::handlers::playlists::common {

namespace ops = userver::storages::scylla::operations;
namespace pl = real_medium::utils::playlist;
using userver::storages::scylla::Value;

namespace {

PlaylistMeta MetaFromRow(const userver::storages::scylla::Row& row) {
    PlaylistMeta m;
    m.playlist_id = row.Get<std::string>("playlist_id");
    m.user_id = row.IsNull("user_id") ? 0LL : row.Get<int64_t>("user_id");
    m.name = row.IsNull("name") ? std::string{} : row.Get<std::string>("name");
    m.created_at_ms = row.IsNull("created_at") ? 0LL : row.Get<int64_t>("created_at");
    m.updated_at_ms = row.IsNull("updated_at") ? 0LL : row.Get<int64_t>("updated_at");
    m.item_count = row.IsNull("item_count") ? 0 : row.Get<int32_t>("item_count");
    m.cover_thumbnails = pl::ReadCoverThumbnails(row);
    return m;
}

}  // namespace

std::optional<PlaylistMeta> LoadPlaylistById(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& playlist_id
) {
    auto table = session->GetTable("playlist_by_id");
    ops::SelectOne sel;
    sel.AddAllColumns();
    sel.WhereString("playlist_id", playlist_id);
    auto row = table.Execute(sel);
    if (row.Empty()) return std::nullopt;
    return MetaFromRow(row);
}

std::vector<PlaylistMeta> LoadAllPlaylistsForUser(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id
) {
    std::vector<PlaylistMeta> out;
    auto table = session->GetTable("playlists_by_user");
    ops::SelectMany sel;
    sel.AddAllColumns();
    sel.WhereInt64("user_id", user_id);
    for (const auto& row : table.Execute(sel)) {
        out.push_back(MetaFromRow(row));
    }
    return out;
}

void InsertMetadataBoth(const userver::storages::scylla::SessionPtr& session, const PlaylistMeta& meta) {
    {
        auto table = session->GetTable("playlist_by_id");
        ops::InsertOne ins;
        ins.BindString("playlist_id", meta.playlist_id);
        ins.BindInt64("user_id", meta.user_id);
        ins.BindString("name", meta.name);
        ins.BindInt64("created_at", meta.created_at_ms);
        ins.BindInt64("updated_at", meta.updated_at_ms);
        ins.BindInt32("item_count", meta.item_count);
        if (meta.cover_thumbnails.empty()) {
            ins.BindNull("cover_thumbnails");
        } else {
            ins.BindList("cover_thumbnails", pl::ToScyllaList(meta.cover_thumbnails));
        }
        table.Execute(ins);
    }
    {
        auto table = session->GetTable("playlists_by_user");
        ops::InsertOne ins;
        ins.BindInt64("user_id", meta.user_id);
        ins.BindString("playlist_id", meta.playlist_id);
        ins.BindString("name", meta.name);
        ins.BindInt64("created_at", meta.created_at_ms);
        ins.BindInt64("updated_at", meta.updated_at_ms);
        ins.BindInt32("item_count", meta.item_count);
        if (meta.cover_thumbnails.empty()) {
            ins.BindNull("cover_thumbnails");
        } else {
            ins.BindList("cover_thumbnails", pl::ToScyllaList(meta.cover_thumbnails));
        }
        table.Execute(ins);
    }
}

void UpdateCountAndCovers(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id,
    int item_count,
    const std::vector<std::string>& cover_thumbnails,
    int64_t updated_at_ms
) {
    {
        auto table = session->GetTable("playlist_by_id");
        ops::UpdateOne upd;
        upd.SetInt32("item_count", item_count);
        upd.SetInt64("updated_at", updated_at_ms);
        if (cover_thumbnails.empty()) {
            upd.SetNull("cover_thumbnails");
        } else {
            upd.SetList("cover_thumbnails", pl::ToScyllaList(cover_thumbnails));
        }
        upd.WhereString("playlist_id", playlist_id);
        table.Execute(upd);
    }
    {
        auto table = session->GetTable("playlists_by_user");
        ops::UpdateOne upd;
        upd.SetInt32("item_count", item_count);
        upd.SetInt64("updated_at", updated_at_ms);
        if (cover_thumbnails.empty()) {
            upd.SetNull("cover_thumbnails");
        } else {
            upd.SetList("cover_thumbnails", pl::ToScyllaList(cover_thumbnails));
        }
        upd.WhereInt64("user_id", user_id);
        upd.WhereString("playlist_id", playlist_id);
        table.Execute(upd);
    }
}

void UpdateName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id,
    const std::string& name,
    int64_t updated_at_ms
) {
    {
        auto table = session->GetTable("playlist_by_id");
        ops::UpdateOne upd;
        upd.SetString("name", name);
        upd.SetInt64("updated_at", updated_at_ms);
        upd.WhereString("playlist_id", playlist_id);
        table.Execute(upd);
    }
    {
        auto table = session->GetTable("playlists_by_user");
        ops::UpdateOne upd;
        upd.SetString("name", name);
        upd.SetInt64("updated_at", updated_at_ms);
        upd.WhereInt64("user_id", user_id);
        upd.WhereString("playlist_id", playlist_id);
        table.Execute(upd);
    }
}

void DeleteMetadataBoth(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id
) {
    {
        auto table = session->GetTable("playlists_by_user");
        ops::DeleteOne del;
        del.WhereInt64("user_id", user_id);
        del.WhereString("playlist_id", playlist_id);
        table.Execute(del);
    }
    {
        auto table = session->GetTable("playlist_by_id");
        ops::DeleteOne del;
        del.WhereString("playlist_id", playlist_id);
        table.Execute(del);
    }
}

std::vector<std::string> RecomputeCoverThumbnails(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& playlist_id
) {
    // CLUSTERING ORDER on the table is (added_at ASC, video_id ASC); a
    // single-partition query may override it to DESC to fetch newest-first
    // without scanning the whole partition. We pull a small overshoot (16) so
    // a few empty thumbnail rows do not force us to fall short of 4 covers.
    std::vector<std::string> out;
    auto rows = session->Execute(
        "SELECT thumbnail_url FROM playlist_items "
        "WHERE playlist_id = ? "
        "ORDER BY added_at DESC, video_id DESC "
        "LIMIT 16",
        std::vector<Value>{Value{playlist_id}}
    );
    for (const auto& row : rows) {
        if (row.IsNull("thumbnail_url")) continue;
        const auto url = row.Get<std::string>("thumbnail_url");
        if (url.empty()) continue;
        out.push_back(url);
        if (out.size() >= pl::kPreviewThumbs) break;
    }
    return out;
}

bool TryReserveName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& name_lower,
    const std::string& playlist_id,
    std::string* existing_playlist_id_out
) {
    auto table = session->GetTable("playlist_name_by_user");
    ops::InsertOne ins;
    ins.BindInt64("user_id", user_id);
    ins.BindString("name_lower", name_lower);
    ins.BindString("playlist_id", playlist_id);
    ins.IfNotExists();
    const auto res = table.ExecuteLwt(ins);
    if (!res.applied && existing_playlist_id_out != nullptr) {
        if (!res.previous.IsNull("playlist_id")) {
            *existing_playlist_id_out = res.previous.Get<std::string>("playlist_id");
        }
    }
    return res.applied;
}

void ReleaseName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& name_lower
) {
    auto table = session->GetTable("playlist_name_by_user");
    ops::DeleteOne del;
    del.WhereInt64("user_id", user_id);
    del.WhereString("name_lower", name_lower);
    table.Execute(del);
}

std::vector<VideoMembership> LoadVideoMemberships(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& video_id
) {
    std::vector<VideoMembership> out;
    auto table = session->GetTable("playlists_by_video");
    ops::SelectMany sel;
    sel.AddAllColumns();
    sel.WhereInt64("user_id", user_id);
    sel.WhereString("video_id", video_id);
    for (const auto& row : table.Execute(sel)) {
        VideoMembership m;
        m.playlist_id = row.Get<std::string>("playlist_id");
        m.added_at_ms = row.IsNull("added_at") ? 0LL : row.Get<int64_t>("added_at");
        out.push_back(std::move(m));
    }
    return out;
}

std::optional<VideoMembership> LookupVideoMembership(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& video_id,
    const std::string& playlist_id
) {
    auto table = session->GetTable("playlists_by_video");
    ops::SelectOne sel;
    sel.AddAllColumns();
    sel.WhereInt64("user_id", user_id);
    sel.WhereString("video_id", video_id);
    sel.WhereString("playlist_id", playlist_id);
    auto row = table.Execute(sel);
    if (row.Empty()) return std::nullopt;
    VideoMembership m;
    m.playlist_id = playlist_id;
    m.added_at_ms = row.IsNull("added_at") ? 0LL : row.Get<int64_t>("added_at");
    return m;
}

}  // namespace real_medium::handlers::playlists::common
