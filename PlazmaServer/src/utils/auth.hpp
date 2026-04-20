#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <userver/server/http/http_request.hpp>

#include "jwt.hpp"

namespace real_medium::utils {

enum class AuthResult { kAnonymous, kAuthenticated, kInvalid };

struct AuthInfo {
    AuthResult result = AuthResult::kAnonymous;
    int64_t user_id = 0;
};

inline AuthInfo ExtractAuth(const userver::server::http::HttpRequest& request) {
    const auto auth = request.GetHeader("Authorization");
    if (auth.empty()) return {AuthResult::kAnonymous, 0};

    // RFC 7235: auth scheme tokens are case-insensitive
    if (auth.size() <= 7) return {AuthResult::kInvalid, 0};
    std::string scheme = auth.substr(0, 7);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
    if (scheme != "bearer ") return {AuthResult::kInvalid, 0};

    const std::string token = auth.substr(7);
    if (token.empty()) return {AuthResult::kInvalid, 0};

    auto user_id = jwt::Verify(token);
    if (!user_id) return {AuthResult::kInvalid, 0};
    return {AuthResult::kAuthenticated, *user_id};
}

}  // namespace real_medium::utils
