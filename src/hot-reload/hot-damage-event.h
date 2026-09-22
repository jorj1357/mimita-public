// 09 22 2026
/* purpose
* Hot-side per-victim damage consequence: resolve policy and apply the damage +
* replication event through the generic `damage.policy` / `damage.event`
* primitives. One step, callable from any hot consequence orchestration.
* Does NOT own the trace or weapon behavior.
*/
#pragma once

#include "hot-reload/game-api.h"

// Resolve the authoritative damage/knockback for one victim (hot policy + cap).
inline void hotResolveDamagePolicy(GameplayContextV1* ctx, GameDamagePolicyV1& request)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<GameDamagePolicyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_POLICY));
    if (fn)
        fn(ctx->host, &request);
}

// Apply one damage fact and emit the matching replication event.
inline void hotApplyDamageEvent(GameplayContextV1* ctx, GameDamageEventV1& event)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<GameDamageEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_EVENT));
    if (fn)
        fn(ctx->host, &event);
}
