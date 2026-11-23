// What the ABI translation unit needs from the boot translation unit.
#pragma once

#include "core/win32.hpp"

// Resolved from the sidecar at load and called from EOS_Platform_Tick on the
// game's main thread. Null when better_game.dll is absent.
extern void (*g_pump)();

// Logs the call and returns EOS_Success (0). Every unimplemented export is a
// jump to this, which is why the whole surface can exist without 600 bodies.
int stub(const char* name);

// Appends one formatted line to the shim log, next to the `stub` lines and
// under the same EOSSHIM_LOG switch. Does nothing while logging is off.
void shim_log(const char* fmt, ...);
