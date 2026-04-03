#pragma once

#include <userver/clients/http/client.hpp>
#include <userver/s3api/authenticators/access_key.hpp>
#include <userver/s3api/clients/s3api.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/clients/http/component.hpp>


namespace real_medium::s3 {

using ClientPtr = userver::s3api::ClientPtr;

class S3Component : public userver::components::LoggableComponentBase {
public:
    static constexpr std::string_view kName = "s3-client";

    S3Component(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context);

    userver::s3api::ClientPtr GetClient() const;

private:
    userver::clients::http::Client& http_client_;
    userver::s3api::ClientPtr client_;
};

}
