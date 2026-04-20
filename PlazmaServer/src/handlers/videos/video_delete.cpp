#include "video_delete.hpp"

#include <userver/formats/json/serialize.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/row.hpp>

#include "utils/auth.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::videos::del {

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
    request.GetHttpResponse().SetContentType("application/json");

    const auto auth = utils::ExtractAuth(request);
    if (auth.result != utils::AuthResult::kAuthenticated) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "authentication required"})";
    }

    const auto video_id = request.GetPathArg("id");
    if (video_id.empty()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        return R"({"error": "missing video id"})";
    }

    auto by_id_table = session_->GetTable("video_by_id");
    userver::storages::scylla::operations::SelectOne select;
    select.AddAllColumns();
    select.WhereString("video_id", video_id);

    try {
        auto row = by_id_table.Execute(select);
        if (row.Empty()) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
            return R"({"error": "video not found"})";
        }

        const auto owner_id = row.Get<int64_t>("user_id");
        if (auth.user_id != owner_id) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kForbidden);
            return R"({"error": "not your video"})";
        }

        const auto storage_url = row.Get<std::string>("storage_url");
        const auto s3_key      = utils::video::S3KeyFromUrl(storage_url);

        // day and created_at are needed to delete from videos_by_day.
        // Both are always set for videos created after migration 004.
        const bool have_day_key = !row.IsNull("day") && !row.IsNull("created_at");
        const auto day          = have_day_key ? row.Get<std::string>("day") : std::string{};
        const int64_t created_at = have_day_key ? row.Get<int64_t>("created_at") : 0LL;

        // Delete from S3 first (idempotent — re-runs are safe even if object is gone)
        try {
            s3_.GetClient()->RemoveObject(s3_key);
        } catch (const std::exception& ex) {
            LOG_ERROR() << "S3 delete failed for key=" << s3_key << ": " << ex.what();
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadGateway);
            return R"({"error": "storage delete failed"})";
        }

        // Delete from video_by_id (primary lookup index)
        {
            userver::storages::scylla::operations::DeleteOne del;
            del.WhereString("video_id", video_id);
            by_id_table.Execute(del);
        }

        // Delete from videos (user's own partition)
        {
            auto table = session_->GetTable("videos");
            userver::storages::scylla::operations::DeleteOne del;
            del.WhereInt64("user_id",  owner_id);
            del.WhereString("video_id", video_id);
            table.Execute(del);
        }

        // Delete from videos_by_day (global feed index).
        // Only public videos have a row there; skipping is safe for private/unlisted.
        if (have_day_key) {
            auto table = session_->GetTable("videos_by_day");
            userver::storages::scylla::operations::DeleteOne del;
            del.WhereString("day",      day);
            del.WhereInt64("created_at", created_at);
            del.WhereString("video_id", video_id);
            table.Execute(del);
        } else {
            LOG_WARNING() << "DELETE /v1/videos/" << video_id
                          << ": skipping videos_by_day delete (pre-migration row, no day/created_at)";
        }

        LOG_INFO() << "DELETE /v1/videos/" << video_id << " user_id=" << auth.user_id;
        request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
        return "";

    } catch (const std::exception& ex) {
        LOG_ERROR() << "DELETE /v1/videos/" << video_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::videos::del
