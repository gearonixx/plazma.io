#pragma once

#include <userver/components/component_context.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/scylla/component.hpp>

namespace real_medium::handlers::playlists::detail {

class Handler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-playlists-detail";

    Handler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    userver::storages::scylla::SessionPtr session_;
};

}  // namespace real_medium::handlers::playlists::detail
