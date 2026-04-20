#include "video_create.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <utility>

#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/utils/uuid7.hpp>

#include "utils/auth.hpp"
#include "utils/thumbnail.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::videos::create {

namespace {

struct MultipartPart {
    std::string name;
    std::string filename;
    std::string content_type;
    std::string data;
};

std::string ExtractBoundary(const std::string& ct) {
    auto pos = ct.find("boundary=");
    if (pos == std::string::npos) return {};
    pos += 9;
    if (pos < ct.size() && ct[pos] == '"') {
        ++pos;
        const auto end = ct.find('"', pos);
        return (end == std::string::npos) ? std::string{} : ct.substr(pos, end - pos);
    }
    const auto end = ct.find_first_of("; \t\r\n", pos);
    return ct.substr(pos, end - pos);
}

// Case-insensitive search for a header name within a MIME part headers block.
// Returns the position of the colon (or npos if not found).
size_t FindHeader(const std::string& headers, std::string_view name) {
    std::string lh = headers;
    std::transform(lh.begin(), lh.end(), lh.begin(), ::tolower);
    std::string ln(name);
    std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
    ln += ':';
    return lh.find(ln);
}

// Extract a named parameter from a Content-Disposition header value, e.g. name="foo".
// header_value is the full value string starting after "Content-Disposition:".
std::string ExtractDispositionParam(const std::string& headers, const std::string& param) {
    const auto hpos = FindHeader(headers, "Content-Disposition");
    if (hpos == std::string::npos) return {};
    const auto line_end = headers.find("\r\n", hpos);
    const auto line_len = (line_end == std::string::npos) ? std::string::npos : line_end - hpos;
    const auto line = headers.substr(hpos, line_len);

    // Case-insensitive search for param="
    std::string lline = line, lparam = param + "=\"";
    std::transform(lline.begin(), lline.end(), lline.begin(), ::tolower);
    std::transform(lparam.begin(), lparam.end(), lparam.begin(), ::tolower);
    const auto ppos = lline.find(lparam);
    if (ppos == std::string::npos) return {};
    const auto val_start = ppos + lparam.size();
    const auto val_end = line.find('"', val_start);
    return (val_end == std::string::npos) ? std::string{} : line.substr(val_start, val_end - val_start);
}

// Extract Content-Type value from a MIME part header block, stripping parameters.
std::string ExtractPartContentType(const std::string& headers) {
    const auto hpos = FindHeader(headers, "Content-Type");
    if (hpos == std::string::npos) return {};
    // Advance past the header name and colon
    auto pos = headers.find(':', hpos);
    if (pos == std::string::npos) return {};
    ++pos;
    while (pos < headers.size() && headers[pos] == ' ') ++pos;
    auto end = headers.find("\r\n", pos);
    auto ct = headers.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
    // Strip parameters after ';' (e.g. "; charset=utf-8")
    const auto semi = ct.find(';');
    if (semi != std::string::npos) ct.resize(semi);
    while (!ct.empty() && ct.back() == ' ') ct.pop_back();
    return ct;
}

std::vector<MultipartPart> ParseMultipart(const std::string& body, const std::string& boundary) {
    std::vector<MultipartPart> parts;
    const auto delim = "--" + boundary;

    size_t pos = body.find(delim);
    while (pos != std::string::npos) {
        pos += delim.size();

        // Check for terminal boundary "--" or part separator "\r\n"
        if (pos + 1 < body.size() && body[pos] == '-' && body[pos + 1] == '-') break;
        if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') {
            pos += 2;
        } else {
            break;  // malformed boundary line
        }

        const auto headers_end = body.find("\r\n\r\n", pos);
        if (headers_end == std::string::npos) break;
        const auto headers = body.substr(pos, headers_end - pos);
        const auto data_start = headers_end + 4;

        const auto next_delim = body.find("\r\n" + delim, data_start);
        if (next_delim == std::string::npos) break;

        MultipartPart part;
        part.name         = ExtractDispositionParam(headers, "name");
        part.filename     = ExtractDispositionParam(headers, "filename");
        part.content_type = ExtractPartContentType(headers);
        part.data         = body.substr(data_start, next_delim - data_start);
        parts.push_back(std::move(part));

        pos = next_delim + 2;  // skip the \r\n before the next delimiter
        pos = body.find(delim, pos);
    }
    return parts;
}

}  // namespace

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : HttpHandlerBase(config, context),
    s3_(context.FindComponent<s3::S3Component>()),
    session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()),
    fs_tp_(context.GetTaskProcessor("fs-task-processor")) {
}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    request.GetHttpResponse().SetContentType("application/json");

    // Auth is required; no anonymous uploads
    const auto auth = utils::ExtractAuth(request);
    if (auth.result == utils::AuthResult::kInvalid) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "invalid or expired token"})";
    }
    if (auth.result == utils::AuthResult::kAnonymous) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "authentication required"})";
    }
    const int64_t user_id = auth.user_id;

    const auto content_type_hdr = request.GetHeader("Content-Type");
    const auto boundary = ExtractBoundary(content_type_hdr);
    if (boundary.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "expected multipart/form-data with boundary"})";
    }

    const auto parts = ParseMultipart(request.RequestBody(), boundary);
    if (parts.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "no parts found in multipart body"})";
    }

    // Locate file part and optional form fields. The video file part is the
    // first non-thumbnail part with a filename; the optional thumbnail part is
    // keyed by name == "thumbnail" (client sends an optimistic frame so the
    // feed can display *something* before the server's ffmpeg job finishes).
    const MultipartPart* file_part      = nullptr;
    const MultipartPart* thumbnail_part = nullptr;
    std::string title_field;
    std::string visibility_field = "public";

    for (const auto& part : parts) {
        if (part.name == "thumbnail") {
            if (!part.data.empty()) thumbnail_part = &part;
        } else if (!part.filename.empty() && file_part == nullptr) {
            file_part = &part;
        } else if (part.name == "title") {
            title_field = part.data;
        } else if (part.name == "visibility") {
            const auto& v = part.data;
            if (v == "public" || v == "unlisted" || v == "private") visibility_field = v;
        }
    }

    if (!file_part) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "no file part found in request"})";
    }

    // Resolve MIME: prefer part Content-Type, fall back to extension inference
    std::string mime = file_part->content_type;
    if (mime.empty()) {
        mime = utils::video::MimeFromFilename(file_part->filename);
    }
    if (!utils::video::IsAllowedMime(mime)) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnsupportedMediaType);
        return R"({"error": "unsupported video format; allowed: mp4, webm, mkv, mov, avi, ogv"})";
    }

    if (file_part->data.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "file part is empty"})";
    }

    const auto video_id      = userver::utils::generators::GenerateUuidV7();
    const auto safe_filename = utils::video::SanitizeFilename(file_part->filename);
    const auto s3_key        = "videos/" + video_id + "/" + safe_filename;
    const auto storage_url   = "s3://plazma-videos/" + s3_key;
    const auto size_bytes    = static_cast<int64_t>(file_part->data.size());
    const auto created_at_ms = utils::video::NowMs();
    const auto day           = utils::video::DayString(created_at_ms);

    // Derive/normalize title
    std::string title = utils::video::NormalizeTitle(
        title_field.empty() ? [&] {
            std::string t = safe_filename;
            const auto dot = t.rfind('.');
            if (dot != std::string::npos) t.resize(dot);
            return t;
        }() : title_field
    );
    if (title.empty()) title = video_id;  // last-resort fallback

    // Upload to S3 first; if this fails don't write metadata
    try {
        userver::s3api::Client::Meta meta;
        meta[userver::http::headers::kContentType] = mime;
        s3_.GetClient()->PutObject(s3_key, file_part->data, meta);
    } catch (const std::exception& ex) {
        LOG_ERROR() << "S3 upload failed for video_id=" << video_id << ": " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadGateway);
        return R"({"error": "storage upload failed"})";
    }

    // Write metadata to all three tables (application-level "batch")
    try {
        {
            auto table = session_->GetTable("videos");
            userver::storages::scylla::operations::InsertOne ins;
            ins.BindInt64("user_id",      user_id);
            ins.BindString("video_id",    video_id);
            ins.BindString("title",       title);
            ins.BindString("storage_url", storage_url);
            ins.BindString("thumbnail_url", "");
            ins.BindString("mime",        mime);
            ins.BindInt64("size_bytes",   size_bytes);
            ins.BindString("visibility",  visibility_field);
            ins.BindInt64("created_at_ms", created_at_ms);
            table.Execute(ins);
        }
        {
            auto table = session_->GetTable("video_by_id");
            userver::storages::scylla::operations::InsertOne ins;
            ins.BindString("video_id",    video_id);
            ins.BindInt64("user_id",      user_id);
            ins.BindString("title",       title);
            ins.BindString("storage_url", storage_url);
            ins.BindString("mime",        mime);
            ins.BindInt64("size_bytes",   size_bytes);
            ins.BindString("thumbnail_url", "");
            ins.BindString("visibility",  visibility_field);
            ins.BindInt64("created_at",   created_at_ms);
            ins.BindString("day",         day);
            table.Execute(ins);
        }
        // Only public videos go into the global feed table;
        // private/unlisted are accessible only via direct lookup or the owner's library.
        if (visibility_field == "public") {
            auto table = session_->GetTable("videos_by_day");
            userver::storages::scylla::operations::InsertOne ins;
            ins.BindString("day",         day);
            ins.BindInt64("created_at",   created_at_ms);
            ins.BindString("video_id",    video_id);
            ins.BindInt64("user_id",      user_id);
            ins.BindString("title",       title);
            ins.BindString("storage_url", storage_url);
            ins.BindString("thumbnail_url", "");
            ins.BindString("mime",        mime);
            ins.BindInt64("size_bytes",   size_bytes);
            ins.BindString("visibility",  visibility_field);
            table.Execute(ins);
        }
    } catch (const std::exception& ex) {
        LOG_ERROR() << "Scylla write failed for video_id=" << video_id
                    << " user_id=" << user_id << ": " << ex.what();
        // S3 object is already uploaded; orphaned until a future cleanup job runs.
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "metadata write failed"})";
    }

    // Phase 2: persist the client-supplied optimistic thumbnail synchronously
    // so the response already carries a working thumbnail URL. Small image, tiny
    // cost; if it fails we just let the server-side extraction fill the gap.
    std::string thumb_url_for_response;
    if (thumbnail_part != nullptr) {
        std::string thumb_mime = thumbnail_part->content_type;
        if (thumb_mime.empty()) thumb_mime = "image/jpeg";
        try {
            SeedOptimisticThumbnail(video_id, thumbnail_part->data, thumb_mime);
            thumb_url_for_response = "s3://plazma-videos/videos/" + video_id + "/thumb.jpg";
        } catch (const std::exception& ex) {
            LOG_WARNING() << "optimistic thumbnail seed failed video_id=" << video_id
                          << ": " << ex.what();
        }
    }

    // Phase 1 + 3: run ffmpeg-based derivatives (authoritative thumbnail +
    // storyboard sprite) asynchronously on the fs task processor so the client
    // isn't blocked. If the client already supplied a usable thumbnail, skip
    // the primary frame extraction — the storyboard still runs.
    const bool have_optimistic = !thumb_url_for_response.empty();
    ScheduleMediaDerivatives(
        file_part->data, mime, video_id, user_id, day,
        visibility_field, have_optimistic
    );

    LOG_INFO() << "POST /v1/videos video_id=" << video_id
               << " user_id=" << user_id
               << " size=" << size_bytes
               << " mime=" << mime
               << " visibility=" << visibility_field
               << " optimistic_thumb=" << (have_optimistic ? "yes" : "no");

    userver::formats::json::ValueBuilder response;
    response["video"] = utils::video::BuildVideoJson(
        video_id, user_id, title, storage_url, mime, size_bytes,
        std::nullopt, thumb_url_for_response, visibility_field, created_at_ms
    );
    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    return userver::formats::json::ToString(response.ExtractValue());
}

void Handler::SeedOptimisticThumbnail(const std::string& video_id,
                                      const std::string& thumb_bytes,
                                      const std::string& thumb_mime) const {
    const std::string key = "videos/" + video_id + "/thumb.jpg";
    const std::string url = "s3://plazma-videos/" + key;

    userver::s3api::Client::Meta meta;
    meta[userver::http::headers::kContentType] = thumb_mime;
    s3_.GetClient()->PutObject(key, thumb_bytes, meta);

    // Look up the rest of the primary-key material (user_id, day, visibility,
    // created_at) via the single-partition video_by_id table. The caller just
    // finished writing this row, so the read should find it immediately.
    int64_t user_id    = 0;
    int64_t created_at = 0;
    std::string day;
    std::string visibility;
    {
        auto table = session_->GetTable("video_by_id");
        userver::storages::scylla::operations::SelectOne sel;
        sel.AddAllColumns();
        sel.WhereString("video_id", video_id);
        auto row = table.Execute(sel);
        if (row.Empty()) return;  // metadata row vanished — nothing to do
        user_id    = row.Get<int64_t>("user_id");
        day        = row.IsNull("day") ? std::string{} : row.Get<std::string>("day");
        visibility = row.IsNull("visibility") ? std::string{"public"} : row.Get<std::string>("visibility");
        created_at = row.IsNull("created_at") ? 0LL : row.Get<int64_t>("created_at");
    }

    {
        auto table = session_->GetTable("videos");
        userver::storages::scylla::operations::UpdateOne upd;
        upd.SetString("thumbnail_url", url);
        upd.WhereInt64("user_id", user_id);
        upd.WhereString("video_id", video_id);
        table.Execute(upd);
    }
    {
        auto table = session_->GetTable("video_by_id");
        userver::storages::scylla::operations::UpdateOne upd;
        upd.SetString("thumbnail_url", url);
        upd.WhereString("video_id", video_id);
        table.Execute(upd);
    }
    if (visibility == "public" && !day.empty() && created_at > 0) {
        auto table = session_->GetTable("videos_by_day");
        userver::storages::scylla::operations::UpdateOne upd;
        upd.SetString("thumbnail_url", url);
        upd.WhereString("day", day);
        upd.WhereInt64("created_at", created_at);
        upd.WhereString("video_id", video_id);
        table.Execute(upd);
    }
}

void Handler::ScheduleMediaDerivatives(std::string video_bytes,
                                       std::string mime,
                                       std::string video_id,
                                       int64_t user_id,
                                       std::string day,
                                       std::string visibility,
                                       bool skip_primary_thumb) const {
    // Handler is a userver component — its lifetime spans the entire server
    // process, so capturing `this` by value is safe across the detached task.
    userver::engine::AsyncNoSpan(fs_tp_, [
        this,
        video_bytes = std::move(video_bytes),
        mime = std::move(mime),
        video_id = std::move(video_id),
        user_id,
        day = std::move(day),
        visibility = std::move(visibility),
        skip_primary_thumb
    ]() mutable {
        namespace ops = userver::storages::scylla::operations;

        // video_by_id is our source of truth for created_at; we need it to
        // update videos_by_day (public feed).
        int64_t created_at = 0;
        try {
            auto table = session_->GetTable("video_by_id");
            ops::SelectOne sel;
            sel.AddAllColumns();
            sel.WhereString("video_id", video_id);
            auto row = table.Execute(sel);
            if (!row.Empty() && !row.IsNull("created_at")) {
                created_at = row.Get<int64_t>("created_at");
            }
        } catch (const std::exception& ex) {
            LOG_WARNING() << "derivatives: failed to read video_by_id.created_at for "
                          << video_id << ": " << ex.what();
        }

        auto update_all = [&](const std::string& column, const std::string& value) {
            try {
                auto table = session_->GetTable("videos");
                ops::UpdateOne upd;
                upd.SetString(column, value);
                upd.WhereInt64("user_id", user_id);
                upd.WhereString("video_id", video_id);
                table.Execute(upd);
            } catch (const std::exception& ex) {
                LOG_WARNING() << "derivatives: videos update failed: " << ex.what();
            }
            try {
                auto table = session_->GetTable("video_by_id");
                ops::UpdateOne upd;
                upd.SetString(column, value);
                upd.WhereString("video_id", video_id);
                table.Execute(upd);
            } catch (const std::exception& ex) {
                LOG_WARNING() << "derivatives: video_by_id update failed: " << ex.what();
            }
            if (visibility == "public" && !day.empty() && created_at > 0) {
                try {
                    auto table = session_->GetTable("videos_by_day");
                    ops::UpdateOne upd;
                    upd.SetString(column, value);
                    upd.WhereString("day", day);
                    upd.WhereInt64("created_at", created_at);
                    upd.WhereString("video_id", video_id);
                    table.Execute(upd);
                } catch (const std::exception& ex) {
                    LOG_WARNING() << "derivatives: videos_by_day update failed: " << ex.what();
                }
            }
        };

        // Primary thumbnail — skip if the client already seeded a good one.
        if (!skip_primary_thumb) {
            try {
                auto jpg = utils::thumbnail::Extract(video_bytes, mime, video_id);
                const std::string key = "videos/" + video_id + "/thumb.jpg";
                const std::string url = "s3://plazma-videos/" + key;
                userver::s3api::Client::Meta meta;
                meta[userver::http::headers::kContentType] = "image/jpeg";
                s3_.GetClient()->PutObject(key, jpg, meta);
                update_all("thumbnail_url", url);
                LOG_INFO() << "derivatives: thumbnail ready for " << video_id
                           << " (" << jpg.size() << " bytes)";
            } catch (const std::exception& ex) {
                LOG_WARNING() << "derivatives: thumbnail extraction failed for "
                              << video_id << ": " << ex.what();
            }
        }

        // Storyboard sprite — independent of the primary thumbnail; failures
        // here should not block the already-uploaded main video.
        try {
            auto jpg = utils::thumbnail::ExtractStoryboard(video_bytes, mime, video_id);
            const std::string key = "videos/" + video_id + "/storyboard.jpg";
            const std::string url = "s3://plazma-videos/" + key;
            userver::s3api::Client::Meta meta;
            meta[userver::http::headers::kContentType] = "image/jpeg";
            s3_ref->GetClient()->PutObject(key, jpg, meta);
            update_all("storyboard_url", url);
            LOG_INFO() << "derivatives: storyboard ready for " << video_id
                       << " (" << jpg.size() << " bytes)";
        } catch (const std::exception& ex) {
            LOG_WARNING() << "derivatives: storyboard extraction failed for "
                          << video_id << ": " << ex.what();
        }
    }).Detach();
}

}  // namespace real_medium::handlers::videos::create
