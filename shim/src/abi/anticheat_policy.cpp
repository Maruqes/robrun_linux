#include "abi/anticheat_policy.hpp"

#include <cstring>

namespace eos_ac {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool equals(const char* text, const char* word) { return std::strcmp(text, word) == 0; }

}  // namespace

bool parse_policy(const char* text, Policy* out) {
    if (!text || !out) return false;

    if (equals(text, "off")) {
        *out = Policy{};
        out->mode = Mode::Off;
        return true;
    }
    if (equals(text, "null")) {
        *out = Policy{};
        out->mode = Mode::Null;
        return true;
    }
    if (equals(text, "noiface")) {
        *out = Policy{};
        out->mode = Mode::NoIface;
        return true;
    }
    if (equals(text, "echo")) {
        *out = Policy{};
        out->mode = Mode::Echo;
        return true;
    }

    static const char kPrefix[] = "probe:";
    if (std::strncmp(text, kPrefix, sizeof kPrefix - 1) != 0) return false;

    const char* hex = text + (sizeof kPrefix - 1);
    const std::size_t digits = std::strlen(hex);
    if ((digits % 2) != 0 || digits / 2 > kProbeMax) return false;

    Policy parsed{};
    parsed.mode = Mode::Probe;
    for (std::size_t i = 0; i < digits; i += 2) {
        const int hi = hex_value(hex[i]);
        const int lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        parsed.probe[parsed.probe_len++] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    *out = parsed;
    return true;
}

std::size_t reply_for(const Policy& policy, const std::uint8_t* in, std::size_t in_len,
                      std::uint8_t* out, std::size_t cap) {
    switch (policy.mode) {
        case Mode::Off:
        case Mode::Null:
        case Mode::NoIface:
            return 0;
        case Mode::Echo:
            if (!in || in_len == 0 || in_len > cap) return 0;
            std::memcpy(out, in, in_len);
            return in_len;
        case Mode::Probe:
            if (policy.probe_len == 0 || policy.probe_len > cap) return 0;
            std::memcpy(out, policy.probe, policy.probe_len);
            return policy.probe_len;
    }
    return 0;
}

bool interface_available(const Policy& policy) { return policy.mode != Mode::NoIface; }

Verdict verdict_for(const Policy& policy, std::size_t challenges, std::size_t sends) {
    if (challenges == 0) return Verdict::NoChallenges;
    if (sends == 0) return Verdict::Silent;
    if (policy.mode == Mode::Echo) return Verdict::Echoed;
    return Verdict::Answered;
}

const char* verdict_name(Verdict verdict) {
    switch (verdict) {
        case Verdict::NoChallenges: return "no-challenges";
        case Verdict::Silent: return "challenged-no-response";
        case Verdict::Echoed: return "challenged-echoed";
        case Verdict::Answered: return "challenged-answered";
    }
    return "unknown";
}

std::size_t to_hex(const std::uint8_t* data, std::size_t len, char* out, std::size_t cap) {
    static const char kDigits[] = "0123456789abcdef";
    if (!out || cap < len * 2 + 1) return 0;
    for (std::size_t i = 0; i < len; ++i) {
        out[i * 2] = kDigits[data[i] >> 4];
        out[i * 2 + 1] = kDigits[data[i] & 0x0f];
    }
    out[len * 2] = '\0';
    return len * 2;
}

}  // namespace eos_ac
