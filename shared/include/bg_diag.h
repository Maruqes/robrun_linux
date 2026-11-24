/*
 * bg_diag.h - the shim's shared-memory telemetry contract.
 *
 * The shim publishes one slot describing its own work: the EOS_Platform_Tick
 * cadence, the cost of that call, the fault count from the crash probe, and a
 * short ring of recent log lines. A reader opens the section read-only.
 * Plain POD, fixed layout, x64 only.
 *
 * Two structures inside one slot, with two different safety rules:
 *
 *   - the counter block is covered by a seqlock: seq goes odd before a
 *     publish and even after it, and a reader retries on either mismatch;
 *   - the log ring is covered line by line: text is written first and the
 *     line's own seq word last, so on x86 a reader never pairs new text with
 *     an old seq or the reverse. The ring is a bounded tail, not a queue.
 *
 * Counters are lifetime monotonic, so a reader derives per-second rates from
 * the delta between two polls and merges windows instead of losing data.
 */
#ifndef BG_DIAG_H
#define BG_DIAG_H

#include <stdint.h>

#define BG_DIAG_MAGIC 0x42474447u /* "BGDG" */
#define BG_DIAG_VERSION 2u

#define BG_DIAG_SECTION_NAME "Local\\EOSShim.v1.diag"

/* Fixed slot order. The shim owns slot 3; slots 0-2 are part of the fixed
   layout and are left unused by this build. */
enum {
    BG_DIAG_SHIM = 3
};
#define BG_DIAG_PRODUCERS 4

/* Same width as PerfWindow buckets. */
#define BG_DIAG_BUCKETS 8

/* Log ring: one record per line, 160 bytes: seq, pad, wall_ms, text. Longer
   lines are truncated; the log file keeps the full line. */
#define BG_DIAG_LOG_LINES 48
#define BG_DIAG_LOG_TEXT 144

/* Per-thread attribution rows; single-threaded writers leave them zeroed.
   tid 0 marks an unused row. */
typedef struct {
    uint64_t tid;
    uint64_t packets; /* units observed on this thread */
    uint64_t us;       /* time on this thread */
} bg_diag_thread;

#define BG_DIAG_THREADS 8

/* slot.state: the producer's main state, one small number.
   Shim: 0 plain, 1 with the optional frame hook wired. */

/* One ring record. The text is written first and seq last, so a reader
   either sees the old whole line or the new whole line, never a splice.
   wall_ms is UTC milliseconds so lines sort by real time. */
typedef struct {
    uint32_t seq;     /* 0 until the text below is complete */
    uint32_t pad;
    uint64_t wall_ms; /* UTC, ms since epoch */
    char text[BG_DIAG_LOG_TEXT]; /* NUL terminated */
} bg_diag_line;

typedef struct {
    uint32_t seq;    /* odd while the counter block is being written */
    uint32_t producer; /* BG_DIAG_* */
    uint32_t pid;
    /* The shim's loop rate, smoothed: the game's main-thread rate measured
       from the EOS_Platform_Tick cadence. */
    uint32_t hz;
    uint32_t flags;  /* bit 0: the optional frame hook is wired */
    uint32_t state;  /* see the comment above */
    uint64_t mono_ms; /* GetTickCount64 at the moment of this publish */
    /* Monotonic counters: EOS_Platform_Tick calls, faults caught by the crash
       probe, milliseconds spent in the tick handler. */
    uint64_t counters[6];
/* Lifetime bucket totals, the monotonic side of PerfWindow. ms/s per
       bucket = delta of bucket_us between polls; avg per call = delta of
       bucket_us over delta of bucket_count. */
    uint64_t bucket_count[BG_DIAG_BUCKETS];
    uint64_t bucket_us[BG_DIAG_BUCKETS];
    uint64_t bucket_max_us[BG_DIAG_BUCKETS];
    /* Per-thread attribution rows. */
    bg_diag_thread threads[BG_DIAG_THREADS];
    /* Log ring: newest at (log_seq - 1) % BG_DIAG_LOG_LINES. A line's seq is
       written after its text, so seq 0 means never written and any other
       value means the whole line is readable. */
    uint32_t log_head; /* total lines pushed; slot ring index = head % LINES */
    uint32_t log_seq;  /* the seq of the newest line */
    bg_diag_line log[BG_DIAG_LOG_LINES];
} bg_diag_slot;

typedef struct {
    uint32_t magic;
    uint32_t version;
    bg_diag_slot slot[BG_DIAG_PRODUCERS];
} bg_diag;

#endif /* BG_DIAG_H */
