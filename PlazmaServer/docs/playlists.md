# Playlists — full server spec

Authoritative contract for the per-user playlists feature. The frontend has
moved off the local `QSettings` store onto these endpoints
(`Plazma/src/api.cpp :: Api::list/create/rename/delete/addItem/removeItem*`,
`Plazma/src/models/playlists_model.cpp`); this doc is the server-side work
item.

Status: **not yet implemented** on the server. The client uses the same
optimistic-then-reconcile pattern as the rest of the app (see §11), so until
these endpoints exist every mutation will roll back with a banner — that is
the contract for the failure path, not a regression.

The shape of every record, error, and pagination cursor below has already
been frozen client-side; do not deviate without a coordinated client change.

---

## 1. Identity & high-level model

A **playlist** is a named, ordered, mutable collection of `video_id`s, owned
by exactly one user. Every endpoint requires a Bearer JWT and asserts
`playlist.user_id == auth.user_id`; cross-user reads and writes never cross
the wire.

| Concept | Authoritative store | Notes |
| --- | --- | --- |
| Playlist metadata | Scylla `playlists_by_user` + `playlist_by_id` | Two tables, one for owner-listing, one for point lookup. |
| Items inside a playlist | Scylla `playlist_items` | Clustered by `(added_at ASC, video_id ASC)` — insertion order. |
| "Which playlists contain this video for this user" | Scylla `playlists_by_video` | Fast index for the `Save to playlist` dialog and the saved-badge counter on the feed. |
| Cover thumbnails | denormalized `cover_thumbs list<text>` on the metadata row | Up to 4 most-recent thumbnails. Cheap render for the grid; refresh on every add/remove. |

Item ordering inside a playlist is **insertion order, ascending**. We do not
expose a "move up / move down" API in v1; the client never asks for one.

| Attribute | Value |
| --- | --- |
| URL prefix | `/v1/playlists` |
| Auth | Bearer required on every call (`401` on anonymous) |
| Content-Type (response) | `application/json; charset=utf-8` |
| Idempotency keys | Yes for create/add — see §10. |

---

## 2. Resource shapes

### 2.1 `Playlist` summary (used in list responses and as the element of `playlists[]`)

```json
{
  "id":              "01HX9YJ4S6Q8M5Q3R7N2K2Q5T1",
  "name":            "Studying jams",
  "created_at":      "2026-04-26T10:11:12Z",
  "updated_at":      "2026-04-26T18:42:01Z",
  "video_count":     14,
  "cover_thumbnails": [
    "http://localhost:9000/plazma-videos/videos/.../thumb.jpg",
    "http://localhost:9000/plazma-videos/videos/.../thumb.jpg"
  ]
}
```

### 2.2 `Playlist` detail (used in single-fetch + first page of items)

Same as 2.1 plus:

```json
{
  "items":       [ <PlaylistItem>, ... ],
  "next_cursor": "eyJrIjoi…" | null,
  "total":       14
}
```

### 2.3 `PlaylistItem`

The item row carries enough denormalized video data that the playlist
detail page can render rows without a second `GET /v1/videos/{id}` per item
(latency budget is one request per page, not N+1).

```json
{
  "video_id":       "01HX...",
  "title":          "Lo-fi study mix",
  "url":            "http://localhost:9000/plazma-videos/videos/.../playlist.m3u8",
  "thumbnail":      "http://localhost:9000/plazma-videos/videos/.../thumb.jpg",
  "storyboard":     "http://localhost:9000/plazma-videos/videos/.../storyboard.jpg",
  "mime":           "video/mp4",
  "size":           184203213,
  "duration_ms":    91230,
  "author":         "alex",
  "description":    "",
  "added_at":       "2026-04-26T10:11:12Z"
}
```

Field rules:

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `video_id` | string | yes | The canonical `videos.video_id`. Used as primary identity in client model. |
| `title` | string | yes | May be empty string; never `null`. Server snapshots at add time, then refreshes lazily — see §6. |
| `url` | string | yes | Absolute HTTP URL for playback (S3 → HTTP rewrite). |
| `thumbnail` | string | yes | Absolute URL or empty string. Never `null` (the QML role binds directly). |
| `storyboard` | string | yes | Same. Empty string when not yet generated. |
| `mime` | string | yes | e.g. `video/mp4`. Empty string if unknown. |
| `size` | int64 | yes | Bytes. `0` if unknown. |
| `duration_ms` | int64 \| null | yes | `null` when unprobed. |
| `author` | string | yes | Display username; empty if unknown. |
| `description` | string | yes | Short description, may be empty. |
| `added_at` | ISO-8601 string | yes | UTC, seconds precision, `Z` suffix. The server-assigned timestamp at add time — clients **must** treat the server's value as authoritative even if the client locally mutated `added_at` while the request was in flight. |

Do not emit `null` for any string field — the client's QML bindings expect a
defined string. Use empty string instead.

---

## 3. Endpoints

| # | Method & path | Summary |
| --- | --- | --- |
| 3.1 | `GET    /v1/playlists`                                | List the caller's playlists |
| 3.2 | `POST   /v1/playlists`                                | Create a playlist |
| 3.3 | `GET    /v1/playlists/{id}`                           | Fetch one playlist + first page of items |
| 3.4 | `PATCH  /v1/playlists/{id}`                           | Rename |
| 3.5 | `DELETE /v1/playlists/{id}`                           | Delete |
| 3.6 | `GET    /v1/playlists/{id}/items?cursor=…&limit=…`    | Paginated item list (only used when count > one page) |
| 3.7 | `POST   /v1/playlists/{id}/items`                     | Add an item |
| 3.8 | `DELETE /v1/playlists/{id}/items/{video_id}`          | Remove an item |
| 3.9 | `GET    /v1/users/me/playlists/by_video/{video_id}`   | List playlist ids of mine that contain `video_id` |

### 3.1 `GET /v1/playlists`

Request: `GET /v1/playlists?limit=50&sort=name`

| Param | Type | Default | Notes |
| --- | --- | --- | --- |
| `limit` | int | 100 | Clamp to `[1, 200]`. |
| `sort` | enum | `name` | `name` (case-insensitive lex) or `recent` (`updated_at DESC`). |

Response `200`:

```json
{
  "playlists":   [ <Playlist summary>, ... ],
  "next_cursor": null
}
```

`next_cursor` is reserved for the future; v1 always returns the full list
since the realistic playlist count per user is < 200. If a user crosses
that, return up to `limit` and a real cursor — the client will fall back
to "list what you got, fetch more on demand".

### 3.2 `POST /v1/playlists`

Body:

```json
{
  "id":   "01HX9YJ4S6Q8M5Q3R7N2K2Q5T1",
  "name": "Studying jams"
}
```

The client supplies the id as a UUID v7 string. This lets the client retain
its **synchronous** `createPlaylist(name) → id` contract: it generates the
id locally, returns it to the caller for chaining, and fires this request
in the background. See §10 for the idempotency rules that make this safe.

If the body omits `id`, the server generates one (UUID v7).

Validation:

- `name` trimmed; reject `400 {"error":"name is required"}` if empty after trim.
- `name` length ≤ 100 chars; otherwise `400 {"error":"name too long"}`.
- `id` (when supplied) must match `^[0-9A-HJKMNP-TV-Z]{26}$` (Crockford
  base32 ULID/UUIDv7) **or** `^[0-9a-fA-F-]{36}$` (RFC 4122 hyphenated UUID).
  Otherwise `400 {"error":"invalid id"}`.
- Name uniqueness is per-user, **case-insensitive**. Conflict → `409
  {"error":"name already exists"}`. The client matches this — its dialog
  surfaces the message verbatim.

Success: `201 Created`, body `{ "playlist": <Playlist summary> }`.

### 3.3 `GET /v1/playlists/{id}`

Returns the metadata + the first page of items in one round trip.

| Param | Type | Default | Notes |
| --- | --- | --- | --- |
| `limit` | int | 50 | Clamp to `[1, 100]`. |

Response `200`:

```json
{
  "playlist":    <Playlist summary>,
  "items":       [ <PlaylistItem>, ... ],
  "next_cursor": "eyJrIjoi…" | null,
  "total":       <int>
}
```

`404 {"error":"playlist not found"}` if no row, **or** if it exists but
`user_id != auth.user_id` (do not leak existence).

### 3.4 `PATCH /v1/playlists/{id}`

Body: `{ "name": "<new name>" }`. Same validation + 409-on-collision rules
as 3.2. Response `200 {"playlist": <Playlist summary>}`.

### 3.5 `DELETE /v1/playlists/{id}`

Response `204 No Content`. Idempotent — deleting a non-existent (or
already-deleted) playlist returns `204`, not `404`. (Rationale: the client
fires this optimistically; the second delete from a duplicate-tap should
succeed silently.)

Backend must remove rows from all four tables (§5).

### 3.6 `GET /v1/playlists/{id}/items`

Cursor-paginated. The cursor is the same opaque `base64url(json)` shape as
`/v1/videos` (see `video_search.md` §5), but the sort key is fixed to
`(added_at ASC, video_id ASC)` so cursor binding is just `(playlist_id)`.

| Param | Type | Default | Notes |
| --- | --- | --- | --- |
| `limit` | int | 50 | Clamp to `[1, 100]`. |
| `cursor` | opaque | — | Returned by a previous response. |

Response `200`:

```json
{
  "items":       [ <PlaylistItem>, ... ],
  "next_cursor": "eyJrIjoi…" | null
}
```

### 3.7 `POST /v1/playlists/{id}/items`

Body — minimum required:

```json
{
  "video_id": "01HX..."
}
```

Optional client-supplied snapshot (avoids a round trip on the server when the
client already has the data):

```json
{
  "video_id":     "01HX...",
  "title":        "Lo-fi study mix",
  "url":          "...",
  "thumbnail":    "...",
  "storyboard":   "...",
  "mime":         "video/mp4",
  "size":         184203213,
  "duration_ms":  91230,
  "author":       "alex",
  "description":  ""
}
```

Server behavior:

1. If the client snapshot is present and self-consistent, store it as the
   denormalized item row. The server still verifies the video exists in
   `video_by_id` and overwrites any field where the canonical value
   disagrees (canonical wins; client snapshot is a hint).
2. If the client snapshot is absent, fetch the row from `video_by_id` and
   write its fields.
3. If `video_by_id` has no such row, return `404 {"error":"video not
   found"}` — do not store an item that points at nothing.
4. Stamp `added_at = NowMs()` on the server side; do not trust a client
   value here. Clients display the server-stamped value after reconcile.

Idempotency: re-adding the same `video_id` to the same playlist is a no-op
that returns the existing row with status `200 OK` (not `201`) and the
**original** `added_at`. The client uses this to detect "already in this
playlist" and surface the toast — see §10.

Success: `201 Created` for a fresh add, `200 OK` for a re-add. Body in
both cases:

```json
{ "item": <PlaylistItem>, "playlist": <Playlist summary> }
```

The `playlist` summary lets the client refresh `video_count` and
`cover_thumbnails` from a single response — no follow-up `GET` needed.

### 3.8 `DELETE /v1/playlists/{id}/items/{video_id}`

Response `204 No Content`. Idempotent — `204` whether the item existed or
not. As a side effect, refresh `cover_thumbnails` and decrement
`video_count` on the playlist row(s).

The endpoint does **not** return the updated playlist body (the client has
all it needs to compute the new counts locally and reconciles on next
list).

### 3.9 `GET /v1/users/me/playlists/by_video/{video_id}`

For the saved-state badge on the feed and the `summariesForVideo` lookup.

Response `200`:

```json
{
  "playlist_ids": ["01HX...", "01HX..."]
}
```

Always returns an empty array (not `404`) when the video isn't saved
anywhere. Cheap query backed by `playlists_by_video`.

---

## 4. Authentication & authorization

- Bearer JWT in `Authorization: Bearer <token>`.
- Missing or anonymous → `401 {"error":"authentication required"}`. There
  is no public/anonymous read of any playlist endpoint.
- Token present but invalid/expired → `401 {"error":"invalid or expired
  token"}` (matches `utils::ExtractAuth` return shape from the existing
  handlers).
- For every `{id}` route, after JWT check, fetch the playlist row and
  assert `playlist.user_id == auth.user_id`. Mismatch returns `404` (not
  `403`) so the existence of someone else's playlist isn't leaked.

---

## 5. Storage schema (Scylla)

Add as `migrations/005_playlist_tables.cql`. All tables in keyspace
`plazma`.

```cql
-- Owner-side index: list a user's playlists.
-- Two clustering choices needed (name and updated_at), so we keep
-- updated_at as the clustering key for "recent first" reads, then
-- sort by name in-memory after fetch (always small N — playlists per user).
CREATE TABLE IF NOT EXISTS plazma.playlists_by_user (
    user_id          bigint,
    playlist_id      text,
    name             text,
    created_at       bigint,    -- ms since epoch
    updated_at       bigint,    -- ms since epoch
    item_count       int,
    cover_thumbnails list<text>,  -- 0..4 entries, ordered newest-first
    PRIMARY KEY (user_id, playlist_id)
);

-- Point lookup by playlist_id. Used by /v1/playlists/{id} routes so we
-- don't need user_id from the path.
CREATE TABLE IF NOT EXISTS plazma.playlist_by_id (
    playlist_id      text PRIMARY KEY,
    user_id          bigint,
    name             text,
    created_at       bigint,
    updated_at       bigint,
    item_count       int,
    cover_thumbnails list<text>
);

-- Items in a playlist, ordered by insertion time.
-- The composite clustering ensures stable order even if two adds collide
-- on the same millisecond (tie-broken by video_id).
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

-- Membership index keyed by (user_id, video_id) → which of MY playlists
-- contain this video. Fast lookup for the "saved" badge and the
-- Save-to-playlist dialog. Writes go alongside playlist_items writes.
CREATE TABLE IF NOT EXISTS plazma.playlists_by_video (
    user_id     bigint,
    video_id    text,
    playlist_id text,
    added_at    bigint,
    PRIMARY KEY ((user_id, video_id), playlist_id)
);

-- Case-insensitive name uniqueness index. The hash key is the lowercased
-- name; lookup with LIMIT 1 to detect a collision before insert/rename.
-- We keep this as a separate small table rather than a SAI to stay on the
-- minimum-feature subset of Scylla open-source.
CREATE TABLE IF NOT EXISTS plazma.playlist_name_by_user (
    user_id      bigint,
    name_lower   text,
    playlist_id  text,
    PRIMARY KEY ((user_id, name_lower))
);
```

### 5.1 Why these tables and not, say, one collection per user?

- Reads are the hot path: "list my playlists", "list items in this
  playlist", "is this video saved". Each has a dedicated partition key →
  one-partition reads, no scatter-gather.
- Counters use plain `int` (`item_count`) and are recomputed on every
  add/remove rather than using a Scylla `counter` column. Counters and
  list updates can't co-exist on the same row, and we want
  `cover_thumbnails` (a list) on the same row as the count — so the
  application maintains the count consistently across both metadata
  tables on each write.
- `cover_thumbnails` is denormalized as a `list<text>` of up to 4 URLs.
  On every add, take the new item's `thumbnail_url` and prepend it,
  truncate to 4. On every remove, recompute by reading the first 4
  non-empty `thumbnail_url` from `playlist_items` for that
  `playlist_id`. (Cheap: bounded read of 4 rows.)

### 5.2 Write protocol

Adding an item (`POST /v1/playlists/{id}/items`):

1. Read `playlist_by_id` for ownership check.
2. Verify `video_by_id` exists for `video_id`.
3. Read existing `playlist_items` for `(playlist_id, video_id)` —
   technically a partition + clustering range scan filtered to the
   `video_id` clustering tail; in practice we maintain the membership
   index and check `playlists_by_video` first. If hit, return 200 with
   the existing row.
4. Insert into `playlist_items` and `playlists_by_video`.
5. Read up to 4 latest `thumbnail_url` entries from `playlist_items` for
   the playlist; recompute `cover_thumbnails`.
6. Update `playlists_by_user` and `playlist_by_id` with new
   `item_count`, `cover_thumbnails`, `updated_at`.

Steps 4–6 are not transactional in Scylla. The application accepts the
denormalization windows; if a crash leaves `cover_thumbnails` stale, the
next mutation refreshes it. The client's reconcile path (§11) eventually
re-syncs to truth.

Removing an item is the mirror: delete from `playlist_items` +
`playlists_by_video`, recompute `cover_thumbnails`, update count + ts.

Renaming:

1. Read existing row for current name.
2. `INSERT IF NOT EXISTS` into `playlist_name_by_user` for the new
   `(user_id, name_lower)` — Scylla LWT (Paxos), the only place we use
   it. Skip the LWT write if `name_lower` is unchanged (case-only edit).
3. On success, `DELETE` the old `name_lower` entry, `UPDATE` `name` on
   both metadata tables, bump `updated_at`.
4. On LWT failure (conflict) → 409.

Deleting a playlist: clean up rows in this order so a partial failure
leaves the user's view consistent ("disappeared" beats "ghost"):

1. `playlists_by_user` (so it stops showing up in lists).
2. `playlist_by_id` (so direct lookups 404).
3. `playlist_name_by_user`.
4. `playlists_by_video` for every member (range read of `playlist_items`
   first, then per-row delete).
5. `playlist_items` (drop the entire partition: `DELETE … WHERE
   playlist_id = ?`).

---

## 6. Item field freshness

`playlist_items` is denormalized — title, thumbnail, storyboard, etc. are
copies of the canonical `video_by_id` row at the time of add. Two
mechanisms keep them fresh:

1. **Write-time refresh**: when the server processes any
   `video_by_id.thumbnail_url` / `storyboard_url` / `title` update (the
   ffmpeg-derived async path in `video_create.cpp`'s
   `ScheduleMediaDerivatives`), enqueue a fanout to all
   `playlist_items` rows that reference that `video_id` so the cover
   mosaic and item-row rendering reflect reality. Background only — never
   blocks the upload.
2. **Read-time TTL** (optional, v1.1): on `GET /v1/playlists/{id}` or
   `GET /v1/playlists/{id}/items`, if any row's `updated_at` is older
   than 24 h, refresh that row's denormalized fields from
   `video_by_id` before serializing. Bounded read, lazy, eventual.

Mechanism 1 alone is sufficient for v1; mechanism 2 is a backstop if (1)
is ever skipped (e.g. video edited via a tool that bypassed the
fanout-emit code path).

---

## 7. Error responses

All errors are `{"error":"<message>"}` with the statuses below.

| Status | Condition |
| --- | --- |
| `400` | Malformed parameter / body / id. |
| `401` | Missing or invalid bearer token. |
| `404` | Playlist not found, or owned by a different user, or `video_id` not found on add. |
| `409` | Name collision on create or rename. |
| `413` | Item count would exceed `5 000` per playlist (cap below). |
| `422` | Body parse error (JSON syntax / wrong type). Distinct from `400` because `400` should be reserved for "valid JSON, business rules failed". |
| `429` | Rate limit (§9). |
| `500` | Internal error. |

The client surfaces the `error` string verbatim in the inline error rail
under the box, so prefer human-readable messages: "name already exists"
beats "PLAYLIST_NAME_CONFLICT".

---

## 8. Limits & invariants

| Invariant | Limit | Behavior on breach |
| --- | --- | --- |
| Playlists per user | 200 | `409 {"error":"playlist limit reached"}` on create. |
| Items per playlist | 5 000 | `413 {"error":"playlist is full"}` on add. |
| Name length | 100 chars | `400`. |
| Name uniqueness per user | case-insensitive | `409`. |
| Body size | 16 KiB | `413` at the userver layer. |

---

## 9. Rate limits & caching

- **Mutations**: 60 rpm per `user_id` aggregated across all playlist
  endpoints. Burst 30. Over → `429 Retry-After: <seconds>`.
- **Reads**: 600 rpm per `user_id`. (Higher because the client can issue
  back-to-back `summariesForVideo` lookups when the user is browsing the
  feed.)
- `GET /v1/playlists` and `GET /v1/playlists/{id}` set
  `Cache-Control: private, max-age=30` and `Vary: Authorization`. Item
  pages set `Cache-Control: private, max-age=10` (busier, more likely to
  go stale).

---

## 10. Idempotency

The client uses optimistic UI: it applies a mutation locally and fires the
request in the background, rolling back if the request fails. To keep that
safe under double-submits, network retries, and "back from offline":

- **Create** is idempotent on the client-supplied `id`. If the server
  already has a row with that `(user_id, id)` and the same `name`, return
  `200 OK` with the existing row (not `409` and not `201`). If a row
  exists with a different `name` for the same `id`, return `409
  {"error":"id collision"}` — that should never happen with UUID v7 and
  is purely a defensive check.
- **Add item** is idempotent on `(playlist_id, video_id)` — see §3.7.
- **Delete** (playlist or item) is always idempotent — no-op returns
  `204` on missing row.
- **Rename** is idempotent only when the new name equals the current
  name — return `200 OK` with the current row.

Clients **may** include an `Idempotency-Key: <uuid>` header on POST
requests. v1 server MAY ignore it (the resource-level idempotency above
is sufficient); v1.1 may add a 5-minute replay cache keyed by
`(user_id, Idempotency-Key)` for hard guarantees against spurious
duplicates from broken networks.

---

## 11. Client coordination

The client's `PlaylistsModel` keeps a local mirror and applies mutations
optimistically:

1. Generate a UUID v7 client-side for create.
2. Mutate the local cache, emit the relevant Qt model signals so the UI
   updates immediately.
3. Fire the API request.
4. On `2xx`, reconcile fields from the response (server-canonical
   `added_at`, refreshed `cover_thumbnails`, etc.).
5. On `4xx/5xx`, roll back the local mutation and emit a
   `notify(QString)` toast carrying the `error` message.

Server implementations should expect:

- The same `id` arriving twice in quick succession (same `POST
  /v1/playlists` with the client UUID, retried on transient network
  failure). Idempotency on `id` (§10) makes this safe.
- "Add then remove then add again" within a few hundred ms — the user
  toggling the saved state in the dialog. Each round-trip must be
  individually idempotent; the server does not need to coalesce them.

The client also issues `GET /v1/playlists` on app start and on user-
initiated refresh, and reconciles its cache to whatever the server
returned. Anything not in the server response is dropped from the local
cache (so a delete on another device is reflected next launch).

---

## 12. Observability

Per-request structured log fields (in addition to standard `request_id`,
`user_id`, `path`, `status`):

- `playlist_id` (when the path has one).
- `op = list|create|rename|delete|get|add_item|remove_item|by_video`.
- `latency_ms` total handler time.
- `db_calls` count of Scylla operations executed for this request.

Metrics (Prometheus):

- `playlists.requests_total{op, status}`.
- `playlists.latency_ms{op}` histogram.
- `playlists.cache_size_per_user` gauge (median + p99).
- `playlists.collisions_total{kind=name|id}` counter.

---

## 13. Test matrix

Acceptance tests required before shipping:

1. `GET /v1/playlists` for a fresh user → `{"playlists": [], "next_cursor": null}`.
2. `POST /v1/playlists {id, name}` → `201`. Re-POST same body → `200`,
   same body. Re-POST same `id` with different `name` → `409`.
3. Cross-user `GET /v1/playlists/{id}` for someone else's id → `404`
   (not `403`). Verify body matches "playlist not found" verbatim.
4. Rename to a name already used by the **same user** → `409`.
   Rename to a name used by a **different user** → `200` (uniqueness is
   per-user).
5. Rename to current case-only variant ("Mix" → "MIX") of own name →
   `200` (no-op rather than self-collision).
6. Add same `(playlist, video)` twice → first `201`, second `200`,
   `added_at` unchanged.
7. Add to non-existent video → `404 video not found`.
8. Remove non-existent item → `204`.
9. Cover thumbnails: add 5 items, each with a unique thumbnail →
   `cover_thumbnails` reflects the **most recent four**. Remove the
   newest → cover refreshes to the next four.
10. Concurrent `add` of the same video from two clients → exactly one
    row in `playlist_items`, both clients see the same `added_at`.
11. Delete playlist → subsequent `GET /v1/playlists/{id}` is `404`,
    `playlists_by_video` is purged, `playlist_items` partition is empty.
12. `by_video/{vid}` for a vid in 0/1/many of the user's playlists
    returns the right ids; for a vid in **another user's** playlist,
    returns `[]`.
13. Body > 16 KiB → `413` from userver layer (no handler dispatch).
14. Token missing on every endpoint → `401`. Token forged → `401`.
15. Item count cap: add 5 001st item → `413 playlist is full`.

---

## 14. Rollout plan

1. Land migration `005_playlist_tables.cql`. Apply to staging, run a
   schema check (`DESCRIBE TABLE plazma.playlists_by_user` etc.) against
   the live cluster.
2. Implement handlers behind a feature flag
   (`playlists.api.enabled`) defaulted off. Deploy. With the flag off
   every endpoint returns `503 {"error":"playlists api disabled"}` —
   the client falls back to its local-only cache (the bootstrap state
   on first ever use), which keeps the UI working.
3. Flip the flag on for staging. Run the test matrix (§13).
4. Flip on for 10% of production. Watch
   `playlists.errors_total{kind=db}` and `playlists.latency_ms`.
5. Ramp to 100%. Once at 100% for 14 days with no rollback, delete the
   client's `QSettings` migration code path (ship a one-shot import:
   "we found a local playlists blob, push it to the server, then drop
   the blob").

---

## 15. Wire format compatibility checklist

Before merging server changes that touch playlist serialization, run
through this checklist against the client code:

- [ ] `Plazma/src/api.cpp :: Api::listPlaylists` parses
  `playlists[].{id,name,created_at,updated_at,video_count,cover_thumbnails}`
  with `validators::extract*`. New required fields break validation.
- [ ] `PlaylistsModel::reconcileFromServer` clears any local entry not
  in the response — server omitting a field that was previously
  present silently zeroes it on the client.
- [ ] Item rows: `video_id` (not `id`), `added_at` (not `created_at`),
  and the rest match the QML role names in `playlists_model.h`.
- [ ] `error` string is rendered verbatim in
  `Plazma/src/ui/Boxes/SettingsBox.qml`-style dialogs — keep them
  short and human readable.

If any item is unchecked, don't ship the server change.
