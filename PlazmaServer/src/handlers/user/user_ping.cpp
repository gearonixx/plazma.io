#include "user_ping.hpp"

#include <docs/api.hpp>
#include <userver/components/component_config.hpp>
#include <userver/storages/scylla/operations.hpp>

namespace real_medium::handlers::users::ping {

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : HttpHandlerJsonBase(config, context),
    session_(context.FindComponent<userver::components::Scylla>("scylla").GetSession()) {}

userver::formats::json::Value Handler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& /*request*/,
    const userver::formats::json::Value& request_json,
    userver::server::request::RequestContext& /*context*/
) const {
    const auto dto = request_json.As<::handlers::TelegramLoginDTO>();

    auto table = session_->GetTable("users_by_phone");
    userver::storages::scylla::operations::InsertOne insert;
    
    // this is temporary
    insert.BindString("phone_number", dto.phone_number);
    insert.BindInt64("user_id", dto.user_id);
    insert.BindString("username", dto.username.value_or(""));
    insert.BindString("first_name", dto.first_name);
    insert.BindString("last_name", dto.last_name.value_or(""));
    insert.BindBool("is_premium", dto.is_premium);
    table.Execute(insert);

    userver::formats::json::ValueBuilder builder;
    builder["ok"] = true;
    return builder.ExtractValue();
}

}  // namespace real_medium::handlers::users::ping
