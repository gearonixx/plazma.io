#pragma once

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/scylla/component.hpp>

#include "../../s3/component.hpp"

namespace real_medium::handlers::videos::del {

class Handler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-videos-delete";

    Handler(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    s3::S3Component& s3_;
    userver::storages::scylla::SessionPtr session_;
};

}  // namespace real_medium::handlers::videos::del
