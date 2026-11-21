// EOS SDK proxy shim.
//
// Every export logs its name and returns EOS_Success, except the handful of
// calls the game actually uses: platform handle, interface handles, connect
// login with an async callback, id token, and the anti-cheat client session.
//
// Everything is local: no network and no real identity.
//
// Build: build.bat (MSVC), build_mingw.bat or build_linux.sh (MinGW).

#define _CRT_SECURE_NO_WARNINGS  // fopen/strrchr are fine here; no _s variants on MinGW

#include "core/boot.hpp"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

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

struct AuthLoginCallbackInfo {
    std::int32_t ResultCode;
    void* ClientData;
    void* LocalUserId;
    void* ContinuanceToken;
};

using AuthCallback = void (*)(const AuthLoginCallbackInfo*);

struct GetLoginStatusOptions {
    std::int32_t ApiVersion;
    void* LocalUserId;
};

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
    double ExpiresAt;
    std::int32_t AuthType;
    const char* RefreshToken;
    double RefreshExpiresIn;
    double RefreshExpiresAt;
};

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
    return stub("EOS_Platform_Tick");
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
EOS_INTERFACE_GETTER(EOS_Platform_GetAntiCheatClientInterface, g_anticheat)
EOS_INTERFACE_GETTER(EOS_Platform_GetRTCInterface, g_rtc)
EOS_INTERFACE_GETTER(EOS_RTC_GetAudioInterface, g_rtc_audio)

// -- connect ----------------------------------------------------------------

extern "C" std::int32_t EOS_Connect_Login(void*, const e1::LoginOptions*,
                                          void* clientData, e1::LoginCallback callback) {
    stub("EOS_Connect_Login");
    if (!callback) {
        return 1;  // EOS_NoConnection would be wrong; invalid parameters
    }
    auto* job = new e1::LoginJob{clientData, callback};
    if (!CreateThread(nullptr, 0, e1::login_thread, job, 0, nullptr)) {
        delete job;
        return 1;
    }
    return 0;
}

extern "C" std::int32_t EOS_Connect_GetLoginStatus(void*, const void*, std::int32_t* status) {
    stub("EOS_Connect_GetLoginStatus");
    if (status) {
        *status = 2;  // EOS_ELoginStatus_LoggedIn
    }
    return 0;
}

// EOS "Copy" APIs return an allocated object through a ** parameter; the game
// releases it later with the matching *_Release. Writing a struct directly into
// the out slot corrupts the caller's pointer.
extern "C" std::int32_t EOS_Connect_CopyIdToken(void*, const e1::CopyIdTokenOptions*,
                                                e1::IdToken** out) {
    stub("EOS_Connect_CopyIdToken");
    if (!out) {
        return 1;
    }
    auto* token = new e1::IdToken{};
    token->ApiVersion = 1;
    token->ProductUserId = const_cast<char*>(e1::kUserId);
    token->JsonWebToken = e1::kIdToken;
    *out = token;
    return 0;
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
        return 1;
    }
    auto* job = new e1::AuthJob{clientData, callback};
    if (!CreateThread(nullptr, 0, e1::auth_thread, job, 0, nullptr)) {
        delete job;
        return 1;
    }
    return 0;
}

extern "C" std::int32_t EOS_Auth_GetLoginStatus(void*, const e1::GetLoginStatusOptions*,
                                                 std::int32_t* status) {
    stub("EOS_Auth_GetLoginStatus");
    if (status) {
        *status = 2;  // EOS_ELoginStatus_LoggedIn
    }
    return 0;
}

extern "C" std::int32_t EOS_Auth_CopyUserAuthToken(void*, const e1::CopyUserAuthTokenOptions*,
                                                   e1::AuthToken** out) {
    stub("EOS_Auth_CopyUserAuthToken");
    if (!out) {
        return 1;
    }
    auto* token = new e1::AuthToken{};
    token->ApiVersion = 1;
    token->App = "RobRun";
    token->ClientId = "proxy";
    token->AccountId = const_cast<char*>(e1::kEpicId);
    token->AccessToken = "proxy-access-token";
    token->ExpiresIn = 3600.0;
    token->ExpiresAt = 4102444800.0;  // 2100-01-01
    token->AuthType = 1;
    token->RefreshToken = "proxy-refresh-token";
    token->RefreshExpiresIn = 86400.0;
    token->RefreshExpiresAt = 4102444800.0;
    *out = token;
    return 0;
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
    const std::int32_t need = static_cast<std::int32_t>(sizeof e1::kEpicId);
    if (!buffer || !length) {
        return 1;
    }
    if (*length < need) {
        *length = need;
        return 1;
    }
    std::memcpy(buffer, e1::kEpicId, need);
    *length = need;
    return 0;
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
    return result == 8 /* EOS_OperationWillRetry */ ? 0 : 1;
}

extern "C" std::int32_t EOS_ProductUserId_ToString(void*, char* buffer, std::int32_t* length) {
    stub("EOS_ProductUserId_ToString");
    const std::int32_t need = static_cast<std::int32_t>(sizeof e1::kUserId);  // includes NUL
    if (!buffer || !length) {
        return 1;
    }
    if (*length < need) {
        *length = need;
        return 1;  // EOS_InvalidParameters
    }
    std::memcpy(buffer, e1::kUserId, need);
    *length = need;
    return 0;
}

// -- anti-cheat -------------------------------------------------------------

extern "C" std::int32_t EOS_AntiCheatClient_BeginSession(void*, const void*) {
    return stub("EOS_AntiCheatClient_BeginSession");
}

extern "C" std::int32_t EOS_AntiCheatClient_EndSession(void*, const void*) {
    return stub("EOS_AntiCheatClient_EndSession");
}

extern "C" std::uint64_t EOS_AntiCheatClient_AddNotifyMessageToServer(void*, const void*,
                                                                     void*, void*) {
    stub("EOS_AntiCheatClient_AddNotifyMessageToServer");
    return 2;
}

extern "C" std::int32_t EOS_AntiCheatClient_RemoveNotifyMessageToServer(void*, std::uint64_t) {
    return stub("EOS_AntiCheatClient_RemoveNotifyMessageToServer");
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

