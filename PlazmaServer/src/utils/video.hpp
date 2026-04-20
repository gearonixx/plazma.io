#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace real_medium::utils::video {

// "s3://plazma-videos/videos/..." → "http://localhost:9000/plazma-videos/videos/..."
inline std::string StorageUrlToHttp(const std::string& url) {
    constexpr std::string_view kS3 = "s3://";
    // Use compare() which is safe when url is shorter than the prefix
    if (url.size() >= kS3.size()
        && url.compare(0, kS3.size(), kS3.data(), kS3.size()) == 0) {
        return "http://localhost:9000/" + url.substr(kS3.size());
    }
    return url;
}

// "s3://plazma-videos/videos/..." → "videos/..."
inline std::string S3KeyFromUrl(const std::string& url) {
    constexpr std::string_view kPrefix = "s3://plazma-videos/";
    if (url.size() >= kPrefix.size()
        && url.compare(0, kPrefix.size(), kPrefix.data(), kPrefix.size()) == 0) {
        return url.substr(kPrefix.size());
    }
    return url;
}

// Milliseconds since epoch → ISO8601 UTC "YYYY-MM-DDTHH:MM:SSZ"
inline std::string FormatTimestampMs(int64_t ms) {
    std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm tm{};
    gmtime_r(&sec, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Milliseconds since epoch → UTC date "YYYY-MM-DD"
inline std::string DayString(int64_t ms) {
    std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm tm{};
    gmtime_r(&sec, &tm);
    char buf[12];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

inline int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Infer MIME type from file extension (lowercase).
inline std::string MimeFromFilename(const std::string& filename) {
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot + 1 >= filename.size()) return {};
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == "mp4" || ext == "m4v") return "video/mp4";
    if (ext == "webm")                return "video/webm";
    if (ext == "mkv")                 return "video/x-matroska";
    if (ext == "mov")                 return "video/quicktime";
    if (ext == "avi")                 return "video/x-msvideo";
    if (ext == "ogv")                 return "video/ogg";
    return {};
}

// Whitelist of accepted video MIME types.
inline bool IsAllowedMime(const std::string& mime) {
    return mime == "video/mp4"
        || mime == "video/webm"
        || mime == "video/x-matroska"
        || mime == "video/quicktime"
        || mime == "video/x-msvideo"
        || mime == "video/ogg";
}

// Replace characters outside [A-Za-z0-9._-] with underscores.
inline std::string SanitizeFilename(const std::string& name) {
    std::string out = name;
    for (char& c : out) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                     || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) c = '_';
    }
    return out;
}

// Trim leading/trailing whitespace and control characters from a title string.
inline std::string NormalizeTitle(std::string s, size_t max_len = 200) {
    // Strip leading whitespace/control chars
    size_t start = 0;
    while (start < s.size()
           && (static_cast<unsigned char>(s[start]) <= 0x20)) ++start;
    // Strip trailing
    size_t end = s.size();
    while (end > start
           && (static_cast<unsigned char>(s[end - 1]) <= 0x20)) --end;
    s = s.substr(start, end - start);
    if (s.size() > max_len) s.resize(max_len);
    return s;
}

// Build the canonical video JSON object returned by every endpoint.
// `storyboard_url` is optional; pass empty string if no storyboard is available.
inline userver::formats::json::Value BuildVideoJson(
    const std::string& id,
    int64_t user_id,
    const std::string& title,
    const std::string& storage_url,
    const std::string& mime,
    int64_t size_bytes,
    const std::optional<int64_t>& duration_ms,
    const std::string& thumbnail_url,
    const std::string& visibility,
    int64_t created_at_ms,
    const std::string& storyboard_url = {}
) {
    userver::formats::json::ValueBuilder vb;
    vb["id"]         = id;
    vb["user_id"]    = user_id;
    vb["title"]      = title;
    vb["url"]        = StorageUrlToHttp(storage_url);
    vb["mime"]       = mime;
    vb["size"]       = size_bytes;
    vb["visibility"] = visibility;
    vb["created_at"] = FormatTimestampMs(created_at_ms);

    if (duration_ms.has_value()) {
        vb["duration_ms"] = *duration_ms;
    } else {
        vb["duration_ms"] = userver::formats::json::ValueBuilder{
            userver::formats::common::Type::kNull}.ExtractValue();
    }
    if (!thumbnail_url.empty()) {
        vb["thumbnail"] = StorageUrlToHttp(thumbnail_url);
    } else {
        vb["thumbnail"] = userver::formats::json::ValueBuilder{
            userver::formats::common::Type::kNull}.ExtractValue();
    }
    if (!storyboard_url.empty()) {
        vb["storyboard"] = StorageUrlToHttp(storyboard_url);
    } else {
        vb["storyboard"] = userver::formats::json::ValueBuilder{
            userver::formats::common::Type::kNull}.ExtractValue();
    }
    return vb.ExtractValue();
}

// Build an HTTP-accessible storyboard URL from an `s3://…` value, or return
// null if unset. Used by feed handlers that include storyboard metadata.
inline userver::formats::json::Value BuildStoryboardJson(const std::string& storyboard_url) {
    if (storyboard_url.empty()) {
        return userver::formats::json::ValueBuilder{
            userver::formats::common::Type::kNull}.ExtractValue();
    }
    return userver::formats::json::ValueBuilder{StorageUrlToHttp(storyboard_url)}.ExtractValue();
}

}  // namespace real_medium::utils::video
