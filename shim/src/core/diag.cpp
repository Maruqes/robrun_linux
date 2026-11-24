#include "core/diag.hpp"

#include "core/win32.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bg_diag.h"
#include "diag_slot.hpp"
#include "core/tick_rate.hpp"
#include "perf.hpp"

namespace {

bg_diag* g_diag = nullptr;
HANDLE g_diag_map = nullptr;

// Ticks and their cost, lifetime monotonic; the slot publishes the deltas.
std::atomic<std::uint64_t> g_ticks{0};
std::atomic<std::uint64_t> g_faults{0};
std::uint64_t g_tick_us = 0;
std::uint64_t g_tick_max_us = 0;
std::uint64_t g_publish_ms = 0;
TickRate g_rate;

void open() {
    if (g_diag) return;
    g_diag_map = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                    sizeof(bg_diag), BG_DIAG_SECTION_NAME);
    if (!g_diag_map) return;
    g_diag = static_cast<bg_diag*>(
        MapViewOfFile(g_diag_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(bg_diag)));
    if (!g_diag) {
        CloseHandle(g_diag_map);
        g_diag_map = nullptr;
        return;
    }
    if (g_diag->magic != BG_DIAG_MAGIC) {
        g_diag->magic = BG_DIAG_MAGIC;
        g_diag->version = BG_DIAG_VERSION;
    }
}

std::uint64_t wall_ms() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    return ((static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) / 10000;
}

}  // namespace

void shim_diag_note_locked(const char* fmt, ...) {
    open();
    if (!g_diag) return;
    char text[BG_DIAG_LOG_TEXT];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    diag_line_push(g_diag->slot[BG_DIAG_SHIM], wall_ms(), text);
}

void shim_diag_tick(std::uint64_t start_us, std::uint64_t tick_us, bool pump_wired) {
    // The cadence runs on every call so the rate stays honest; the publish is
    // throttled. The rate follows the start times, the cost the durations.
    g_rate.observe(start_us);

    g_ticks.fetch_add(1, std::memory_order_relaxed);
    g_tick_us += tick_us;
    if (tick_us > g_tick_max_us) g_tick_max_us = tick_us;

    open();
    if (!g_diag) return;
    const std::uint64_t now = GetTickCount64();
    if (g_publish_ms && now - g_publish_ms < 250) return;
    g_publish_ms = now;

    bg_diag_slot& s = g_diag->slot[BG_DIAG_SHIM];
    diag_slot_begin(s);
    s.producer = BG_DIAG_SHIM;
    s.pid = GetCurrentProcessId();
    s.hz = static_cast<std::uint32_t>(g_rate.hz() + 0.5);
    s.flags = pump_wired ? 1u : 0u;
    s.state = pump_wired ? 1u : 0u;
    s.mono_ms = now;
    s.counters[0] = g_ticks.load(std::memory_order_relaxed);
    s.counters[1] = g_faults.load(std::memory_order_relaxed);
    s.counters[2] = g_tick_us / 1000;
    s.bucket_count[0] = g_ticks.load(std::memory_order_relaxed);
    s.bucket_us[0] = g_tick_us;
    s.bucket_max_us[0] = g_tick_max_us;
    diag_slot_end(s);
}

// The fault handler cannot reach this TU's anonymous namespace; the counter is
// declared here and incremented by boot.cpp through this free function.
void shim_diag_fault() { g_faults.fetch_add(1, std::memory_order_relaxed); }
