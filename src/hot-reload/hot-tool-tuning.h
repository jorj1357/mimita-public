// 09 22 2026
/* purpose
* Hot-side helper for querying the authoritative (JSON/cpp-resolved) weapon
* tuning through the generic `weapon.tuning` capability, so hot tool behaviors
* use the same values as the cold registry and editing config/weapons.json
* hot-retunes them.
* Does NOT own weapons.
*/
#pragma once

#include <cstdint>
#include <cstring>

#include "hot-reload/game-api.h"

inline bool hotQueryWeaponTuning(GameplayContextV1* ctx,
                                 std::uint32_t weaponDefNetworkId,
                                 GameWeaponTuningV1& out)
{
    out = GameWeaponTuningV1{};
    if (!ctx || !ctx->resolveCapability || weaponDefNetworkId == 0)
        return false;
    auto fn = reinterpret_cast<GameWeaponTuningFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_WEAPON_TUNING));
    if (!fn)
        return false;
    return fn(ctx->host, weaponDefNetworkId, &out) && out.found != 0;
}

inline bool hotTuningHasParam(const GameWeaponTuningV1& tuning, const char* key,
                              float* out)
{
    for (std::uint32_t i = 0;
         i < tuning.customCount && i < (std::uint32_t)GAME_MAX_WEAPON_CUSTOM_PARAMS;
         ++i)
    {
        if (std::strcmp(tuning.custom[i].key, key) == 0)
        {
            if (out)
                *out = tuning.custom[i].value;
            return true;
        }
    }
    return false;
}
