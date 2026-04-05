#include "../session.h"

#include <QJsonObject>
#include <optional>

namespace validators {

std::optional<UserLogin> ensureLoginResponse(const QJsonObject& json, QString& error) {
    if (!json.contains("user_id") || json["user_id"].toInteger() <= 0) {
        error = "user_id: invalid or missing";
        return std::nullopt;
    }

    if (!json.contains("phone_number") || json["phone_number"].toString().isEmpty()) {
        error = "phone_number: field is missing";
        return std::nullopt;
    }

    if (!json.contains("first_name") || json["first_name"].toString().isEmpty()) {
        error = "first_name: field is missing";
        return std::nullopt;
    }

    return UserLogin{
        .userId = json["user_id"].toInteger(),
        .username = json["username"].toString(),
        .firstName = json["first_name"].toString(),
        .lastName = json["last_name"].toString(),
        .phoneNumber = json["phone_number"].toString(),
        .isPremium = json["is_premium"].toBool(),
    };
}

}  // namespace validators
