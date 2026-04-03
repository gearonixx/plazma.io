#pragma once

#include <fmt/format.h>
#include <string_view>

#include <userver/components/component_fwd.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/utils/assert.hpp>

namespace real_medium::handlers {

class Common : public userver::server::handlers::HttpHandlerJsonBase {
public:
    Common(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context
    );


};

}  // namespace real_medium::handlers
