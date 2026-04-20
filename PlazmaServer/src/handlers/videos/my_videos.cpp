#include "my_videos.hpp"

#include <algorithm>
#include <stdexcept>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/scylla/operations.hpp>
#include <userver/storages/scylla/row.hpp>

#include "utils/auth.hpp"
#include "utils/video.hpp"

namespace real_medium::handlers::videos::my {

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

    const auto auth = utils::ExtractAuth(request);
    if (auth.result != utils::AuthResult::kAuthenticated) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "authentication required"})";
    }

    int limit = 20;
    const auto limit_str = request.GetArg("limit");
    if (!limit_str.empty()) {
        try {
            limit = std::clamp(std::stoi(limit_str), 1, 100);
        } catch (const std::exception&) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
            return R"({"error": "limit must be an integer in [1, 100]"})";
        }
    }

    userver::formats::json::ValueBuilder videos_arr{userver::formats::common::Type::kArray};

    try {
        auto table = session_->GetTable("videos");
        userver::storages::scylla::operations::SelectMany select;
        select.AddAllColumns();
        select.WhereInt64("user_id", auth.user_id);

        int collected = 0;
        for (const auto& row : table.Execute(select)) {
            if (collected >= limit) break;

            const int64_t created_at_ms = row.IsNull("created_at_ms")
                ? 0LL : row.Get<int64_t>("created_at_ms");

            videos_arr.PushBack(utils::video::BuildVideoJson(
                row.Get<std::string>("video_id"),
                auth.user_id,
                row.Get<std::string>("title"),
                row.Get<std::string>("storage_url"),
                row.IsNull("mime")        ? std::string{} : row.Get<std::string>("mime"),
                row.IsNull("size_bytes")  ? 0LL           : row.Get<int64_t>("size_bytes"),
                std::nullopt,
                row.IsNull("thumbnail_url") ? std::string{} : row.Get<std::string>("thumbnail_url"),
                row.IsNull("visibility")  ? std::string{"public"} : row.Get<std::string>("visibility"),
                created_at_ms,
                row.IsNull("storyboard_url") ? std::string{} : row.Get<std::string>("storyboard_url")
            ));
            ++collected;
        }
    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/users/me/videos user_id=" << auth.user_id
                    << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }

    LOG_INFO() << "GET /v1/users/me/videos user_id=" << auth.user_id;

    userver::formats::json::ValueBuilder response;
    response["videos"]      = videos_arr.ExtractValue();
    response["next_cursor"] = userver::formats::json::ValueBuilder{
        userver::formats::common::Type::kNull}.ExtractValue();
    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace real_medium::handlers::videos::my
