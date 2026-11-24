// The anti-cheat client policy: EOSSHIM_AC parsing and the reply the shim
// hands back to the game. The Windows ABI is not exercised here.
#include "abi/anticheat_policy.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++failures;
}

bool parse(const char* text, eos_ac::Policy* policy) { return eos_ac::parse_policy(text, policy); }

}  // namespace

int main() {
    eos_ac::Policy policy;
    check(eos_ac::Policy{}.mode == eos_ac::Mode::Null,
          "the default policy refuses the session");

    check(parse("off", &policy), "off parses");
    check(policy.mode == eos_ac::Mode::Off, "off selects Mode::Off");

    check(parse("null", &policy), "null parses");
    check(policy.mode == eos_ac::Mode::Null, "null selects Mode::Null");
    check(policy.probe_len == 0, "null carries no probe");

    check(parse("echo", &policy), "echo parses");
    check(policy.mode == eos_ac::Mode::Echo, "echo selects Mode::Echo");

    check(parse("probe:00ff10", &policy), "probe:00ff10 parses");
    check(policy.mode == eos_ac::Mode::Probe, "probe selects Mode::Probe");
    check(policy.probe_len == 3, "probe carries three bytes");
    check(policy.probe[0] == 0x00 && policy.probe[1] == 0xff && policy.probe[2] == 0x10,
          "probe bytes decode in order");

    check(parse("probe:ABCDEF", &policy), "uppercase hex parses");
    check(policy.probe_len == 3 && policy.probe[0] == 0xab && policy.probe[2] == 0xef,
          "uppercase hex decodes");

    check(parse("probe:", &policy), "an empty probe parses");
    check(policy.mode == eos_ac::Mode::Probe && policy.probe_len == 0,
          "an empty probe is Mode::Probe with no bytes");

    check(!parse("probe:0", &policy), "odd digit counts are rejected");
    check(!parse("probe:zz", &policy), "non-hex digits are rejected");
    check(!parse("probe: 00", &policy), "spaces are rejected");
    check(!parse("nullx", &policy), "unknown words are rejected");
    check(!parse("", &policy), "empty text is rejected");
    check(!parse(nullptr, &policy), "null text is rejected");

    char wide[eos_ac::kProbeMax * 2 + 3];
    for (std::size_t i = 0; i < eos_ac::kProbeMax * 2; ++i) wide[i] = 'a';
    wide[eos_ac::kProbeMax * 2] = '\0';
    check(parse((std::string("probe:") + wide).c_str(), &policy),
          "a probe of exactly kProbeMax bytes parses");
    check(policy.probe_len == eos_ac::kProbeMax, "a probe of exactly kProbeMax is full length");

    for (std::size_t i = 0; i < eos_ac::kProbeMax * 2 + 2; ++i) wide[i] = 'a';
    wide[eos_ac::kProbeMax * 2 + 2] = '\0';
    check(!parse((std::string("probe:") + wide).c_str(), &policy),
          "a probe longer than kProbeMax is rejected");

    // interface_available -------------------------------------------------------

    eos_ac::Policy iface;
    iface.mode = eos_ac::Mode::Off;
    check(eos_ac::interface_available(iface), "off hands out the interface");
    iface.mode = eos_ac::Mode::Echo;
    check(eos_ac::interface_available(iface), "echo hands out the interface");
    iface.mode = eos_ac::Mode::Probe;
    check(eos_ac::interface_available(iface), "probe hands out the interface");
    iface.mode = eos_ac::Mode::Null;
    check(eos_ac::interface_available(iface), "null keeps the interface and refuses the session");
    iface.mode = eos_ac::Mode::NoIface;
    check(!eos_ac::interface_available(iface), "noiface reports no interface");

    // reply_for -----------------------------------------------------------------

    const std::uint8_t incoming[] = {1, 2, 3, 4};
    std::uint8_t out[8] = {};

    eos_ac::Policy off;
    off.mode = eos_ac::Mode::Off;
    check(eos_ac::reply_for(off, incoming, sizeof incoming, out, sizeof out) == 0,
          "off replies with nothing");

    eos_ac::Policy null_policy;
    null_policy.mode = eos_ac::Mode::Null;
    check(eos_ac::reply_for(null_policy, incoming, sizeof incoming, out, sizeof out) == 0,
          "null replies with nothing");

    eos_ac::Policy echo;
    echo.mode = eos_ac::Mode::Echo;
    check(eos_ac::reply_for(echo, incoming, sizeof incoming, out, sizeof out) == 4,
          "echo replies with the incoming length");
    check(std::memcmp(out, incoming, 4) == 0, "echo copies the incoming bytes");
    check(eos_ac::reply_for(echo, incoming, sizeof incoming, out, 3) == 0,
          "echo refuses a buffer that cannot hold the message");

    eos_ac::Policy probe;
    probe.mode = eos_ac::Mode::Probe;
    probe.probe[0] = 0xaa;
    probe.probe[1] = 0xbb;
    probe.probe_len = 2;
    check(eos_ac::reply_for(probe, incoming, sizeof incoming, out, sizeof out) == 2,
          "probe replies with the configured length");
    check(out[0] == 0xaa && out[1] == 0xbb, "probe copies the configured bytes");
    check(eos_ac::reply_for(probe, nullptr, 0, out, sizeof out) == 2,
          "probe does not depend on the incoming message");

    eos_ac::Policy empty_probe;
    empty_probe.mode = eos_ac::Mode::Probe;
    check(eos_ac::reply_for(empty_probe, incoming, sizeof incoming, out, sizeof out) == 0,
          "an empty probe replies with nothing");

    // verdict_for ---------------------------------------------------------------

    eos_ac::Policy v;
    v.mode = eos_ac::Mode::Off;
    check(eos_ac::verdict_for(v, 0, 0) == eos_ac::Verdict::NoChallenges,
          "no challenges is NoChallenges, whatever the mode");
    check(eos_ac::verdict_for(v, 20, 0) == eos_ac::Verdict::Silent,
          "challenges with no sends is Silent");
    v.mode = eos_ac::Mode::Echo;
    check(eos_ac::verdict_for(v, 20, 20) == eos_ac::Verdict::Echoed,
          "echo with sends is Echoed");
    v.mode = eos_ac::Mode::Probe;
    check(eos_ac::verdict_for(v, 20, 20) == eos_ac::Verdict::Answered,
          "probe with sends is Answered");
    check(eos_ac::verdict_for(v, 20, 0) == eos_ac::Verdict::Silent,
          "challenges with no sends stays Silent even in probe mode");

    check(std::strcmp(eos_ac::verdict_name(eos_ac::Verdict::NoChallenges), "no-challenges") == 0,
          "NoChallenges names itself");
    check(std::strcmp(eos_ac::verdict_name(eos_ac::Verdict::Silent),
                      "challenged-no-response") == 0,
          "Silent names itself");

    // to_hex --------------------------------------------------------------------

    char hex[16] = {};
    check(eos_ac::to_hex(incoming, 4, hex, sizeof hex) == 8, "to_hex writes two chars per byte");
    check(std::strcmp(hex, "01020304") == 0, "to_hex is lowercase and in order");
    check(eos_ac::to_hex(incoming, 4, hex, 8) == 0, "to_hex refuses a buffer without room for NUL");
    check(eos_ac::to_hex(incoming, 0, hex, sizeof hex) == 0, "to_hex of nothing writes nothing");

    // hex_text: the %s argument of every hex log line ---------------------------

    char text[9] = {'z', 'z', 0};
    const char* empty = eos_ac::hex_text(nullptr, 0, 4, text, sizeof text);
    check(empty != nullptr && empty[0] == '\0', "hex_text of no payload is an empty string");
    empty = eos_ac::hex_text(incoming, 0, 4, text, sizeof text);
    check(empty != nullptr && empty[0] == '\0', "hex_text of a zero length is an empty string");
    check(std::strcmp(eos_ac::hex_text(incoming, 4, 2, text, sizeof text), "0102") == 0,
          "hex_text caps the rendered bytes");
    const char* tight = eos_ac::hex_text(incoming, 4, 4, text, 4);
    check(tight != nullptr && tight[0] == '\0', "hex_text with too small a scratch is empty");

    if (failures != 0) {
        std::printf("[!] anticheat_policy: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("[ok] anticheat_policy\n");
    return 0;
}
