#include "common.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>

namespace real_medium::handlers {

Common::Common(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& component_context
)
    : HttpHandlerJsonBase(config, component_context) {}
}  // namespace real_medium::handlers
