#include "../session.h"

#include <QJsonObject>
#include <optional>

namespace validators {

std::optional<UserLogin> ensureLoginResponse(const QJsonObject& json, QString& error) {
    if (!json.contains("user") || !json["user"].isObject()) {
        error = "[response] user: field is missing";
        return std::nullopt;
    }
    const auto user = json["user"].toObject();

    if (!user.contains("user_id") || user["user_id"].toInteger() <= 0) {
        error = "[response] user_id: invalid or missing";
        return std::nullopt;
    }

    if (!user.contains("phone_number") || user["phone_number"].toString().isEmpty()) {
        error = "[response] phone_number: field is missing";
        return std::nullopt;
    }

    if (!user.contains("first_name") || user["first_name"].toString().isEmpty()) {
        error = "[response] first_name: field is missing";
        return std::nullopt;
    }

    return UserLogin{
        .userId = user["user_id"].toInteger(),
        .username = user["username"].toString(),
        .firstName = user["first_name"].toString(),
        .lastName = user["last_name"].toString(),
        .phoneNumber = user["phone_number"].toString(),
        .isPremium = user["is_premium"].toBool(),
    };
}

}  // namespace validators
