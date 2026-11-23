// The ABI translation unit's view of the anti-cheat module.
#pragma once

#include <cstdint>

// Non-zero when the shim should hand the game an anti-cheat client interface.
// Zero in `null` mode: the game then sees no client at all, which is the state
// the vendor's own Linux build reports.
extern "C" std::int32_t shim_anticheat_interface_available();
