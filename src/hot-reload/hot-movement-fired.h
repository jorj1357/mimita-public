// 09 16 2026
/* purpose
* Shared hot movement-fired facts. Hot `movement.main` ORs the abilities that
* actually fired this frame into one local-only dynamic component; the hot
* animation state machine reads and clears it. This lets ability animations play
* ONLY when the ability truly activated (available + input edge), instead of on
* held input intent. No STL/pointers; local-only (never replicated).
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_MOVEMENT_FIRED_COMPONENT =
    gameHash("MovementFired");
static constexpr std::uint32_t HOT_MOVEMENT_FIRED_VERSION = 1;

enum HotMovementFiredFlags : std::uint32_t {
    HOT_FIRED_DASH      = 1u << 0,
    HOT_FIRED_DOWN_DASH = 1u << 1,
    HOT_FIRED_GROUND_JUMP = 1u << 2,
    HOT_FIRED_AIR_JUMP  = 1u << 3,
    HOT_FIRED_FREEZE    = 1u << 4,
};

struct HotMovementFiredV1 {
    std::uint32_t version;
    std::uint32_t flags;   // HotMovementFiredFlags bits, cleared after the read
};
