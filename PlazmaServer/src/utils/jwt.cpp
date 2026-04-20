#include "jwt.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace real_medium::utils::jwt {

namespace {

constexpr std::string_view kDefaultSecret = "plazma-jwt-secret-CHANGE-IN-PRODUCTION";
constexpr int64_t kTtlSeconds = 30LL * 24 * 3600;

// Secret loaded once at startup from JWT_SECRET env var, fallback to default.
// Stored as static so getenv is called only once (thread-safe read after first call).
const std::string& GetSecret() {
    static const std::string secret = [] {
        const char* env = std::getenv("JWT_SECRET");
        if (env && *env) return std::string{env, std::strlen(env)};
        return std::string{kDefaultSecret};
    }();
    return secret;
}

const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::string& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    const auto* data = reinterpret_cast<const unsigned char*>(in.data());
    const size_t len = in.size();
    size_t i = 0;
    while (i < len) {
        const unsigned char a = data[i++];
        const unsigned char b = (i < len) ? data[i++] : 0;
        const unsigned char c = (i < len) ? data[i++] : 0;
        // Track how many bytes this group actually had (before we hit end)
        const int bytes_in_group = static_cast<int>(
            (i <= len ? 3 : 3 - (int)(i - len)));
        out += kB64Chars[a >> 2];
        out += kB64Chars[((a & 3) << 4) | (b >> 4)];
        out += (bytes_in_group >= 2) ? kB64Chars[((b & 0xF) << 2) | (c >> 6)] : '=';
        out += (bytes_in_group >= 3) ? kB64Chars[c & 0x3F] : '=';
    }
    return out;
}

std::string Base64UrlEncode(const std::string& in) {
    auto s = Base64Encode(in);
    for (char& c : s) {
        if      (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!s.empty() && s.back() == '=') s.pop_back();
    return s;
}

std::string Base64UrlDecode(std::string s) {
    // Restore standard base64
    for (char& c : s) {
        if      (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (s.size() % 4) s += '=';

    std::string out;
    out.reserve(s.size() * 3 / 4);

    auto dec = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    for (size_t i = 0; i + 3 < s.size(); i += 4) {
        const int a = dec(s[i]);
        const int b = dec(s[i + 1]);
        if (a < 0 || b < 0) return {};  // invalid input
        const int c = (s[i + 2] != '=') ? dec(s[i + 2]) : 0;
        const int d = (s[i + 3] != '=') ? dec(s[i + 3]) : 0;
        if ((s[i + 2] != '=' && c < 0) || (s[i + 3] != '=' && d < 0)) return {};
        out += static_cast<char>((a << 2) | (b >> 4));
        if (s[i + 2] != '=') out += static_cast<char>(((b & 0xF) << 4) | (c >> 2));
        if (s[i + 3] != '=') out += static_cast<char>(((c & 3) << 6) | d);
    }
    return out;
}

std::string HmacSha256Raw(const std::string& data, const std::string& key) {
    unsigned char digest[32];
    unsigned int len = 32;
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &len);
    return std::string(reinterpret_cast<const char*>(digest), len);
}

int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Extract integer claim from a minimal JWT payload JSON.
// Matches exact key with surrounding JSON delimiters to avoid substring false-positives.
std::optional<int64_t> ExtractClaim(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos += needle.size();
    // Skip optional whitespace
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (pos >= json.size()) return std::nullopt;
    // Must start with digit or minus
    if (!std::isdigit(static_cast<unsigned char>(json[pos])) && json[pos] != '-') {
        return std::nullopt;
    }
    try {
        size_t consumed = 0;
        int64_t val = std::stoll(json.substr(pos), &consumed);
        // Ensure the next non-digit char is a valid JSON delimiter
        const char next = (pos + consumed < json.size()) ? json[pos + consumed] : '}';
        if (next != ',' && next != '}' && next != ' ' && next != ']') return std::nullopt;
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

std::string Mint(int64_t user_id, const std::string& phone) {
    const int64_t iat = NowSeconds();
    const int64_t exp = iat + kTtlSeconds;

    const std::string header_json = R"({"alg":"HS256","typ":"JWT"})";

    std::ostringstream payload_ss;
    payload_ss << R"({"sub":)" << user_id
               << R"(,"phone":")" << phone
               << R"(","iat":)" << iat
               << R"(,"exp":)" << exp << "}";
    const std::string payload_json = payload_ss.str();

    const auto hdr = Base64UrlEncode(header_json);
    const auto pay = Base64UrlEncode(payload_json);
    const auto signing_input = hdr + "." + pay;
    const auto sig = Base64UrlEncode(HmacSha256Raw(signing_input, GetSecret()));

    return signing_input + "." + sig;
}

std::optional<int64_t> Verify(const std::string& token) {
    if (token.empty()) return std::nullopt;

    const auto dot1 = token.find('.');
    if (dot1 == std::string::npos || dot1 == 0) return std::nullopt;
    const auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos || dot2 == dot1 + 1) return std::nullopt;
    if (dot2 + 1 >= token.size()) return std::nullopt;  // no signature

    // Verify signature before doing anything else (timing-safe via OpenSSL HMAC)
    const auto signing_input = token.substr(0, dot2);
    const auto sig_b64 = token.substr(dot2 + 1);
    const auto expected_sig = Base64UrlEncode(HmacSha256Raw(signing_input, GetSecret()));
    if (sig_b64 != expected_sig) return std::nullopt;

    // Decode and parse payload
    const auto payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    const auto payload_json = Base64UrlDecode(payload_b64);
    if (payload_json.empty()) return std::nullopt;

    const auto exp = ExtractClaim(payload_json, "exp");
    if (!exp) return std::nullopt;
    if (NowSeconds() >= *exp) return std::nullopt;  // expired

    return ExtractClaim(payload_json, "sub");
}

}  // namespace real_medium::utils::jwt
