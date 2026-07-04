#pragma once

namespace DebugConfig
{
    inline bool ENABLE_DEBUG_LOGS = true;

    inline bool GLB_VERBOSE = false;
    inline bool COLLISION_VERBOSE = false;
    inline bool PHYSICS_VERBOSE = false;
    inline bool RENDER_VERBOSE = false;

    inline float PRINT_INTERVAL = 0.25f;

    // Runtime debug toggles (controlled via terminal)
    inline bool DEBUG_COLLISION = false;
    inline bool DEBUG_COLLISION_TRACE = false;
    inline bool DEBUG_MOVEMENT = false;
    inline bool DEBUG_PHYSICS = false;
    inline bool DEBUG_SOUND = false;
    inline bool DEBUG_RENDER = false;
    inline bool DEBUG_PLAYERARCH = false;
    inline bool DEBUG_WIREFRAME = false;
    inline bool DEBUG_NORMALS = false;
    inline bool DEBUG_BOUNDS = false;
    inline bool DEBUG_UVCHECKER = false;
    inline bool DEBUG_LIGHTING_ONLY = false;
    inline bool DEBUG_TEXTURES_ONLY = false;
    inline bool DEBUG_AO_ONLY = false;
    inline bool DEBUG_COLLISION_VISUALS = false;
    inline bool DEBUG_UI = false;
    inline bool DEBUG_TICKS = false;
    inline bool DEBUG_INPUT = false;
    inline bool DEBUG_COMMANDS = false;
    inline bool DEBUG_NPC = false;
    inline bool DEBUG_NPC_COMBAT = false;
    inline bool DEBUG_RAGDOLL = false;
    inline bool DEBUG_PERSISTENT_PHYSICS = false;
    inline bool DEBUG_NPC_DEATH = false;
    inline bool DEBUG_NPC_DEATH_FREEZE = false;
    inline bool DEBUG_BLOOD_RAYS = false;
    inline bool DEBUG_BLOOD_HITS = false;
    inline bool DEBUG_BLOOD_FORCE = false;
    inline bool DEBUG_DEBRIS = false;
    inline bool DEBUG_RECOIL = false;
    inline bool DEBUG_RELOAD = false;
    inline bool DEBUG_SWORDSWORD = false;
    inline bool DEBUG_ANIMATION = false;
    inline bool DEBUG_GODBALL = false;
    inline bool DEBUG_GODBALL_HITSTOP = false;
    inline bool DEBUG_ANIM_ARMS = false;
    // 7 1 2026 I MADE A GIT COMMIT ON THIS DA
    // TO MIMITA REPO
    // IDK WAHT CHANGED OR WHAT WAS WRONG BUT SLOPES WORK AND
    // OMGOSH FINALLY BRO. OMGSOSH.
    inline bool DEBUG_COLLISION_SYSTEM = false;
    // inline bool DEBUG_COLLISION_SYSTEM = true;
    inline bool DEBUG_COLLISION_PLAYER = false;
    inline bool DEBUG_COLLISION_LIMB = false;
    inline bool DEBUG_COLLISION_BODY_PUSH = false;
    inline bool DEBUG_COLLISION_GRID = false;
    inline bool DEBUG_COLLISION_VALIDATE = false;
    inline bool DEBUG_BOMBTAG = false;
    inline bool DEBUG_NPC_MOVEMENT = false;
    inline bool DEBUG_PERF_MODEL = false;
    inline bool DEBUG_REPLAY = true;
    inline bool DEBUG_NETWORKING = false;
    inline bool DEBUG_DUEL = false;
    inline bool DEBUG_VISUALS_MASTER = false;
    inline bool DEBUG_WEAPON_VIEWMODEL = false;
    inline bool DEBUG_MENU_PREVIEW = false;
    inline bool DEBUG_WEAPON_COLLISION = false;
    // Was true before 7/1/2026 — caused debugBodyWeaponPhase to run every 0.25s
    // doing a full second BodyWeapon pass (sphere gen + broadphase + triangle tests).
    // Also had a printf inside the rate-limited path.
    // Collision diagnostics: always-on pipeline instrumentation.
    // Set false to suppress all collision diagnostics.
    inline bool DEBUG_COLLISION_DIAGNOSTICS = false;
    // Debug shot line: red beam + sphere. Set 0 to hide debug visuals.
    inline bool DEBUG_WPN_SHOT_LINE = true;

    // World-space crosshair settings (live-updated via console commands)
    inline float WORLD_XH_ALPHA = 0.0f;       // 0=opaque, 1=invisible
    inline float WORLD_XH_LENGTH = 1.0f;       // arm length multiplier
    inline float WORLD_XH_R = 1.0f;            // red
    inline float WORLD_XH_G = 0.5f;            // green
    inline float WORLD_XH_B = 0.0f;            // blue
    inline float WORLD_XH_GAP = 1.0f;          // center gap multiplier
    inline float WORLD_XH_THICKNESS = 1.0f;    // arm thickness multiplier
    inline bool  WORLD_XH_OUTLINE = false;     // outline enabled
    inline float WORLD_XH_OUTLINE_ALPHA = 0.0f; // 0=opaque, 1=invisible
    inline bool  WORLD_XH_DYNAMIC = true;      // 0=fixed size, 1=distance-based scaling
    inline float WORLD_XH_MAXSIZE = 2.0f;      // max scale multiplier, clamped ≥ 0
    inline float WORLD_XH_MINSIZE = 0.5f;      // min scale multiplier, clamped ≥ 0
    inline bool  WORLD_XH_CENTERDOT = false;   // 0=hidden, 1=visible

    inline bool DEBUG_ROTATION = false;
    inline bool DEBUG_AUTH = false;

    inline void ResetAll()
    {
        DEBUG_COLLISION = false;
        DEBUG_COLLISION_TRACE = false;
        DEBUG_MOVEMENT = false;
        DEBUG_PHYSICS = true;
        DEBUG_SOUND = false;
        DEBUG_RENDER = true;
        DEBUG_PLAYERARCH = true;
        DEBUG_WIREFRAME = false;
        DEBUG_NORMALS = false;
        DEBUG_BOUNDS = true;
        DEBUG_UVCHECKER = false;
        DEBUG_LIGHTING_ONLY = false;
        DEBUG_TEXTURES_ONLY = false;
        DEBUG_AO_ONLY = false;
        DEBUG_COLLISION_VISUALS = true;
        DEBUG_UI = false;
        DEBUG_TICKS = false;
        DEBUG_INPUT = false;
        DEBUG_COMMANDS = false;
        DEBUG_NPC = false;
        DEBUG_BLOOD_RAYS = false;
        DEBUG_BLOOD_HITS = false;
        DEBUG_BLOOD_FORCE = false;
        DEBUG_DEBRIS = false;
        DEBUG_RECOIL = false;
        DEBUG_RELOAD = false;
        DEBUG_SWORDSWORD = false;
        DEBUG_ANIMATION = false;
        DEBUG_GODBALL = false;
        DEBUG_GODBALL_HITSTOP = false;
        DEBUG_ANIM_ARMS = false;
        DEBUG_NPC_MOVEMENT = false;
        DEBUG_COLLISION_PLAYER = false;
        DEBUG_COLLISION_LIMB = false;
        DEBUG_COLLISION_BODY_PUSH = false;
        DEBUG_COLLISION_GRID = false;
        DEBUG_COLLISION_VALIDATE = false;
        DEBUG_BOMBTAG = false;
        DEBUG_REPLAY = false;
        DEBUG_NETWORKING = false;
        DEBUG_DUEL = false;
        DEBUG_NPC_COMBAT = false;
    }
}

namespace CursorConfig
{
    inline bool CUSTOM_CURSOR_ENABLED = true;
    inline const char* CUSTOM_CURSOR_PATH = "assets/textures/cursor.png";

    // false = top-left arrow-style hotspot, true = centered crosshair-style hotspot
    inline bool CUSTOM_CURSOR_HOTSPOT_CENTERED = false;
}
