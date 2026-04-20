-- Add missing columns to existing videos table
ALTER TABLE plazma.videos ADD mime text;
ALTER TABLE plazma.videos ADD size_bytes bigint;
ALTER TABLE plazma.videos ADD visibility text;
ALTER TABLE plazma.videos ADD created_at_ms bigint;

-- Point lookup by video_id
CREATE TABLE IF NOT EXISTS plazma.video_by_id (
    video_id      text PRIMARY KEY,
    user_id       bigint,
    title         text,
    storage_url   text,
    mime          text,
    size_bytes    bigint,
    duration_ms   bigint,
    thumbnail_url text,
    visibility    text,
    created_at    bigint,
    day           text
);

-- Global feed bucketed by UTC day; created_at DESC gives newest-first per day
CREATE TABLE IF NOT EXISTS plazma.videos_by_day (
    day           text,
    created_at    bigint,
    video_id      text,
    user_id       bigint,
    title         text,
    storage_url   text,
    thumbnail_url text,
    duration_ms   bigint,
    mime          text,
    size_bytes    bigint,
    visibility    text,
    PRIMARY KEY (day, created_at, video_id)
) WITH CLUSTERING ORDER BY (created_at DESC, video_id DESC);

-- View/like counters; counter columns must live in their own table
CREATE TABLE IF NOT EXISTS plazma.video_stats (
    video_id text PRIMARY KEY,
    views    counter,
    likes    counter
);
