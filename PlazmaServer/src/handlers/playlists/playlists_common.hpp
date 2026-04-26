#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/scylla/session.hpp>

namespace real_medium::handlers::playlists::common {

// In-memory snapshot of a playlist metadata row. Mirrors the columns shared by
// `playlists_by_user` and `playlist_by_id` — both tables are kept in sync.
struct PlaylistMeta {
    std::string playlist_id;
    int64_t user_id = 0;
    std::string name;
    int64_t created_at_ms = 0;
    int64_t updated_at_ms = 0;
    int item_count = 0;
    std::vector<std::string> cover_thumbnails;  // raw `s3://…` URLs
};

// Read playlist_by_id by primary key. Returns nullopt when the row is absent.
std::optional<PlaylistMeta> LoadPlaylistById(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& playlist_id
);

// Read every (playlist_id, name, ...) pair owned by user_id. Used by the list
// endpoint and by the create handler's count cap check.
std::vector<PlaylistMeta> LoadAllPlaylistsForUser(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id
);

// Insert a playlist into both metadata tables. Both writes use BindNull when
// the cover_thumbnails vector is empty so the column is left as `null`
// rather than an empty CQL list (Scylla treats them differently in toolings).
void InsertMetadataBoth(const userver::storages::scylla::SessionPtr& session, const PlaylistMeta& meta);

// Update count + covers + updated_at on both metadata tables.
void UpdateCountAndCovers(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id,
    int item_count,
    const std::vector<std::string>& cover_thumbnails,
    int64_t updated_at_ms
);

// Update name + updated_at on both metadata tables. Used by the rename handler.
void UpdateName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id,
    const std::string& name,
    int64_t updated_at_ms
);

// Delete metadata rows from both tables (idempotent).
void DeleteMetadataBoth(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& playlist_id
);

// Read the top-N most-recent non-empty thumbnail URLs from a playlist's items
// partition, capped at `kPreviewThumbs`.
std::vector<std::string> RecomputeCoverThumbnails(
    const userver::storages::scylla::SessionPtr& session,
    const std::string& playlist_id
);

// Insert into playlist_name_by_user via LWT. Returns true if the insert was
// applied (i.e. no existing row for that (user_id, name_lower)). When
// `existing_playlist_id_out` is non-null and the LWT failed, it receives the
// playlist_id of the row that already owns the name.
bool TryReserveName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& name_lower,
    const std::string& playlist_id,
    std::string* existing_playlist_id_out = nullptr
);

// Delete a (user_id, name_lower) entry from the name index. No-op if absent.
void ReleaseName(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& name_lower
);

// Look up `(user_id, video_id) -> playlist_id` rows from the membership index.
struct VideoMembership {
    std::string playlist_id;
    int64_t added_at_ms = 0;
};

std::vector<VideoMembership> LoadVideoMemberships(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& video_id
);

// Read a single playlists_by_video entry; nullopt if absent.
std::optional<VideoMembership> LookupVideoMembership(
    const userver::storages::scylla::SessionPtr& session,
    int64_t user_id,
    const std::string& video_id,
    const std::string& playlist_id
);

}  // namespace real_medium::handlers::playlists::common
