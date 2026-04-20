#include "video_view.hpp"

#include <userver/logging/log.hpp>

namespace real_medium::handlers::videos::view {

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
    const auto video_id = request.GetPathArg("id");

    // TODO: UPDATE plazma.video_stats SET views = views + 1 WHERE video_id = ?
    // Counter tables require a raw CQL path; wire up once userver-scylla exposes it.

    LOG_INFO() << "POST /v1/videos/" << video_id << "/view";
    request.SetResponseStatus(userver::server::http::HttpStatus::kNoContent);
    return "";
}

}  // namespace real_medium::handlers::videos::view
