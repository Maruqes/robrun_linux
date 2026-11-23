// diag.hpp - single-writer/single-reader helpers over the bg_diag.h contract.
//
// Every slot has exactly one writer process, so the counter block needs only
// a seqlock, and the log ring needs only text-before-seq store ordering.
// Platform-free on purpose: the host suite drives both sides of every rule
// here, and the producers only add their clocks.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bg_diag.h"
#include "bg_ipc.h"

// ---- counter block ---------------------------------------------------------

// One publish: seq odd, fill the plain fields, end. The fences keep the
// compiler from sinking field stores past the seq store.
inline void diag_slot_begin(bg_diag_slot& s) {
    s.seq = (s.seq & ~1u) + 1u;  // odd: a publish is in progress
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

inline void diag_slot_end(bg_diag_slot& s) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    s.seq = (s.seq & ~1u) + 2u;  // even: the block is complete
}

// Copy the counter block out (everything before the ring). False when the
// producer has never published, the slot is mid-publish, or the publish kept
// colliding with this read; the panel just tries again next poll.
inline bool diag_slot_read(const bg_diag_slot& s, bg_diag_slot& out) {
    const std::uint32_t before = s.seq;
    if (before == 0 || (before & 1u)) return false;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::memcpy(&out, &s, offsetof(bg_diag_slot, log));
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return s.seq == before;
}

// ---- log ring ---------------------------------------------------------------

// Append one line to the slot's ring, overwriting the oldest when full. The
// text goes in first and the record's seq last, so a concurrent reader sees
// the old line whole or the new line whole (x86 keeps stores in order); the
// fences make that ordering explicit to the compiler instead of assumed.
inline void diag_line_push(bg_diag_slot& s, std::uint64_t wall_ms, const char* text) {
    if (!text) return;
    bg_diag_line& rec = s.log[s.log_head % BG_DIAG_LOG_LINES];
    std::size_t len = 0;
    while (len + 1 < sizeof rec.text && text[len]) {
        rec.text[len] = text[len];
        ++len;
    }
    rec.text[len] = '\0';
    rec.wall_ms = wall_ms;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    rec.seq = ++s.log_seq;
    ++s.log_head;
}

// ---- the agent's consumer gate ----------------------------------------------

// True when someone still wants the agent's frames: the ESP boxes or the aim
// assist. With neither, reading the world is money spent on nothing, so the
// pump returns before its first Unity call. Fail-open like the master switch:
// no block, or a block another build wrote, means no opinion and work runs.
inline bool bg_consumers_on(const bg_control* c) {
    return !c || c->magic != BG_IPC_MAGIC || c->enabled != 0 || c->aim != 0;
}
