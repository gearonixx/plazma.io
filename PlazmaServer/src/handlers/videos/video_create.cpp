#include "video_create.hpp"

#include <userver/utils/uuid7.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/storages/scylla/operations.hpp>

namespace real_medium::handlers::videos::create {

namespace {

struct MultipartFile {
    std::string filename;
    std::string content_type;
    std::string data;
};

std::optional<MultipartFile> ParseFirstFilePart(
    const std::string& body,
    const std::string& boundary
) {
    const auto delim = "--" + boundary;
    auto part_start = body.find(delim);
    if (part_start == std::string::npos) return std::nullopt;

    part_start = body.find("\r\n", part_start) + 2; // skip delimiter line
    auto headers_end = body.find("\r\n\r\n", part_start);
    if (headers_end == std::string::npos) return std::nullopt;

    auto headers = body.substr(part_start, headers_end - part_start);
    auto data_start = headers_end + 4;

    auto next_delim = body.find(delim, data_start);
    if (next_delim == std::string::npos) return std::nullopt;

    // strip trailing \r\n before delimiter
    auto data_end = next_delim - 2;

    MultipartFile result;
    result.data = body.substr(data_start, data_end - data_start);

    // extract filename from Content-Disposition
    if (auto pos = headers.find("filename=\""); pos != std::string::npos) {
        pos += 10;
        auto end = headers.find('"', pos);
        result.filename = headers.substr(pos, end - pos);
    }

    // extract Content-Type
    if (auto pos = headers.find("Content-Type: "); pos != std::string::npos) {
        pos += 14;
        auto end = headers.find("\r\n", pos);
        result.content_type = headers.substr(pos, end == std::string::npos ? end : end - pos);
    }

    return result;
}


std::string ExtractBoundary(const std::string& content_type) {
    auto pos = content_type.find("boundary=");
    if (pos == std::string::npos) return {};
    pos += 9;
    if (pos < content_type.size() && content_type[pos] == '"') {
        ++pos;
        auto end = content_type.find('"', pos);
        if (end == std::string::npos) return {};
        return content_type.substr(pos, end - pos);
    }
    auto end = content_type.find_first_of("; \t\r\n", pos);
    return content_type.substr(pos, end - pos);
}

}  // namespace

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : HttpHandlerBase(config, context),
    s3_(context.FindComponent<s3::S3Component>()),
    session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {
}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    // 1. Parse multipart body
    const auto content_type = request.GetHeader("Content-Type");
    const auto boundary = ExtractBoundary(content_type);
    if (boundary.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "expected multipart/form-data"})";
    }

    const auto& body = request.RequestBody();
    auto file = ParseFirstFilePart(body, boundary);
    if (!file) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "no file found in request"})";
    }

    // 2. Generate S3 key and upload
    const auto video_id = userver::utils::generators::GenerateUuidV7();

    std::string safe_filename = file->filename;
    for (char& c : safe_filename) {
        if (c == ' ') c = '_';
    }

    const auto s3_key = "videos/" + video_id + "/" + safe_filename;

    auto client = s3_.GetClient();
    userver::s3api::Client::Meta meta;
    meta[userver::http::headers::kContentType] = file->content_type;
    client->PutObject(s3_key, file->data, meta);

    const auto storage_url = "s3://plazma-videos/" + s3_key;

    // 3. Insert into Cassandra
    // TODO: get user_id from auth token/session
    const int64_t user_id = 1;  // placeholder

    auto table = session_->GetTable("videos");
    userver::storages::scylla::operations::InsertOne insert;
    insert.BindInt64("user_id", user_id);
    insert.BindString("video_id", video_id);
    insert.BindString("title", safe_filename);
    insert.BindString("storage_url", storage_url);
    table.Execute(insert);

    // 4. Respond
    userver::formats::json::ValueBuilder response;
    response["video_id"] = video_id;
    response["storage_url"] = storage_url;
    response["filename"] = safe_filename;
    response["size"] = file->data.size();

    request.SetResponseStatus(userver::server::http::HttpStatus::kCreated);
    return userver::formats::json::ToString(response.ExtractValue());
}

}
