// NPC-only debug controls split from the project-wide src/config.h aggregate.
// Keeping these flags in a narrow header means changing an NPC debug flag does
// not rebuild every consumer of the global debug configuration.
#pragma once

#include <cstdint>

namespace DebugConfig
{
    inline bool DEBUG_NPC = false;
    inline bool DEBUG_NPC_COMBAT = false;
    // Focused presentation/lifecycle trace for one NPC identity.
    inline bool DEBUG_NPC_TRACE = false;
    inline std::uint32_t NPC_TRACE_TARGET = 0;
    inline bool DEBUG_NPC_DEATH = false;
    inline bool DEBUG_NPC_MOVEMENT = false;

    inline void ResetNpcDebugFlags()
    {
        DEBUG_NPC = false;
        DEBUG_NPC_COMBAT = false;
        DEBUG_NPC_TRACE = false;
        NPC_TRACE_TARGET = 0;
        DEBUG_NPC_DEATH = false;
        DEBUG_NPC_MOVEMENT = false;
    }
}
