// The shim's panel-facing telemetry slot: the tick cadence (the game's frame
// rate measured from outside the agent), the cost of EOS_Platform_Tick, and
// the fault count from the crash probe - plus the few log lines worth seeing
// in the panel: the banner, the sidecar load and the fault lines.
//
// Ring writes happen under the caller's g_state.lock, which the banner, the
// sidecar report and the fault handler already hold; the counter block is
// published from EOS_Platform_Tick only, which is the game's main thread, so
// each structure keeps its single writer.
#pragma once

#include <cstdint>

// Mirror one line into the ring. Caller holds g_state.lock.
void shim_diag_note_locked(const char* fmt, ...);

// Called from EOS_Platform_Tick with the duration of the whole call, pump
// included: publishes at most ~4 Hz, the rest is two counter adds.
void shim_diag_tick(std::uint64_t tick_us, bool pump_wired);

// Called by the crash probe's fault handler, on whatever thread faulted.
void shim_diag_fault();
