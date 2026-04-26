#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/scylla/row.hpp>
#include <userver/storages/scylla/value.hpp>

#include "video.hpp"

namespace real_medium::utils::playlist {

// ── Limits & invariants (spec §8) ─────────────────────────────────────────────

inline constexpr std::size_t kMaxNameLen = 100;
inline constexpr int kMaxPlaylistsPerUser = 200;
inline constexpr int kMaxItemsPerPlaylist = 5000;
inline constexpr std::size_t kPreviewThumbs = 4;

inline constexpr int kDefaultListLimit = 100;
inline constexpr int kMaxListLimit = 200;
inline constexpr int kDefaultItemsLimit = 50;
inline constexpr int kMaxItemsLimit = 100;
inline constexpr int kDetailFirstPageDefault = 50;
inline constexpr int kDetailFirstPageMax = 100;

// ── ID validation (spec §3.2 — accepts ULID/UUIDv7 26-char OR RFC 4122 36-char)

inline bool IsCrockford(char c) {
    if (c >= '0' && c <= '9') return true;
    // Crockford alphabet: A-Z minus I, L, O, U
    if (c >= 'A' && c <= 'Z') return c != 'I' && c != 'L' && c != 'O' && c != 'U';
    if (c >= 'a' && c <= 'z') return c != 'i' && c != 'l' && c != 'o' && c != 'u';
    return false;
}

inline bool IsHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// 26-char Crockford base32 (ULID / canonical UUIDv7) OR 36-char RFC 4122 hyphenated UUID.
inline bool IsValidId(std::string_view id) {
    if (id.size() == 26) {
        for (char c : id) {
            if (!IsCrockford(c)) return false;
        }
        return true;
    }
    if (id.size() == 36) {
        for (std::size_t i = 0; i < id.size(); ++i) {
            const bool is_dash_pos = (i == 8 || i == 13 || i == 18 || i == 23);
            if (is_dash_pos) {
                if (id[i] != '-') return false;
            } else {
                if (!IsHex(id[i])) return false;
            }
        }
        return true;
    }
    return false;
}

// ── Name handling ─────────────────────────────────────────────────────────────

// Trim ASCII whitespace/control chars from both ends.
inline std::string TrimName(const std::string& in) {
    std::size_t s = 0, e = in.size();
    while (s < e && static_cast<unsigned char>(in[s]) <= 0x20) ++s;
    while (e > s && static_cast<unsigned char>(in[e - 1]) <= 0x20) --e;
    return in.substr(s, e - s);
}

// Case-fold for uniqueness. Implementation in playlist.cpp uses ICU full
// case folding so non-ASCII names ("Música" / "MÚSICA") collide correctly.
std::string CaseFold(const std::string& in);

// ── JSON builders ─────────────────────────────────────────────────────────────

// Build the cover_thumbnails JSON array. Empty entries are filtered out and the
// list is truncated to the spec's max of 4.
inline userver::formats::json::Value BuildCoverThumbnailsJson(const std::vector<std::string>& urls) {
    userver::formats::json::ValueBuilder arr{userver::formats::common::Type::kArray};
    std::size_t emitted = 0;
    for (const auto& u : urls) {
        if (u.empty()) continue;
        arr.PushBack(real_medium::utils::video::StorageUrlToHttp(u));
        if (++emitted >= kPreviewThumbs) break;
    }
    return arr.ExtractValue();
}

// Read cover_thumbnails (list<text>) from a Scylla row, falling back to empty.
inline std::vector<std::string> ReadCoverThumbnails(
    const userver::storages::scylla::Row& row,
    std::string_view col = "cover_thumbnails"
) {
    std::vector<std::string> out;
    if (row.IsNull(col)) return out;
    const auto opt = row.TryGet<userver::storages::scylla::List>(col);
    if (!opt.has_value()) return out;
    out.reserve(opt->items.size());
    for (const auto& v : opt->items) {
        if (!v.IsNull() && v.Is<std::string>()) {
            out.push_back(v.Get<std::string>());
        }
    }
    return out;
}

// Convert a vector of URLs into a Scylla List value for binding.
inline userver::storages::scylla::List ToScyllaList(const std::vector<std::string>& urls) {
    userver::storages::scylla::List out;
    out.items.reserve(urls.size());
    for (const auto& u : urls) {
        out.items.emplace_back(u);
    }
    return out;
}

// Build the canonical Playlist summary JSON (spec §2.1).
inline userver::formats::json::Value BuildPlaylistJson(
    const std::string& id,
    const std::string& name,
    int64_t created_at_ms,
    int64_t updated_at_ms,
    int video_count,
    const std::vector<std::string>& cover_thumbnails
) {
    userver::formats::json::ValueBuilder vb;
    vb["id"] = id;
    vb["name"] = name;
    vb["created_at"] = real_medium::utils::video::FormatTimestampMs(created_at_ms);
    vb["updated_at"] = real_medium::utils::video::FormatTimestampMs(updated_at_ms);
    vb["video_count"] = video_count;
    vb["cover_thumbnails"] = BuildCoverThumbnailsJson(cover_thumbnails);
    return vb.ExtractValue();
}

// Snapshot of canonical video fields used in PlaylistItem rows. Mirrors
// playlist_items columns one-to-one.
struct ItemSnapshot {
    std::string video_id;
    std::string title;
    std::string storage_url;     // raw `s3://...` form; converted to HTTP on serialize
    std::string thumbnail_url;   // raw `s3://...` form
    std::string storyboard_url;  // raw `s3://...` form
    std::string mime;
    int64_t size_bytes = 0;
    std::optional<int64_t> duration_ms;
    std::string author;
    std::string description;
    int64_t added_at_ms = 0;
};

// Build the canonical PlaylistItem JSON (spec §2.3). String fields are emitted
// as empty strings (never null); duration_ms is an int64 or null.
inline userver::formats::json::Value BuildItemJson(const ItemSnapshot& s) {
    userver::formats::json::ValueBuilder vb;
    vb["video_id"] = s.video_id;
    vb["title"] = s.title;
    vb["url"] = s.storage_url.empty() ? std::string{} : real_medium::utils::video::StorageUrlToHttp(s.storage_url);
    vb["thumbnail"] =
        s.thumbnail_url.empty() ? std::string{} : real_medium::utils::video::StorageUrlToHttp(s.thumbnail_url);
    vb["storyboard"] =
        s.storyboard_url.empty() ? std::string{} : real_medium::utils::video::StorageUrlToHttp(s.storyboard_url);
    vb["mime"] = s.mime;
    vb["size"] = s.size_bytes;
    if (s.duration_ms.has_value()) {
        vb["duration_ms"] = *s.duration_ms;
    } else {
        vb["duration_ms"] = userver::formats::json::ValueBuilder{userver::formats::common::Type::kNull}.ExtractValue();
    }
    vb["author"] = s.author;
    vb["description"] = s.description;
    vb["added_at"] = real_medium::utils::video::FormatTimestampMs(s.added_at_ms);
    return vb.ExtractValue();
}

// Read a playlist_items row into a snapshot. Tolerant of NULL columns — string
// fields become empty, size_bytes becomes 0, duration_ms becomes nullopt.
inline ItemSnapshot ItemFromRow(const userver::storages::scylla::Row& row) {
    ItemSnapshot s;
    s.video_id = row.Get<std::string>("video_id");
    s.title = row.IsNull("title") ? std::string{} : row.Get<std::string>("title");
    s.storage_url = row.IsNull("storage_url") ? std::string{} : row.Get<std::string>("storage_url");
    s.thumbnail_url = row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url");
    s.storyboard_url = row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url");
    s.mime = row.IsNull("mime") ? std::string{} : row.Get<std::string>("mime");
    s.size_bytes = row.IsNull("size_bytes") ? 0LL : row.Get<int64_t>("size_bytes");
    if (!row.IsNull("duration_ms")) s.duration_ms = row.Get<int64_t>("duration_ms");
    s.author = row.IsNull("author") ? std::string{} : row.Get<std::string>("author");
    s.description = row.IsNull("description") ? std::string{} : row.Get<std::string>("description");
    s.added_at_ms = row.IsNull("added_at") ? 0LL : row.Get<int64_t>("added_at");
    return s;
}

// ── Cursor (opaque base64url JSON) ────────────────────────────────────────────
//
// items list cursor binds to a single playlist, so the only thing we need to
// preserve between pages is the (added_at, video_id) tuple of the last row
// that was emitted. The next page selects rows whose composite is strictly
// greater than that tuple.

struct ItemCursor {
    int64_t added_at_ms = 0;
    std::string video_id;
};

std::string EncodeItemCursor(const ItemCursor& c);
std::optional<ItemCursor> DecodeItemCursor(const std::string& token);

// ── JSON helpers ──────────────────────────────────────────────────────────────

inline userver::formats::json::Value NullJson() {
    return userver::formats::json::ValueBuilder{userver::formats::common::Type::kNull}.ExtractValue();
}

}  // namespace real_medium::utils::playlist
