#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>

#include <userver/storages/scylla/component.hpp>

// #include <userver/server/handlers/ping.hpp>
// #include <userver/server/handlers/tests_control.hpp>
// #include <userver/storages/postgres/component.hpp>

// #include <userver/storages/mongo/component.hpp>

// #include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "handlers/user/auth_login.hpp"
#include "handlers/videos/video_create.hpp"

#include "s3/component.hpp"

// using namespace real_medium::handlers;

int main(int argc, char* argv[]) {
    // userver::server::handlers::auth::RegisterAuthCheckerFactory<real_medium::auth::CheckerFactory>();

    auto component_list = userver::components::MinimalServerComponentList()
                            //   .Append<userver::components::TestsuiteSupport>()
                              .AppendComponentList(userver::clients::http::ComponentList())
                              .Append<userver::components::Scylla>("scylla")
                              .Append<real_medium::handlers::users::auth_login::Handler>()
                              .Append<real_medium::handlers::videos::create::Handler>()
                              .Append<real_medium::s3::S3Component>()
                            //   .Append<userver::server::handlers::TestsControl>()
                              .Append<userver::clients::dns::Component>();

    // real_medium::handlers::users_login::post::AppendLoginUser(component_list);

    return userver::utils::DaemonMain(argc, argv, component_list);
}