// EOS anti-cheat client surface.
//
// The game registers a MessageToServer callback, begins a session and then
// hands every server message to ReceiveMessageFromServer. Historically this
// file did nothing with any of it: the calls succeeded, the server's messages
// were dropped, and the session stayed silent.
//
// That silence is what a server running the anti-cheat half punishes. This
// translation unit keeps the surface honest and, more importantly, makes it
// observable: every anti-cheat message in both directions is written to
// eos-shim.log as hex, so the protocol can be read from a capture instead of
// guessed at.
//
// EOSSHIM_AC selects the behaviour (see knowledge/projects/RobRun/operations/
// troubleshooting):
//   off        succeed and stay silent - the historical behaviour, the default
//   null       BeginSession answers EOS_AntiCheat_ClientProtectionNotAvailable
//              (0x2EE0), the same answer a client without an anti-cheat module
//              gives; nothing is sent
//   echo       send every server message back unchanged
//   probe:HEX  send HEX to the server on BeginSession and after every server
//              message
//
// Every message is logged regardless of the mode, so `off` is a capture mode.

#include "abi/anticheat.hpp"
#include "abi/anticheat_options.hpp"
#include "abi/anticheat_policy.hpp"
#include "abi/eos_result.hpp"
#include "core/boot.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace {

// EOS_AntiCheatClient_BeginSessionOptions, EOS SDK 1.19.2.
struct BeginSessionOptions {
    std::int32_t ApiVersion;
    void* LocalUserId;
    std::int32_t Mode;
};

// EOS_AntiCheatClient_OnMessageToServerCallbackInfo. Callback info structs in
// EOS carry no ApiVersion; ClientData is first.
struct MessageToServerInfo {
    void* ClientData;
    const void* MessageData;
    std::uint32_t MessageDataSizeBytes;
};

using MessageToServerCallback = void (*)(const MessageToServerInfo*);

inline constexpr std::uint64_t kNotificationMessageToServer = 2;
inline constexpr std::uint32_t kMaxLoggedBytes = 160;

struct State {
    std::mutex lock;
    eos_ac::Policy policy;
    MessageToServerCallback to_server = nullptr;
    void* to_server_client_data = nullptr;
    bool session = false;
    std::int32_t session_mode = 0;
    unsigned long long received = 0;
    unsigned long long sent = 0;
    unsigned long long began_ms = 0;
    unsigned long long first_challenge_ms = 0;
    bool warned_enforcing = false;
};

State g_state;

const char* policy_name(eos_ac::Mode mode) {
    switch (mode) {
        case eos_ac::Mode::Off: return "off";
        case eos_ac::Mode::Null: return "null";
        case eos_ac::Mode::NoIface: return "noiface";
        case eos_ac::Mode::Echo: return "echo";
        case eos_ac::Mode::Probe: return "probe";
    }
    return "off";
}

// EOSSHIM_AC wins; without it, a one-line file next to the DLL selects the
// mode. Steam launches the game from its own client, so the environment is not
// available on a Proton install; the file is.
bool read_mode_file(char* out, std::size_t capacity) {
    char module[MAX_PATH] = {};
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&g_state), &self) ||
        !GetModuleFileNameA(self, module, MAX_PATH)) {
        return false;
    }
    char* slash = std::strrchr(module, '\\');
    if (!slash) return false;
    slash[1] = '\0';

    std::string path = std::string(module) + "eosshim-ac.txt";
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) return false;
    const std::size_t read = std::fread(out, 1, capacity - 1, file);
    std::fclose(file);
    out[read] = '\0';
    for (std::size_t i = 0; i < read; ++i) {
        if (out[i] == '\r' || out[i] == '\n' || out[i] == ' ' || out[i] == '\t') {
            out[i] = '\0';
            break;
        }
    }
    return out[0] != '\0';
}

BOOL CALLBACK load_policy(PINIT_ONCE, PVOID, PVOID*) {
    char text[256] = {};
    const DWORD env_len = GetEnvironmentVariableA("EOSSHIM_AC", text, sizeof text);
    if (env_len >= sizeof text) {
        shim_log("proxy: anticheat: EOSSHIM_AC ignored: %lu chars exceed %zu, keeping default %s",
                 static_cast<unsigned long>(env_len), sizeof text - 1,
                 policy_name(g_state.policy.mode));
        return TRUE;
    }
    if (!env_len && !read_mode_file(text, sizeof text)) {
        shim_log("proxy: anticheat: policy=%s (default) probe_len=0",
                 policy_name(g_state.policy.mode));
        return TRUE;
    }

    eos_ac::Policy parsed;
    if (eos_ac::parse_policy(text, &parsed)) {
        g_state.policy = parsed;
        shim_log("proxy: anticheat: policy=%s probe_len=%zu", policy_name(parsed.mode),
                 parsed.probe_len);
        return TRUE;
    }
    shim_log("proxy: anticheat: EOSSHIM_AC=%s is not understood, keeping default %s", text,
             policy_name(g_state.policy.mode));
    return TRUE;
}

INIT_ONCE g_policy_once = INIT_ONCE_STATIC_INIT;

void policy() { InitOnceExecuteOnce(&g_policy_once, load_policy, nullptr, nullptr); }

const char* hex_of(const std::uint8_t* data, std::size_t len, char* scratch,
                   std::size_t scratch_len) {
    return eos_ac::hex_text(data, len, kMaxLoggedBytes, scratch, scratch_len);
}

// Hands one message to the callback the game registered with
// AddNotifyMessageToServer. Must run on the thread the game called us from:
// the callback goes straight back into the game's network send path. The
// callback snapshot is taken under the lock and invoked without it, because
// the game is allowed to re-enter the shim from inside the callback.
void send_to_server(const std::uint8_t* data, std::size_t len) {
    MessageToServerCallback callback = nullptr;
    void* client_data = nullptr;
    {
        std::lock_guard<std::mutex> guard(g_state.lock);
        callback = g_state.to_server;
        client_data = g_state.to_server_client_data;
        if (callback) ++g_state.sent;
    }
    if (!callback) {
        shim_log("proxy: anticheat: send dropped len=%zu: no MessageToServer handler", len);
        return;
    }

    MessageToServerInfo info{};
    info.ClientData = client_data;
    info.MessageData = data;
    info.MessageDataSizeBytes = static_cast<std::uint32_t>(len);
    callback(&info);
}

}  // namespace

extern "C" std::int32_t shim_anticheat_interface_available() {
    policy();
    return eos_ac::interface_available(g_state.policy) ? 1 : 0;
}

extern "C" std::int32_t EOS_AntiCheatClient_BeginSession(void*,
                                                         const BeginSessionOptions* options) {
    stub("EOS_AntiCheatClient_BeginSession");
    policy();

    const std::int32_t api = options ? options->ApiVersion : 0;
    const std::int32_t mode = options ? options->Mode : 0;

    {
        std::lock_guard<std::mutex> guard(g_state.lock);
        g_state.session = true;
        g_state.session_mode = mode;
        g_state.received = 0;
        g_state.sent = 0;
        g_state.began_ms = GetTickCount64();
        g_state.first_challenge_ms = 0;
        g_state.warned_enforcing = false;
    }

    shim_log("proxy: anticheat: begin api=%d mode=%d", api, mode);

    if (g_state.policy.mode == eos_ac::Mode::Null) {
        shim_log("proxy: anticheat: begin refused result=0x%x (ClientProtectionNotAvailable)",
                 eos_result::kClientProtectionNotAvailable);
        return eos_result::kClientProtectionNotAvailable;
    }

    if (g_state.policy.mode == eos_ac::Mode::Probe && g_state.policy.probe_len) {
        char scratch[kMaxLoggedBytes * 2 + 1] = {};
        const auto hex = hex_of(g_state.policy.probe, g_state.policy.probe_len, scratch,
                                sizeof scratch);
        shim_log("proxy: anticheat: send begin probe len=%zu data=%s",
                 g_state.policy.probe_len, hex);
        send_to_server(g_state.policy.probe, g_state.policy.probe_len);
    }

    return eos_result::kSuccess;
}

extern "C" std::int32_t EOS_AntiCheatClient_EndSession(void*) {
    stub("EOS_AntiCheatClient_EndSession");

    std::lock_guard<std::mutex> guard(g_state.lock);

    const unsigned long long now = GetTickCount64();
    const unsigned long long lasted = g_state.began_ms ? now - g_state.began_ms : 0;
    const unsigned long long first = g_state.first_challenge_ms && g_state.began_ms
                                         ? g_state.first_challenge_ms - g_state.began_ms
                                         : 0;
    const eos_ac::Verdict verdict =
        eos_ac::verdict_for(g_state.policy, g_state.received, g_state.sent);

    shim_log("proxy: anticheat: end mode=%d received=%llu sent=%llu", g_state.session_mode,
             g_state.received, g_state.sent);
    shim_log("proxy: anticheat: summary policy=%s lasted_ms=%llu challenges=%llu sends=%llu "
             "first_challenge_ms=%llu verdict=%s",
             policy_name(g_state.policy.mode), lasted, g_state.received, g_state.sent, first,
             eos_ac::verdict_name(verdict));
    if (verdict == eos_ac::Verdict::Silent) {
        shim_log("proxy: anticheat: this host ran the anti-cheat server half; the client never "
                 "answered, so it was disconnected with 403/E-0503");
        shim_log("proxy: anticheat: FIX: delete eosshim-ac.txt next to the DLL (default is null) "
                 "or write null into it, then rejoin");
    }

    g_state.session = false;
    g_state.session_mode = 0;
    g_state.began_ms = 0;
    g_state.first_challenge_ms = 0;
    g_state.warned_enforcing = false;
    return eos_result::kSuccess;
}

extern "C" std::uint64_t EOS_AntiCheatClient_AddNotifyMessageToServer(
    void*, const void*, void* client_data, MessageToServerCallback callback) {
    stub("EOS_AntiCheatClient_AddNotifyMessageToServer");

    std::lock_guard<std::mutex> guard(g_state.lock);
    g_state.to_server = callback;
    g_state.to_server_client_data = client_data;
    shim_log("proxy: anticheat: handler=%p client_data=%p", reinterpret_cast<void*>(callback),
             client_data);
    return kNotificationMessageToServer;
}

extern "C" std::int32_t EOS_AntiCheatClient_RemoveNotifyMessageToServer(void*,
                                                                       std::uint64_t id) {
    stub("EOS_AntiCheatClient_RemoveNotifyMessageToServer");

    std::lock_guard<std::mutex> guard(g_state.lock);
    shim_log("proxy: anticheat: handler removed id=%llu", id);
    g_state.to_server = nullptr;
    g_state.to_server_client_data = nullptr;
    return eos_result::kSuccess;
}

extern "C" std::int32_t EOS_AntiCheatClient_ReceiveMessageFromServer(void*,
                                                                     const void* raw_options) {
    policy();
    if (!raw_options) {
        shim_log("proxy: anticheat: recv dropped: no options result=%d",
                 eos_result::kInvalidParameters);
        return eos_result::kInvalidParameters;
    }

    const auto* options =
        static_cast<const eos_ac::ReceiveMessageFromServerOptions*>(raw_options);
    const std::uint32_t len = options->DataLengthBytes;
    const void* data = options->Data;

    unsigned long long received = 0;
    unsigned long long sent = 0;
    unsigned long long since_begin_ms = 0;
    bool first_challenge = false;
    bool warn_enforcing = false;
    {
        std::lock_guard<std::mutex> guard(g_state.lock);
        received = ++g_state.received;
        sent = g_state.sent + 1;
        if (g_state.received == 1 && g_state.began_ms) {
            g_state.first_challenge_ms = GetTickCount64();
            since_begin_ms = g_state.first_challenge_ms - g_state.began_ms;
            first_challenge = true;
            warn_enforcing =
                g_state.policy.mode == eos_ac::Mode::Off && !g_state.warned_enforcing;
            g_state.warned_enforcing = g_state.warned_enforcing || warn_enforcing;
        }
    }

    if (first_challenge) {
        shim_log("proxy: anticheat: first challenge %llu ms after begin", since_begin_ms);
        if (warn_enforcing) {
            shim_log("proxy: anticheat: ENFORCING HOST: this host runs the anti-cheat server half");
            shim_log("proxy: anticheat: policy=off sends nothing, so it will disconnect us with "
                     "403/E-0503 in about a minute");
            shim_log("proxy: anticheat: FIX: delete eosshim-ac.txt (default is null) or write "
                     "null into it, then rejoin");
        }
    }

    char scratch[kMaxLoggedBytes * 2 + 1] = {};
    const auto hex = hex_of(static_cast<const std::uint8_t*>(data), len, scratch,
                            sizeof scratch);

    shim_log("proxy: anticheat: recv n=%llu api=%d len=%u data=%s%s", received,
             options->ApiVersion, len, hex, len > kMaxLoggedBytes ? " (truncated)" : "");

    if (!data || len == 0) {
        shim_log("proxy: anticheat: recv ignored: empty payload");
        return eos_result::kSuccess;
    }

    std::uint8_t reply[512];
    const std::size_t reply_len =
        eos_ac::reply_for(g_state.policy, static_cast<const std::uint8_t*>(data), len, reply,
                          sizeof reply);
    if (reply_len == 0) return eos_result::kSuccess;

    char reply_hex[kMaxLoggedBytes * 2 + 1] = {};
    const auto text = hex_of(reply, reply_len, reply_hex, sizeof reply_hex);
    shim_log("proxy: anticheat: send n=%llu len=%zu data=%s", sent, reply_len, text);
    send_to_server(reply, reply_len);
    return eos_result::kSuccess;
}
