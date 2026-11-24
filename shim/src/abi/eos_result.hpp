// EOS 1.19.2 result codes and the id-to-string copy contract the shim's
// hand-written exports share.
//
// Platform-free on purpose: the host suite links this file, so it must not
// include any Windows header. The exports themselves live in eos.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace eos_result {

// EOS_EResult values, eos_result.h of SDK 1.19.2.
inline constexpr std::int32_t kSuccess = 0;
inline constexpr std::int32_t kInvalidParameters = 10;
inline constexpr std::int32_t kOperationWillRetry = 19;
inline constexpr std::int32_t kLimitExceeded = 22;

// EOS_ELoginStatus_LoggedIn.
inline constexpr std::int32_t kLoginStatusLoggedIn = 2;

// EOS_AntiCheat_ClientProtectionNotAvailable: BeginSession answered by a client
// with no anti-cheat module. The value is a packed EOS_EResult.
inline constexpr std::int32_t kClientProtectionNotAvailable = 0x2EE0;

// EOS_EResult_IsOperationComplete: every result is final except
// EOS_OperationWillRetry.
bool operation_complete(std::int32_t result);

// The EOS_*Id_ToString contract. `id_size` counts the NUL. Null buffer or
// length -> InvalidParameters; a buffer shorter than `id_size` -> LimitExceeded
// with *length set to the size needed; otherwise copies the id with its NUL,
// sets *length to `id_size` and returns Success.
std::int32_t copy_id_string(const char* id, std::size_t id_size, char* buffer,
                            std::int32_t* length);

}  // namespace eos_result
