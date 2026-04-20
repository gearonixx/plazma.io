#include "video_get.hpp"

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/row.hpp>

#include "utils/auth.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::videos::get {

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : HttpHandlerBase(config, context),
    session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {
}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    request.GetHttpResponse().SetContentType("application/json");

    const auto video_id = request.GetPathArg("id");
    if (video_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "missing video id"})";
    }

    // Auth is optional; needed only for gating private videos
    const auto auth = utils::ExtractAuth(request);
    if (auth.result == utils::AuthResult::kInvalid) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "invalid or expired token"})";
    }

    auto table = session_->GetTable("video_by_id");
    userver::storages::scylla::operations::SelectOne select;
    select.AddAllColumns();
    select.WhereString("video_id", video_id);

    try {
        auto row = table.Execute(select);
        if (row.Empty()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "video not found"})";
        }

        const auto owner_id   = row.Get<int64_t>("user_id");
        const auto visibility = row.IsNull("visibility")
            ? std::string{"public"} : row.Get<std::string>("visibility");

        // Private videos are only accessible to their owner
        if (visibility == "private"
            && (auth.result != utils::AuthResult::kAuthenticated || auth.user_id != owner_id)) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kForbidden);
            return R"({"error": "access denied"})";
        }

        const int64_t created_at_ms = row.IsNull("created_at") ? 0LL : row.Get<int64_t>("created_at");
        const auto title            = row.Get<std::string>("title");
        const auto storage_url      = row.Get<std::string>("storage_url");
        const auto mime             = row.IsNull("mime")         ? std::string{} : row.Get<std::string>("mime");
        const auto size_bytes       = row.IsNull("size_bytes")   ? 0LL           : row.Get<int64_t>("size_bytes");
        const auto thumbnail        = row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url");
        const auto storyboard       = row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url");

        // Build video object including stats placeholder (zeros until counter table is wired)
        userver::formats::json::ValueBuilder video_vb;
        video_vb["id"]          = video_id;
        video_vb["user_id"]     = owner_id;
        video_vb["title"]       = title;
        video_vb["url"]         = utils::video::StorageUrlToHttp(storage_url);
        video_vb["mime"]        = mime;
        video_vb["size"]        = size_bytes;
        video_vb["visibility"]  = visibility;
        video_vb["created_at"]  = utils::video::FormatTimestampMs(created_at_ms);
        video_vb["duration_ms"] = userver::formats::json::ValueBuilder{
            userver::formats::common::Type::kNull}.ExtractValue();
        if (!thumbnail.empty()) {
            video_vb["thumbnail"] = utils::video::StorageUrlToHttp(thumbnail);
        } else {
            video_vb["thumbnail"] = userver::formats::json::ValueBuilder{
                userver::formats::common::Type::kNull}.ExtractValue();
        }
        video_vb["storyboard"] = utils::video::BuildStoryboardJson(storyboard);
        video_vb["stats"]["views"] = int64_t{0};
        video_vb["stats"]["likes"] = int64_t{0};

        userver::formats::json::ValueBuilder response;
        response["video"] = video_vb.ExtractValue();

        LOG_INFO() << "GET /v1/videos/" << video_id << " user_id=" << auth.user_id;
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/videos/" << video_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::videos::get
