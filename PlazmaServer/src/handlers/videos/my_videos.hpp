#pragma once

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/scylla/component.hpp>

namespace real_medium::handlers::videos::my {

class Handler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-my-videos";

    Handler(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    userver::storages::scylla::SessionPtr session_;
};

}  // namespace real_medium::handlers::videos::my
