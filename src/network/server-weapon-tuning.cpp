// 09 22 2026
/* purpose
* Implements the authoritative weapon tuning lookup (see the header).
*/
#include "network/server-weapon-tuning.h"

#include <cstdio>
#include <string>

#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "network/network-weapons.h"

namespace MimitaNet {

bool serverWeaponTuning(std::uint32_t weaponDefNetworkId, GameWeaponTuningV1* out)
{
    if (!out)
        return false;
    *out = GameWeaponTuningV1{};

    const std::string* id = weaponIdForDefNetworkId(weaponDefNetworkId);
    const WeaponDefinition* def =
        id ? WeaponRegistry::instance().get(*id) : nullptr;
    if (!def)
        return false;

    out->found = 1;
    out->behaviorType = static_cast<std::uint32_t>(def->behaviorType);
    out->executionType = static_cast<std::uint32_t>(def->executionType);
    out->networkMode = static_cast<std::uint32_t>(def->networkMode);
    out->magazineSize = def->magazineSize;
    out->reserveAmmo = initialReserveAmmoForDefinition(*def);
    out->pelletCount = def->pelletCount;
    out->damage = def->damage;
    out->headshotMultiplier = def->headshotMultiplier;
    out->fireDelay = def->fireDelay;
    out->reloadTime = def->reloadTime;
    out->spread = def->spread;
    out->recoil = def->recoil;
    out->beamThickness = def->beamThickness;
    out->beamWorldThickness = def->beamWorldThickness;
    out->victimKnockbackPerDamage = def->victimKnockbackPerDamage;
    out->projectileSpeed = def->projectileSpeed;
    out->projectileRadius = def->projectileRadius;
    out->projectileLifetime = def->projectileLifetime;

    std::uint32_t count = 0;
    for (const auto& entry : def->customParams)
    {
        if (count >= (std::uint32_t)GAME_MAX_WEAPON_CUSTOM_PARAMS)
            break;
        GameWeaponParamV1& param = out->custom[count++];
        std::snprintf(param.key, GAME_WEAPON_PARAM_KEY, "%s", entry.first.c_str());
        param.value = entry.second;
    }
    out->customCount = count;
    return true;
}

} // namespace MimitaNet
