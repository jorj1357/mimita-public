#pragma once

namespace DebugConfig
{
    inline bool ENABLE_DEBUG_LOGS = false;

    inline bool GLB_VERBOSE = false;
    inline bool COLLISION_VERBOSE = false;
    inline bool PHYSICS_VERBOSE = false;
    inline bool RENDER_VERBOSE = false;

    inline float PRINT_INTERVAL = 0.25f;

    // Runtime debug toggles (controlled via terminal)
    inline bool DEBUG_COLLISION = false;
    inline bool DEBUG_MOVEMENT = false;
    inline bool DEBUG_PHYSICS = true;
    inline bool DEBUG_SOUND = false;
    inline bool DEBUG_RENDER = true;
    inline bool DEBUG_PLAYERARCH = true;
    inline bool DEBUG_WIREFRAME = false;
    inline bool DEBUG_NORMALS = false;
    inline bool DEBUG_BOUNDS = true;
    inline bool DEBUG_UVCHECKER = false;
    inline bool DEBUG_LIGHTING_ONLY = false;
    inline bool DEBUG_TEXTURES_ONLY = false;
    inline bool DEBUG_AO_ONLY = false;
    inline bool DEBUG_COLLISION_VISUALS = true;
    inline bool DEBUG_UI = false;
    inline bool DEBUG_TICKS = false;
    inline bool DEBUG_INPUT = false;
    inline bool DEBUG_COMMANDS = false;
    inline bool DEBUG_NPC = false;
    inline bool DEBUG_RAGDOLL = false;
    inline bool DEBUG_BLOOD_RAYS = false;
    inline bool DEBUG_BLOOD_HITS = false;
    inline bool DEBUG_BLOOD_FORCE = false;
    inline bool DEBUG_DEBRIS = false;
    inline bool DEBUG_RECOIL = false;
    inline bool DEBUG_RELOAD = false;
    inline bool DEBUG_SWORDSWORD = false;
    inline bool DEBUG_ANIMATION = false;
    inline bool DEBUG_GODBALL = false;
    inline bool DEBUG_VISUALS_MASTER = true;

    inline void ResetAll()
    {
        DEBUG_COLLISION = false;
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
    }
}

namespace CursorConfig
{
    inline bool CUSTOM_CURSOR_ENABLED = true;
    inline const char* CUSTOM_CURSOR_PATH = "assets/textures/cursor.png";

    // false = top-left arrow-style hotspot, true = centered crosshair-style hotspot
    inline bool CUSTOM_CURSOR_HOTSPOT_CENTERED = false;
}
