// The shim's telemetry slot: the tick cadence (the game's frame rate measured
// at the EOS_Platform_Tick boundary), the cost of that call, and the fault
// count from the crash probe - plus the few log lines worth mirroring there:
// the banner and the fault lines.
//
// Ring writes happen under the caller's g_state.lock, which the banner and the
// fault handler already hold; the counter block is published from
// EOS_Platform_Tick only, which is the game's main thread, so each structure
// keeps its single writer.
#pragma once

#include <cstdint>

// Mirror one line into the ring. Caller holds g_state.lock.
void shim_diag_note_locked(const char* fmt, ...);

// Called from EOS_Platform_Tick with the call's monotonic start time and the
// duration of the whole call, pump included: publishes at most ~4 Hz.
void shim_diag_tick(std::uint64_t start_us, std::uint64_t tick_us, bool pump_wired);

// Called by the crash probe's fault handler, on whatever thread faulted.
void shim_diag_fault();
