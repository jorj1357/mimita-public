// 09 12 2026
/* purpose
* Hot replaceable presentation module: damage-number formatting and rocket
* trail parameters. Returns plain presentation data; never owns GPU resources.
* Does NOT own effect pools, rendering, audio, or network state.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"

#include <cstdio>

namespace {

bool MIMITA_GAME_CALL formatDamageNumber(
    const DamageNumberStyleV1* base, int damage, std::uint32_t /*flags*/,
    DamageNumberStyleV1* out, GameMemory*)
{
    if (!base || !out || base->text[0] == '\0')
        return false;

    *out = *base;

    // Healing stays green and small. Critical hits grow and turn gold.
    if (damage < 0) {
        out->scale = base->scale;
        out->endScale = base->endScale;
    } else if (damage >= 100) {
        out->scale = base->scale * 1.6f;
        out->endScale = base->endScale * 1.6f;
        out->color[0] = 1.0f;
        out->color[1] = 0.85f;
        out->color[2] = 0.20f;
    }
    return true;
}

bool MIMITA_GAME_CALL rocketTrail(
    const RocketTrailStyleV1* base, RocketTrailStyleV1* out, GameMemory*)
{
    if (!base || !out)
        return false;
    *out = *base;
    out->size = base->size * 1.25f;
    out->endSize = base->endSize * 1.25f;
    return true;
}

const GamePresentationModuleV1 gPresentationModuleV1 = {
    1u,
    sizeof(GamePresentationModuleV1),
    formatDamageNumber,
    rocketTrail,
};

} // namespace

const GameModuleDescriptor* MimitaGetPresentationModule()
{
    static const GameModuleDescriptor descriptor = {
        "presentation", 1u, sizeof(GamePresentationModuleV1), &gPresentationModuleV1};
    return &descriptor;
}

#endif
