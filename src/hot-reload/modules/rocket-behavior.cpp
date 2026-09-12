// 09 12 2026
/* purpose
* Hot replaceable rocket flight and explosion policy module.
* Returns plain parameter overrides; the EXE owns projectile state and authority.
* Defaults are identity so behavior is unchanged until a developer edits this
* file, which is then activated live without relinking.
* Does NOT own projectile simulation, damage application, or packet flow.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"

namespace {

bool MIMITA_GAME_CALL adjustRocketFlight(
    const RocketFlightStateV1* state, const RocketFlightParamsV1* base,
    RocketFlightParamsV1* out, GameMemory*)
{
    if (!base || !out)
        return false;
    *out = *base;

    // Live-editable proof policy. Change these values while MiMITA.exe is
    // running, save, and the next live generation should use them.
    out->speedScale = 1.30f;
    (void)state;
    return true;
}

bool MIMITA_GAME_CALL explosionParameters(
    const ExplosionStateV1* state, const ExplosionParamsV1* base,
    ExplosionParamsV1* out, GameMemory*)
{
    if (!base || !out)
        return false;
    *out = *base;

    // Live-editable proof policy. This makes the activation visible without
    // changing the persistent rocket state owned by the EXE.
    out->baseDamage = base->baseDamage * 12.50f;
    (void)state;
    return true;
}

const GameGameplayModuleV1 gGameplayModuleV1 = {
    1u,
    sizeof(GameGameplayModuleV1),
    adjustRocketFlight,
    explosionParameters,
};

} // namespace

const GameModuleDescriptor* MimitaGetGameplayModule()
{
    static const GameModuleDescriptor descriptor = {
        "gameplay", 1u, sizeof(GameGameplayModuleV1), &gGameplayModuleV1};
    return &descriptor;
}

#endif
