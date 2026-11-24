// EOS SDK proxy shim.
//
// Stage D: every export logs its name and returns EOS_Success.
// Stage E1: the calls the observed game trail demands are implemented for real
//           (knowledge/projects/RobRun/evidence/observed-calls): platform handle, interface
//           handles, connect login with an async callback, id token, and the
//           anti-cheat session calls.
//
// Everything is still local: no network, no real identity, no anti-cheat.
//
// Build: build.bat (MSVC) or build_linux.sh (MinGW cross).

#define _CRT_SECURE_NO_WARNINGS  // fopen/strrchr are fine here; no _s variants on MinGW

#include "abi/anticheat.hpp"
#include "abi/eos_result.hpp"
#include "core/boot.hpp"
#include "core/diag.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "perf.hpp"


#define EOS_STUB(name)                       \
    extern "C" std::int32_t name(void) {     \
        return stub(#name);                  \
    }

#include "exports.inc"

// ---------------------------------------------------------------------------
// stage E1: handles and identity
// ---------------------------------------------------------------------------

namespace e1 {

// EOS handles are opaque: the game stores whatever pointer we return and passes
// it back. Any stable non-NULL address is a valid handle.
struct Platform {};
struct Connect {};
struct Auth {};
struct AntiCheatClient {};
struct RTC {};
struct RtcAudio {};

Platform g_platform;
Connect g_connect;
Auth g_auth;
AntiCheatClient g_anticheat;
RTC g_rtc;
RtcAudio g_rtc_audio;

// Product user id must be 32 hex characters; EOS formats them uppercase.
const char kUserId[] = "00000000A1B2C3D4A1B2C3D400000001";

// Stand-in for the JSON web token EOS would issue. Must be a syntactically
// valid three-segment JWS: the game decodes it and reads the claims.
const char kIdToken[] =
    "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjAwMDAwMDAwMDAwMDAwMDAwMDAwMDAw"
    "MDAwMDAwMDAwIn0.eyJzdWIiOiIwMDAwMDAwMGExYjJjM2Q0YTFiMmMzZDQwMDAwMDAwMSIsInBm"
    "c2lkIjoiMDAwMDAwMDBhMWIyYzNkNGExYjJjM2Q0MDAwMDAwMDIiLCJpc3MiOiJodHRwczovL2Fw"
    "aS5lcGljZ2FtZXMuZGV2L2VwaWMvaWQvdjIiLCJhdWQiOiJ4eXphNzg5MW11MGw5ajV0Nmg3Yjhu"
    "OW0wcDFxMnIzcyIsImV4cCI6NDEwMjQ0NDgwMCwiaWF0IjoxNzYzNTAwMDAwLCJqdGkiOiIwMDAw"
    "MDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwIn0.AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

struct Credentials {
    std::int32_t ApiVersion;
    const char* Token;
    std::int32_t Type;
};

struct LoginOptions {
    std::int32_t ApiVersion;
    const Credentials* Credentials;
    const void* UserLoginInfo;
};

struct LoginCallbackInfo {
    std::int32_t ResultCode;
    void* ClientData;
    void* LocalUserId;
    void* ContinuanceToken;
};

using LoginCallback = void (*)(const LoginCallbackInfo*);

struct CopyIdTokenOptions {
    std::int32_t ApiVersion;
    void* LocalUserId;
};

struct IdToken {
    std::int32_t ApiVersion;
    void* ProductUserId;
    const char* JsonWebToken;
};

struct LoginJob {
    void* clientData;
    LoginCallback callback;
};

DWORD WINAPI login_thread(LPVOID param) {
    LoginJob* job = static_cast<LoginJob*>(param);
    Sleep(10);  // never call back inside the API function: games hold locks

    LoginCallbackInfo info{};
    info.ResultCode = 0;  // EOS_Success
    info.ClientData = job->clientData;
    info.LocalUserId = const_cast<char*>(kUserId);
    shim_log("proxy: connect: login callback result=%d user=%s", info.ResultCode, kUserId);
    job->callback(&info);

    delete job;
    return 0;
}

// -- auth types -------------------------------------------------------------

const char kEpicId[] = "00000000DEADBEEF0000000000000001";

struct AuthCredentials {
    std::int32_t ApiVersion;
    const char* Id;
    const char* Token;
    std::int32_t Type;
};

struct AuthLoginOptions {
    std::int32_t ApiVersion;
    const AuthCredentials* Credentials;
    std::int32_t ScopeFlags;
};

// EOS_Auth_LoginCallbackInfo, EOS SDK 1.19.2. The game reads every field, so a
// shorter struct hands it stack bytes for the trailing pointers.
struct AuthLoginCallbackInfo {
    std::int32_t ResultCode;
    void* ClientData;
    void* LocalUserId;
    const void* PinGrantInfo;
    void* ContinuanceToken;
    const void* AccountFeatureRestrictedInfo;
    void* SelectedAccountId;
};

using AuthCallback = void (*)(const AuthLoginCallbackInfo*);

struct CopyUserAuthTokenOptions {
    std::int32_t ApiVersion;
    void* LocalUserId;
};

struct AuthToken {
    std::int32_t ApiVersion;
    const char* App;
    const char* ClientId;
    void* AccountId;
    const char* AccessToken;
    double ExpiresIn;
    const char* ExpiresAt;  // ISO 8601 UTC string, not a number
    std::int32_t AuthType;
    const char* RefreshToken;
    double RefreshExpiresIn;
    const char* RefreshExpiresAt;
};

const char kTokenExpiresAt[] = "2100-01-01T00:00:00.000Z";

struct AuthJob {
    void* clientData;
    AuthCallback callback;
};

DWORD WINAPI auth_thread(LPVOID param) {
    AuthJob* job = static_cast<AuthJob*>(param);
    Sleep(10);

    AuthLoginCallbackInfo info{};
    info.ResultCode = 0;
    info.ClientData = job->clientData;
    info.LocalUserId = const_cast<char*>(kEpicId);
    info.SelectedAccountId = const_cast<char*>(kEpicId);
    shim_log("proxy: auth: login callback result=%d user=%s", info.ResultCode, kEpicId);
    job->callback(&info);

    delete job;
    return 0;
}

}  // namespace e1

extern "C" const char* EOS_GetVersion(void) {
    stub("EOS_GetVersion");
    return "1.19.2.1-58105819";
}

// -- platform ---------------------------------------------------------------

extern "C" void* EOS_Platform_Create(const void*) {
    stub("EOS_Platform_Create");
    return &e1::g_platform;
}

extern "C" std::int32_t EOS_Platform_Tick(void*) {
    // The game's per-frame call. The measured duration is the whole call, and
    // the start time drives the published tick rate.
    const std::uint64_t tick_t0 = PerfWindow::now_us();
    const std::int32_t result = stub("EOS_Platform_Tick");
    if (g_pump) g_pump();
    shim_diag_tick(tick_t0, PerfWindow::since_us(tick_t0), g_pump != nullptr);
    return result;
}

extern "C" void EOS_Platform_Release(void*) {
    stub("EOS_Platform_Release");
}

extern "C" std::int32_t EOS_Shutdown(void) {
    return stub("EOS_Shutdown");
}

#define EOS_INTERFACE_GETTER(name, handle)              \
    extern "C" void* name(void*) {                      \
        stub(#name);                                    \
        return &e1::handle;                             \
    }

EOS_INTERFACE_GETTER(EOS_Platform_GetConnectInterface, g_connect)
EOS_INTERFACE_GETTER(EOS_Platform_GetAuthInterface, g_auth)
// The anti-cheat interface is the one getter that may answer NULL: EOSSHIM_AC=null
// reports that this client has no anti-cheat, which is the only client-side
// state a host can be told apart from "anti-cheat is broken".
extern "C" void* EOS_Platform_GetAntiCheatClientInterface(void*) {
    stub("EOS_Platform_GetAntiCheatClientInterface");
    if (!shim_anticheat_interface_available()) return nullptr;
    return &e1::g_anticheat;
}
EOS_INTERFACE_GETTER(EOS_Platform_GetRTCInterface, g_rtc)
EOS_INTERFACE_GETTER(EOS_RTC_GetAudioInterface, g_rtc_audio)

// -- connect ----------------------------------------------------------------

extern "C" std::int32_t EOS_Connect_Login(void*, const e1::LoginOptions*,
                                          void* clientData, e1::LoginCallback callback) {
    stub("EOS_Connect_Login");
    if (!callback) {
        shim_log("proxy: connect: login refused: no callback result=%d",
                 eos_result::kInvalidParameters);
        return eos_result::kInvalidParameters;
    }
    auto* job = new e1::LoginJob{clientData, callback};
    HANDLE thread = CreateThread(nullptr, 0, e1::login_thread, job, 0, nullptr);
    if (!thread) {
        shim_log("proxy: connect: login thread failed error=%lu", GetLastError());
        delete job;
        return eos_result::kInvalidParameters;
    }
    CloseHandle(thread);
    return eos_result::kSuccess;
}

// EOS_ELoginStatus EOS_Connect_GetLoginStatus(EOS_HConnect, EOS_ProductUserId):
// two arguments, the status is the return value. There is no out pointer.
extern "C" std::int32_t EOS_Connect_GetLoginStatus(void*, void*) {
    stub("EOS_Connect_GetLoginStatus");
    return eos_result::kLoginStatusLoggedIn;
}

// EOS "Copy" APIs return an allocated object through a ** parameter; the game
// releases it later with the matching *_Release. Writing a struct directly into
// the out slot corrupts the caller's pointer.
extern "C" std::int32_t EOS_Connect_CopyIdToken(void*, const e1::CopyIdTokenOptions*,
                                                e1::IdToken** out) {
    stub("EOS_Connect_CopyIdToken");
    if (!out) {
        shim_log("proxy: connect: copy id token refused: no out slot result=%d",
                 eos_result::kInvalidParameters);
        return eos_result::kInvalidParameters;
    }
    auto* token = new e1::IdToken{};
    token->ApiVersion = 1;
    token->ProductUserId = const_cast<char*>(e1::kUserId);
    token->JsonWebToken = e1::kIdToken;
    *out = token;
    return eos_result::kSuccess;
}

extern "C" void EOS_Connect_IdToken_Release(e1::IdToken* token) {
    stub("EOS_Connect_IdToken_Release");
    delete token;
}

extern "C" std::uint64_t EOS_Connect_AddNotifyAuthExpiration(void*, const void*, void*, void*) {
    stub("EOS_Connect_AddNotifyAuthExpiration");
    return 1;
}

// -- auth -------------------------------------------------------------------

extern "C" std::int32_t EOS_Auth_Login(void*, const e1::AuthLoginOptions*,
                                       void* clientData, e1::AuthCallback callback) {
    stub("EOS_Auth_Login");
    if (!callback) {
        shim_log("proxy: auth: login refused: no callback result=%d",
                 eos_result::kInvalidParameters);
        return eos_result::kInvalidParameters;
    }
    auto* job = new e1::AuthJob{clientData, callback};
    HANDLE thread = CreateThread(nullptr, 0, e1::auth_thread, job, 0, nullptr);
    if (!thread) {
        shim_log("proxy: auth: login thread failed error=%lu", GetLastError());
        delete job;
        return eos_result::kInvalidParameters;
    }
    CloseHandle(thread);
    return eos_result::kSuccess;
}

// EOS_ELoginStatus EOS_Auth_GetLoginStatus(EOS_HAuth, EOS_EpicAccountId): the
// status is the return value, like the connect variant.
extern "C" std::int32_t EOS_Auth_GetLoginStatus(void*, void*) {
    stub("EOS_Auth_GetLoginStatus");
    return eos_result::kLoginStatusLoggedIn;
}

extern "C" std::int32_t EOS_Auth_CopyUserAuthToken(void*, const e1::CopyUserAuthTokenOptions*,
                                                   e1::AuthToken** out) {
    stub("EOS_Auth_CopyUserAuthToken");
    if (!out) {
        shim_log("proxy: auth: copy user auth token refused: no out slot result=%d",
                 eos_result::kInvalidParameters);
        return eos_result::kInvalidParameters;
    }
    auto* token = new e1::AuthToken{};
    token->ApiVersion = 1;
    token->App = "RobRun";
    token->ClientId = "emulator";
    token->AccountId = const_cast<char*>(e1::kEpicId);
    token->AccessToken = "emulator-access-token";
    token->ExpiresIn = 3600.0;
    token->ExpiresAt = e1::kTokenExpiresAt;
    token->AuthType = 1;
    token->RefreshToken = "emulator-refresh-token";
    token->RefreshExpiresIn = 86400.0;
    token->RefreshExpiresAt = e1::kTokenExpiresAt;
    *out = token;
    return eos_result::kSuccess;
}

extern "C" void* EOS_Auth_GetLoggedInAccountByIndex(void*, std::int32_t index) {
    stub("EOS_Auth_GetLoggedInAccountByIndex");
    return index == 0 ? const_cast<char*>(e1::kEpicId) : nullptr;
}

extern "C" std::int32_t EOS_Auth_GetLoggedInAccountsCount(void*) {
    return 1;
}

extern "C" void EOS_Auth_Token_Release(e1::AuthToken* token) {
    stub("EOS_Auth_Token_Release");
    delete token;
}

extern "C" void* EOS_EpicAccountId_FromString(const char* value) {
    stub("EOS_EpicAccountId_FromString");
    if (!value || std::strlen(value) != 32) {
        return nullptr;
    }
    return const_cast<char*>(e1::kEpicId);
}

extern "C" std::int32_t EOS_EpicAccountId_ToString(void*, char* buffer, std::int32_t* length) {
    stub("EOS_EpicAccountId_ToString");
    const std::int32_t offered = length ? *length : -1;
    const std::int32_t result =
        eos_result::copy_id_string(e1::kEpicId, sizeof e1::kEpicId, buffer, length);
    if (result != eos_result::kSuccess) {
        shim_log("proxy: ids: epic account to-string result=%d offered=%d need=%zu", result,
                 offered, sizeof e1::kEpicId);
    }
    return result;
}

// -- ids --------------------------------------------------------------------

extern "C" void* EOS_ProductUserId_FromString(const char* value) {
    stub("EOS_ProductUserId_FromString");
    if (!value || std::strlen(value) != 32) {
        return nullptr;
    }
    return const_cast<char*>(e1::kUserId);
}

// EOS_Bool: non-zero means valid. Returning 0 here makes the game discard the
// user id we just handed it in the login callback.
extern "C" std::int32_t EOS_ProductUserId_IsValid(const char* value) {
    return value ? 1 : 0;
}

extern "C" std::int32_t EOS_EpicAccountId_IsValid(const char* value) {
    return value ? 1 : 0;
}

// EOS_Bool: true when the operation finished. Only "will retry" is incomplete.
extern "C" std::int32_t EOS_EResult_IsOperationComplete(std::int32_t result) {
    return eos_result::operation_complete(result) ? 1 : 0;
}

extern "C" std::int32_t EOS_ProductUserId_ToString(void*, char* buffer, std::int32_t* length) {
    stub("EOS_ProductUserId_ToString");
    const std::int32_t offered = length ? *length : -1;
    const std::int32_t result =
        eos_result::copy_id_string(e1::kUserId, sizeof e1::kUserId, buffer, length);
    if (result != eos_result::kSuccess) {
        shim_log("proxy: ids: product user to-string result=%d offered=%d need=%zu", result,
                 offered, sizeof e1::kUserId);
    }
    return result;
}

// -- anti-cheat -------------------------------------------------------------
//
// The session calls live in abi/anticheat.cpp: they carry the message plumbing
// and the EOSSHIM_AC policy. Only the interface getter stays here, with the
// other platform getters.

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

