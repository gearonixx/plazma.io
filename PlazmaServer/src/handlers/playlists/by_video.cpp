#include "by_video.hpp"

#include <stdexcept>

#include <userver/formats/common/type.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>

#include "playlists_common.hpp"
#include "utils/auth.hpp"

namespace real_medium::handlers::playlists::by_video {

namespace pc = real_medium::handlers::playlists::common;

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerBase(config, context),
      session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {}

std::string Handler::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& /*context*/
) const {
    request.GetHttpResponse().SetContentType("application/json");

    const auto auth = real_medium::utils::ExtractAuth(request);
    if (auth.result == real_medium::utils::AuthResult::kInvalid) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "invalid or expired token"})";
    }
    if (auth.result != real_medium::utils::AuthResult::kAuthenticated) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kUnauthorized);
        return R"({"error": "authentication required"})";
    }

    const auto video_id = request.GetPathArg("video_id");
    if (video_id.empty()) {
        // Empty path arg treated as "video has no entries" — match the spec
        // contract that empty results are 200 with an empty array.
        userver::formats::json::ValueBuilder response;
        response["playlist_ids"] = userver::formats::json::ValueBuilder{userver::formats::common::Type::kArray}
                                       .ExtractValue();
        return userver::formats::json::ToString(response.ExtractValue());
    }

    try {
        const auto memberships = pc::LoadVideoMemberships(session_, auth.user_id, video_id);

        userver::formats::json::ValueBuilder ids{userver::formats::common::Type::kArray};
        for (const auto& m : memberships) {
            ids.PushBack(m.playlist_id);
        }

        userver::formats::json::ValueBuilder response;
        response["playlist_ids"] = ids.ExtractValue();
        LOG_INFO() << "GET /v1/users/me/playlists/by_video/" << video_id
                   << " user_id=" << auth.user_id << " count=" << memberships.size();
        return userver::formats::json::ToString(response.ExtractValue());

    } catch (const std::exception& ex) {
        LOG_ERROR() << "GET /v1/users/me/playlists/by_video/" << video_id << " failed: " << ex.what();
        request.SetResponseStatus(userver::server::http::HttpStatus::kInternalServerError);
        return R"({"error": "internal error"})";
    }
}

}  // namespace real_medium::handlers::playlists::by_video
