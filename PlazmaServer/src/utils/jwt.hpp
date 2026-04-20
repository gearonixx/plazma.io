#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace real_medium::utils::jwt {

// Mint a 30-day HS256 JWT. Payload: {sub, phone, iat, exp}.
std::string Mint(int64_t user_id, const std::string& phone);

// Verify signature + expiry. Returns user_id on success, nullopt on failure.
std::optional<int64_t> Verify(const std::string& token);

}  // namespace real_medium::utils::jwt
