/*
 * bg_diag.h - shared-memory observability contract between the three
 *             producers (better_game.dll, view.exe, enet.dll) and panel.exe.
 *
 * The panel answers two questions with this section: "what is the game's
 * frame rate right now" and "where do our milliseconds go". Each producer
 * owns exactly one slot and is its only writer; the panel is read-only.
 * Plain POD, fixed layout, x64 only, like bg_ipc.h.
 *
 * Two structures inside one slot, with two different safety rules:
 *
 *   - the counter block is covered by a seqlock: seq goes odd before a
 *     publish and even after it, and a reader retries on either mismatch;
 *   - the log ring is covered line by line: text is written first and the
 *     line's own seq word last, so on x86 a reader never pairs new text with
 *     an old seq or the reverse. The ring is a bounded tail, not a queue.
 *
 * Counters are lifetime monotonic. The panel derives per-second rates from
 * the delta between two polls, so a slow or missed poll merges windows
 * instead of losing data. Producers also mirror their recent log lines here
 * so the panel needs no file tailing and no log parsing.
 */
#ifndef BG_DIAG_H
#define BG_DIAG_H

#include <stdint.h>

#define BG_DIAG_MAGIC 0x42474447u /* "BGDG" */
#define BG_DIAG_VERSION 2u

#define BG_DIAG_SECTION_NAME "Local\\BetterGame.v1.diag"

/* Fixed slot order; the panel indexes by these. */
enum {
    BG_DIAG_AGENT = 0,   /* better_game.dll, the game's main thread */
    BG_DIAG_OVERLAY = 1, /* view.exe */
    BG_DIAG_ENET = 2,    /* enet.dll proxy, the game's network threads */
    BG_DIAG_SHIM = 3     /* EOSSDK-Win64-Shipping.dll, the EOS stub layer */
};
#define BG_DIAG_PRODUCERS 4

/* Same width as PerfWindow buckets: pump phases, frame phases, packet cost. */
#define BG_DIAG_BUCKETS 8

/* Log ring: one record per line, 160 bytes: seq, pad, wall_ms, text. Longer
   lines are truncated; the log files keep the full line. */
#define BG_DIAG_LOG_LINES 48
#define BG_DIAG_LOG_TEXT 144

/* slot.flags for BG_DIAG_AGENT */
enum {
    BG_DAG_WORKING = 1u << 0,  /* pump past the switches: doing Unity work */
    BG_DAG_SCANNING = 1u << 1, /* a discovery pass is in flight */
    BG_DAG_COLD = 1u << 2,    /* the running pass pays a runtime call per hit */
    BG_DAG_MASTER = 1u << 3,   /* control block master as last read */
    BG_DAG_ESP = 1u << 4,      /* control block enabled (ESP boxes) as read */
    BG_DAG_AIM = 1u << 5       /* control block aim as read */
};

/* slot.flags for BG_DIAG_OVERLAY */
enum {
    BG_DOV_WINDOW = 1u << 0,  /* the game window was found this frame */
    BG_DOV_LIVE = 1u << 1,   /* a fresh agent frame was drawn this frame */
    BG_DOV_ESP = 1u << 2,    /* control block enabled (ESP boxes) as read */
    BG_DOV_AIM = 1u << 3,     /* aim assist armed (view.cfg master) */
    BG_DOV_MASTER = 1u << 4   /* control block master as last read */
};

/* slot.flags for BG_DIAG_ENET */
enum {
    BG_DEN_LOG = 1u << 0,       /* packet log on (enet-proxy.ini log=1) */
    BG_DEN_CAPTURE = 1u << 1,  /* binary capture on (capture=1) */
    BG_DEN_PATCH = 1u << 2,    /* rewrite rules enabled (patch=1) */
    BG_DEN_INVENTORY = 1u << 3, /* inventory overrides active */
    BG_DEN_OBSERVING = 1u << 4, /* any of the above: observe runs per packet */
    BG_DEN_REAL = 1u << 5       /* enet_real.dll loaded */
};

/* Which of our threads ran the per-packet work, with its own counters.
       enet is called from several of the game's network threads and its cost
       must be attributable per thread; single-threaded producers leave it
       zeroed. tid 0 marks an unused row. */
typedef struct {
    uint64_t tid;
    uint64_t packets; /* packets this thread observed */
    uint64_t us;       /* observe time on this thread */
} bg_diag_thread;

#define BG_DIAG_THREADS 8

/* slot.state: the producer's main state, one small number per producer.
   Agent: BG_STATE_* from bg_ipc.h, or 0 before the runtime is up.
   Overlay: 0 no game window, 1 no frame yet, 2 drawing live frames.
   enet: 0 forwarding only, 1 observing. Shim: 0 plain, 1 with the agent's
   pump wired in. */

/* Bucket names, one table per producer, same order as that producer's
   PerfBucket enum; unused trailing slots are null. The panel's status lines
   are built from these, so the names live with the contract. */
static const char* const BG_DIAG_AGENT_BUCKET_NAMES[BG_DIAG_BUCKETS] = {
    "pump", "model", "skel", "scan", "pub", "house", 0, 0};
static const char* const BG_DIAG_OVERLAY_BUCKET_NAMES[BG_DIAG_BUCKETS] = {
    "frame", "read", "aim", "draw", "present", 0, 0, 0};
static const char* const BG_DIAG_ENET_BUCKET_NAMES[BG_DIAG_BUCKETS] = {
    "out", "in", 0, 0, 0, 0, 0, 0};
static const char* const BG_DIAG_SHIM_BUCKET_NAMES[BG_DIAG_BUCKETS] = {
    "tick", 0, 0, 0, 0, 0, 0, 0};

/* One ring record. The text is written first and seq last, so a reader
   either sees the old whole line or the new whole line, never a splice.
   wall_ms is UTC milliseconds so lines from three processes sort by real
   time in the panel. */
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
    /* The producer's own loop rate, smoothed: for the agent this is the
       game's main-thread FPS, measured from EOS_Platform_Tick cadence and
       alive even when every switch is off. The overlay and enet leave it 0;
       their rates come from bucket_count deltas. */
    uint32_t hz;
    uint32_t flags;  /* BG_DAG_* / BG_DOV_* / BG_DEN_* */
    uint32_t state;  /* see the comment above */
    uint64_t mono_ms; /* GetTickCount64 at the moment of this publish */
    /* Producer-specific monotonic counters. Agent: players tracked, entities
       in the last published frame, managed calls total, scan position, scan
       total. Overlay: frames presented, boxes drawn in the last frame, age of
       the last agent frame (ms), entities in the last read. enet: packets
       forwarded out, in, bytes out, in - counted on every packet, observed
       or not. Shim: EOS_Platform_Tick calls, faults caught by the crash
       probe, milliseconds of pump inside them. */
    uint64_t counters[6];
/* Lifetime bucket totals, the monotonic side of PerfWindow. ms/s per
       bucket = delta of bucket_us between polls; avg per call = delta of
       bucket_us over delta of bucket_count. */
    uint64_t bucket_count[BG_DIAG_BUCKETS];
    uint64_t bucket_us[BG_DIAG_BUCKETS];
    uint64_t bucket_max_us[BG_DIAG_BUCKETS];
    /* Per-thread attribution for the multi-threaded producers. */
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
