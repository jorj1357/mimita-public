// 09 15 2026
/* purpose
* Shared hot animation state. One generic `AnimationState` dynamic component
* carries the clip the hot animation policy selected and its playback. Any
* actor/monster/entity may carry it; no Player/Npc/Monster animation type.
* The hot system owns clip selection; the cold renderer owns skeleton/skinning.
* Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_ANIMATION_STATE_COMPONENT =
    gameHash("AnimationState");

struct HotAnimationStateV1 {
    std::uint64_t clipId;       // logical animation clip id (gameHash("anim.*"))
    float playbackTime;         // seconds into the clip
    float playbackRate;         // 1 = normal; <=0 treated as 1
    std::uint32_t loop;         // 1 = loop, 0 = one-shot
    std::uint32_t flags;        // reserved
};

static constexpr std::uint64_t HOT_ANIM_IDLE = gameHash("anim.idle");
static constexpr std::uint64_t HOT_ANIM_MOVE = gameHash("anim.move");
static constexpr std::uint64_t HOT_ANIM_ATTACK = gameHash("anim.attack");
static constexpr std::uint64_t HOT_ANIM_DEAD = gameHash("anim.dead");
static constexpr std::uint64_t HOT_ANIM_JUMP = gameHash("anim.jump");
