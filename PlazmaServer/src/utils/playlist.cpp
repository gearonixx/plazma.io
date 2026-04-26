#include "playlist.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

#include <unicode/unistr.h>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace real_medium::utils::playlist {

// ── Case folding ──────────────────────────────────────────────────────────────

std::string CaseFold(const std::string& in) {
    if (in.empty()) return {};
    auto u = icu::UnicodeString::fromUTF8(in);
    u.foldCase();
    std::string out;
    u.toUTF8String(out);
    return out;
}

// ── Cursor encoding (base64url, padding-free) ─────────────────────────────────

namespace {

std::string Base64UrlEncode(const std::string& in) {
    static const char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < in.size(); i += 3) {
        const std::uint32_t b = (static_cast<std::uint8_t>(in[i]) << 16) |
                                (i + 1 < in.size() ? static_cast<std::uint8_t>(in[i + 1]) << 8 : 0u) |
                                (i + 2 < in.size() ? static_cast<std::uint8_t>(in[i + 2]) : 0u);
        out.push_back(kAlpha[(b >> 18) & 0x3F]);
        out.push_back(kAlpha[(b >> 12) & 0x3F]);
        if (i + 1 < in.size()) out.push_back(kAlpha[(b >> 6) & 0x3F]);
        if (i + 2 < in.size()) out.push_back(kAlpha[b & 0x3F]);
    }
    return out;
}

std::string Base64UrlDecode(const std::string& in) {
    static const auto kRev = [] {
        std::array<int8_t, 256> t{};
        t.fill(-1);
        const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (int i = 0; alpha[i]; ++i) t[static_cast<std::uint8_t>(alpha[i])] = static_cast<int8_t>(i);
        return t;
    }();

    std::string out;
    out.reserve((in.size() * 3) / 4);
    std::uint32_t buf = 0;
    int bits = 0;
    for (const char c : in) {
        const int8_t v = kRev[static_cast<std::uint8_t>(c)];
        if (v < 0) throw std::invalid_argument("bad base64url char");
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace

std::string EncodeItemCursor(const ItemCursor& c) {
    userver::formats::json::ValueBuilder vb;
    vb["v"] = 1;
    vb["a"] = c.added_at_ms;
    vb["i"] = c.video_id;
    return Base64UrlEncode(userver::formats::json::ToString(vb.ExtractValue()));
}

std::optional<ItemCursor> DecodeItemCursor(const std::string& token) {
    try {
        const auto json = userver::formats::json::FromString(Base64UrlDecode(token));
        if (json["v"].As<int>(0) != 1) return std::nullopt;
        ItemCursor c;
        c.added_at_ms = json["a"].As<int64_t>(0);
        c.video_id = json["i"].As<std::string>("");
        return c;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace real_medium::utils::playlist
