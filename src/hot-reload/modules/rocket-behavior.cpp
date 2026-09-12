// 09 12 2026
/* purpose
* Hot replaceable rocket/gameplay behavior module.
* Implements the generic behavior event handler: the kernel emits events with
* plain-data payloads and this module owns the gameplay policy (damage,
* knockback), so editing this file changes authoritative behavior live.
* Also exposes rocket flight motion policy.
* Does NOT own projectile state, health storage, authority, or packet flow.
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
    // Motion policy is identity by default.
    (void)state;
    return true;
}

// Generic authoritative gameplay policy. The kernel fills base values and the
// behavior decides the result. `handled = 1` tells the kernel this policy owns
// the decision; the kernel then applies `outDamage` and the knockback.
void MIMITA_GAME_CALL onEvent(const GameEventV1* event, GameplayContextV1*)
{
    if (!event || !event->payload || event->typeId != GAME_EVENT_DAMAGE_POLICY ||
        event->payloadSize != sizeof(DamagePolicyV1))
        return;

    DamagePolicyV1* policy = static_cast<DamagePolicyV1*>(event->payload);
    policy->handled = 1;

    // Baseline: keep the JSON-derived base damage.
    policy->outDamage = policy->baseDamage;

    // Temporary live proof: explosion damage is intentionally enormous so the
    // authoritative hot-policy path is unmistakable in the running game.
    if (policy->source == GAME_DAMAGE_SOURCE_EXPLOSION)
        policy->outDamage = 123;

    (void)event;
}

const GameGameplayModuleV1 gGameplayModuleV1 = {
    2u,
    sizeof(GameGameplayModuleV1),
    adjustRocketFlight,
    onEvent,
};

} // namespace

const GameModuleDescriptor* MimitaGetGameplayModule()
{
    static const GameModuleDescriptor descriptor = {
        "gameplay", 2u, sizeof(GameGameplayModuleV1), &gGameplayModuleV1};
    return &descriptor;
}

#endif
