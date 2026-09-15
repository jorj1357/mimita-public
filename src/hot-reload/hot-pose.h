// 09 15 2026
/* purpose
* Shared generic pose state. A hot pose system generates local bone offsets and
* publishes them through the generic skeleton.apply capability; the kernel
* stores them as this POD `PoseState` component on the entity (no pointers into
* hot DLL memory). Type-agnostic: player, NPC, monster, replay actor.
* Not a GameAPI context field.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_POSE_STATE_COMPONENT = gameHash("PoseState");

static constexpr std::uint32_t HOT_POSE_MAX_PARTS = GAME_MAX_POSE_PARTS;

struct HotPoseStateV1 {
    std::uint32_t version;
    std::uint32_t count;
    std::uint64_t part[HOT_POSE_MAX_PARTS];
    float translation[HOT_POSE_MAX_PARTS][3];
    float rotationEuler[HOT_POSE_MAX_PARTS][3];
};
