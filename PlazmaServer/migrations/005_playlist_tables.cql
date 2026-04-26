-- Per-user playlists. Five tables, all in keyspace plazma.
--
-- Reads are the hot path:
--   * "list my playlists"          -> playlists_by_user (one partition per user)
--   * "show playlist X"            -> playlist_by_id    (point lookup, hides user_id from URL)
--   * "items in playlist X"        -> playlist_items    (one partition per playlist, ASC by added_at)
--   * "is this video saved by me"  -> playlists_by_video ( (user_id, video_id) -> playlist_ids )
--   * name uniqueness (LWT)        -> playlist_name_by_user (single PK row per user+name_lower)

CREATE TABLE IF NOT EXISTS plazma.playlists_by_user (
    user_id          bigint,
    playlist_id      text,
    name             text,
    created_at       bigint,
    updated_at       bigint,
    item_count       int,
    cover_thumbnails list<text>,
    PRIMARY KEY (user_id, playlist_id)
);

CREATE TABLE IF NOT EXISTS plazma.playlist_by_id (
    playlist_id      text PRIMARY KEY,
    user_id          bigint,
    name             text,
    created_at       bigint,
    updated_at       bigint,
    item_count       int,
    cover_thumbnails list<text>
);

CREATE TABLE IF NOT EXISTS plazma.playlist_items (
    playlist_id    text,
    added_at       bigint,
    video_id       text,
    title          text,
    storage_url    text,
    thumbnail_url  text,
    storyboard_url text,
    mime           text,
    size_bytes     bigint,
    duration_ms    bigint,
    author         text,
    description    text,
    PRIMARY KEY (playlist_id, added_at, video_id)
) WITH CLUSTERING ORDER BY (added_at ASC, video_id ASC);

CREATE TABLE IF NOT EXISTS plazma.playlists_by_video (
    user_id     bigint,
    video_id    text,
    playlist_id text,
    added_at    bigint,
    PRIMARY KEY ((user_id, video_id), playlist_id)
);

CREATE TABLE IF NOT EXISTS plazma.playlist_name_by_user (
    user_id      bigint,
    name_lower   text,
    playlist_id  text,
    PRIMARY KEY ((user_id, name_lower))
);
