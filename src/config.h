#pragma once

namespace DebugConfig
{
    inline bool ENABLE_DEBUG_LOGS = false;

    inline bool GLB_VERBOSE = false;
    inline bool COLLISION_VERBOSE = false;
    inline bool PHYSICS_VERBOSE = false;
    inline bool RENDER_VERBOSE = false;

    inline float PRINT_INTERVAL = 0.25f;
}

namespace CursorConfig
{
    inline bool CUSTOM_CURSOR_ENABLED = true;
    inline const char* CUSTOM_CURSOR_PATH = "assets/textures/cursor.png";

    // false = top-left arrow-style hotspot, true = centered crosshair-style hotspot
    inline bool CUSTOM_CURSOR_HOTSPOT_CENTERED = false;
}
