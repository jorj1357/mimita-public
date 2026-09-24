#pragma once

// LEGACY (cold bridge): the Debug::Category API maps onto StructuredLogger and
// carries no policy of its own. Retained for existing callers; new diagnostics
// should use debug::logEvent or the hot `log.event` capability.
#include <cstdarg>
#include "../config.h"
#include "structured-log.h"

namespace Debug
{
    enum class Category
    {
        General,
        GLB,
        Collision,
        Physics,
        Render,
        NpcCombat,
        NpcMovement,
        Ragdoll,
        Replay,
        Weapons,
        Animation,
        Gui,
        Networking,
        Audio,
        World,
        Duel,
        Auth,
        Avatar,
        Chat,
        Vip
    };

    bool enabled(Category category);
    void startupReport();
    void log(Category category, const char* fmt, ...);
    void logOnce(Category category, const char* key, const char* fmt, ...);
    void logThrottled(Category category, const char* key, float intervalSeconds, const char* fmt, ...);
    void warn(Category category, const char* fmt, ...);
    void error(Category category, const char* fmt, ...);
    void logAuto(Category category, const char* fmt, ...);
}
