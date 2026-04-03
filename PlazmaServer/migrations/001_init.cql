
CREATE KEYSPACE IF NOT EXISTS plazma WITH replication = {
    'class': 'SimpleStrategy',
    'replication_factor': 1
};

CREATE TABLE plazma.videos (
    user_id bigint,
    video_id text,
    title text,
    storage_url text,
    thumbnail_url text,
    created_at timestamp,
    PRIMARY KEY (user_id, video_id)
) WITH CLUSTERING ORDER BY (video_id DESC);


CREATE TABLE IF NOT EXISTS plazma.users_by_phone (
    phone_number text,
    user_id bigint,
    username text,
    PRIMARY KEY (phone_number)
);