#pragma once

#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/scylla/component.hpp>

namespace real_medium::handlers::users::ping {

class Handler final : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "handler-user-ping";

    Handler(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_json,
        userver::server::request::RequestContext& context
    ) const override;

private:
    userver::storages::scylla::SessionPtr session_;
};

}  // namespace real_medium::handlers::users::ping