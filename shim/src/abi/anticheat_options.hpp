// EOS_AntiCheatClient_ReceiveMessageFromServerOptions, EOS SDK 1.19.2:
// ApiVersion, DataLengthBytes, Data.
//
// Platform-free on purpose: the host suite checks the field offsets, so this
// file must not include a Windows header.
#pragma once

#include <cstdint>

namespace eos_ac {

struct ReceiveMessageFromServerOptions {
    std::int32_t ApiVersion;
    std::uint32_t DataLengthBytes;
    const void* Data;
};

}  // namespace eos_ac
