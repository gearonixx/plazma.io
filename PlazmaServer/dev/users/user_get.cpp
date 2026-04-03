#include "user_get.hpp"
#include "db/sql.hpp"
#include "models/user.hpp"
#include "utils/make_error.hpp"

#include <optional>
#include <string>

namespace real_medium::handlers::users::get {



// Why salt? 
// Without it, two users with password "hunter2" have identical hashes. Attacker cracks one, cracks both.
//  Salt makes each hash unique even for identical passwords.

using ResultSet = userver::v2_16_rc::storages::postgres::ResultSet;

userver::formats::json::Value Handler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& /*request_json*/,
    userver::server::request::RequestContext& context
) const {
    auto user_id = context.GetData<std::optional<std::string>>("id");

    const ResultSet result =
        GetPg().Execute(userver::storages::postgres::ClusterHostType::kMaster, sql::kFindUserById, user_id);

    if (result.IsEmpty()) {
        auto& response = request.GetHttpResponse();
        response.SetStatus(userver::server::http::HttpStatus::kNotFound);
        return utils::error::MakeError("user_id", "Invalid user_id. Not found.");
    }

    auto user = result.AsSingleRow<real_medium::models::User>(userver::storages::postgres::kRowTag);

    userver::formats::json::ValueBuilder builder;
    builder["user"] = user;

    return builder.ExtractValue();
}

}  // namespace real_medium::handlers::users::get
