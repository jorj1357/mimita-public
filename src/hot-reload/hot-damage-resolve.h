// 09 22 2026
/* purpose
* Hot-side helper for the generic `damage.resolve` capability: route melee and
* projectile damage victims through the shared cold authoritative consequence
* owner instead of the raw `damage.apply` primitive.
* Does NOT own damage.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

inline void hotResolveDamage(GameplayContextV1* ctx, GameDamageResolveV1& request)
{
    request.victimCount =
        request.victimCount < (std::uint32_t)GAME_MAX_DAMAGE_VICTIMS
            ? request.victimCount
            : (std::uint32_t)GAME_MAX_DAMAGE_VICTIMS;
    if (!ctx || !ctx->resolveCapability || request.victimCount == 0)
        return;
    auto fn = reinterpret_cast<GameDamageResolveFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_RESOLVE));
    if (!fn)
        return;
    fn(ctx->host, &request);
}
