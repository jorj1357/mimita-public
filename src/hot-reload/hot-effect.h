// 09 15 2026
/* purpose
* Shared hot effect state. One generic `EffectLifetime` dynamic component makes
* an ordinary entity behave as a transient effect: it ages, integrates its
* Transform by its Velocity, grows/fades its PresentationState, and is destroyed
* on expiry. Any runtime package can invent a new effect with no enum/switch.
* Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_EFFECT_LIFETIME_COMPONENT =
    gameHash("EffectLifetime");

struct HotEffectLifetimeV1 {
    float age;
    float lifetime;
    float scale0;       // starting scale multiplier
    float growth;       // scale growth per second (0 = constant)
    float fadeStart;    // age fraction (0..1) at which alpha starts to fade
    std::uint32_t flags;
    std::uint32_t reserved;
};
