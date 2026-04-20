#pragma once

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/scylla/component.hpp>

namespace real_medium::handlers::videos::view {

class Handler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-videos-view";

    Handler(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

    // session_ reserved for counter increment (video_stats UPDATE) once raw CQL path is available
private:
    userver::storages::scylla::SessionPtr session_;
};

}  // namespace real_medium::handlers::videos::view
