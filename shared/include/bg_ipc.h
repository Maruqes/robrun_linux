/*
 * bg_ipc.h - shared-memory contract between the in-process agent
 *            (better_game.dll) and the external overlay (view.exe).
 *
 * Plain POD, fixed layout, x64 only. The agent publishes whole frames; the
 * viewer copies a frame only when the sequence counter was stable across the
 * read, so a torn frame is never drawn.
 *
 * Version 4: player model layer. Adds public identity, role, pose, the head
 * bone and a minimal torso. Agent and viewer must be deployed together: the
 * viewer rejects any frame whose version does not match its own.
 */
#ifndef BG_IPC_H
#define BG_IPC_H

#include <stdint.h>

#define BG_IPC_MAGIC 0x42475255u /* "BGRU" */
#define BG_IPC_VERSION 4u

#define BG_MAX_ENTITIES 512
#define BG_NAME_LEN 32

/* Screen-space torso joints: head, neck, chest, pelvis. */
#define BG_BONE_COUNT 4

/* Default draw radius, metres. The viewer and the agent read the same value
   from bg_control, so changing it there changes both. 0 = no limit. */
#define BG_DEFAULT_MAX_DISTANCE 300.0f

/* Section and event names. The agent creates, the viewer opens. */
#define BG_SECTION_NAME "Local\\BetterGame.v1"
#define BG_CONTROL_NAME "Local\\BetterGame.v1.control"
#define BG_TICK_EVENT   "Local\\BetterGame.v1.tick"

/* bg_entity.flags */
enum {
    BG_FLAG_LOCAL = 1u << 0,   /* the player we control */
    BG_FLAG_DEAD = 1u << 1,
    BG_FLAG_TEAMMATE = 1u << 2,
    BG_FLAG_VEHICLE = 1u << 3,
    BG_FLAG_NPC = 1u << 4,     /* server-controlled actor, if distinguishable */
    BG_FLAG_CROUCH = 1u << 5
};

/* bg_entity.role. The view-layer model never carried these; they come from
   PlayerEntityModel.ServerModel. Unknown is deliberately distinct from
   civilian so an unresolved role is white, not green. */
enum {
    BG_ROLE_UNKNOWN = 0,
    BG_ROLE_CIVILIAN = 1,
    BG_ROLE_CRIMINAL = 2,
    BG_ROLE_POLICE = 3
};

/* bg_entity.pose. Stance comes from CrouchModel/InVehicleModel/StateModel. */
enum {
    BG_POSE_UNKNOWN = 0,
    BG_POSE_STAND = 1,
    BG_POSE_CROUCH = 2,
    BG_POSE_VEHICLE = 3,
    BG_POSE_DOWN = 4,
    BG_POSE_DEAD = 5
};

/* bg_frame.state */
enum {
    BG_STATE_DETACHED = 0, /* agent alive, no game world yet */
    BG_STATE_ATTACHED = 1, /* runtime found, world not ready */
    BG_STATE_IN_WORLD = 2  /* positions are meaningful */
};

typedef struct {
    float x, y, z;
} bg_vec3;

typedef struct {
    float x, y;
} bg_vec2;

typedef struct {
    uint32_t id;         /* slot within this frame, for cheap lookups */
    uint32_t flags;      /* BG_FLAG_* */
    int64_t public_id;   /* stable player identity, 0 if unknown */
    int32_t entity_id;   /* model-layer entity id, -1 if unknown */
    uint8_t role;        /* BG_ROLE_* */
    uint8_t pose;        /* BG_POSE_* */
    uint8_t bone_count;  /* valid entries in screen_bones[] */
    uint8_t pad;
    bg_vec3 position;    /* world space, metres: feet */
    bg_vec2 screen_feet; /* pixels, origin = top-left of the game client area */
    bg_vec2 screen_head;
    bg_vec2 screen_bones[BG_BONE_COUNT]; /* head, neck, chest, pelvis */
    float distance;      /* metres from the local player */
    float health;        /* < 0 = unknown */
    float head_height_m; /* head above feet, metres */
    char name[BG_NAME_LEN]; /* UTF-8, NUL terminated */
} bg_entity;

typedef struct {
    uint32_t magic;         /* BG_IPC_MAGIC */
    uint32_t version;       /* BG_IPC_VERSION */
    uint32_t seq;           /* bumped after a completed write */
    uint32_t state;         /* BG_STATE_* */
    uint32_t count;         /* valid entries in entities[] */
    uint32_t total_players; /* players the agent knows this tick, for the HUD */
    uint32_t screen_w;      /* game client area, pixels */
    uint32_t screen_h;
    uint32_t pid;
    int64_t tick_unix_ms;
    bg_vec3 local_position;
    bg_entity entities[BG_MAX_ENTITIES];
} bg_frame;

/* Control block. Created by the agent, written by panel.exe, read by both the
   agent and the viewer every tick. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t enabled;        /* 0 = the overlay draws nothing */
    volatile uint32_t max_distance_m; /* 0 = no limit */
    /* Anything closer than this is not drawn. In a third-person game the
       camera sits several metres behind and above the local player, so they
       hold a near-constant distance and cannot be told apart from a player
       standing next to you - this is the practical cutoff. */
    volatile float min_distance_m;
    /* Master switch. 0 = none of our code runs: the agent returns from its
       pump before the first Unity call, log line or publish, and the viewer
       hides and skips reading, drawing and aiming. The EOS shim is untouched;
       the game cannot run without it. Appended after the v4 fields so a block
       written by an older build still reads correctly. */
    volatile uint32_t master;
    /* Aim assist, as the panel and the overlay see it. The overlay mirrors
       view.cfg here every frame (F8 reloads the file), and the agent reads it
       for the consumer gate: the scan only stops when both ESP and aim are
       off. Appended like master. */
    volatile uint32_t aim;
} bg_control;

#endif /* BG_IPC_H */
