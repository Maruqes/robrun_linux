// Anti-cheat client policy: what the shim answers when the game hands it an
// anti-cheat message for the server.
//
// Platform-free on purpose: the host suite links this file, so it must not
// include any Windows header. The Windows ABI lives in anticheat.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace eos_ac {

// How the shim behaves on the anti-cheat client surface. `Off` reproduces the
// historical behaviour exactly: every call succeeds and no message is sent.
enum class Mode {
    Off,
    Null,     // interface present, BeginSession refused: the game keeps running
    NoIface,  // no client interface at all: this build asserts, kept for evidence
    Echo,
    Probe,
};

// Largest probe payload accepted from EOSSHIM_AC=probe:<hex>.
inline constexpr std::size_t kProbeMax = 96;

struct Policy {
    // Refusing the session is the working behaviour on this build: the host then
    // never enrols the client in the anti-cheat challenge. `off` restores the
    // historical success-and-silence only for a capture run.
    Mode mode = Mode::Null;
    std::uint8_t probe[kProbeMax] = {};
    std::size_t probe_len = 0;
};

// Parses the EOSSHIM_AC value. Accepts "off", "null", "noiface", "echo" and
// "probe:<hex>" (hex may be empty, which sends nothing after begin but is
// still logged).
// Returns false and leaves `out` untouched when the text is not understood.
bool parse_policy(const char* text, Policy* out);

// Returns the message the shim should hand to the game's MessageToServer
// callback for one received server message. Writes at most `cap` bytes into
// `out` and returns the length, or 0 when nothing should be sent.
std::size_t reply_for(const Policy& policy, const std::uint8_t* in, std::size_t in_len,
                      std::uint8_t* out, std::size_t cap);

// What one anti-cheat session amounted to, from the client side. Printed in the
// summary line so a captured log answers "did the host challenge us, and did we
// answer" without reading the whole file.
enum class Verdict {
    NoChallenges,   // the host never ran the anti-cheat server half
    Silent,         // challenged and never answered: the host will disconnect
    Echoed,         // challenged and every message was echoed back
    Answered,       // challenged and the configured probe was sent
};

Verdict verdict_for(const Policy& policy, std::size_t challenges, std::size_t sends);
const char* verdict_name(Verdict verdict);

// Renders `len` bytes as lowercase hex into `out`, NUL-terminated. Returns the
// number of characters written (without the NUL), or 0 when `cap` is too small.
std::size_t to_hex(const std::uint8_t* data, std::size_t len, char* out, std::size_t cap);

// False only for `noiface`: the shim then reports no anti-cheat client
// interface at all. This build of the game asserts on null, so `noiface` is a
// documented dead end rather than a mode to run.
bool interface_available(const Policy& policy);

}  // namespace eos_ac
