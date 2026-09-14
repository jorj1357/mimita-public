// 09 14 2026
/* purpose
* Component-authoritative weapon runtime state (ammo/cooldown/reload) for
* weapons migrated off the legacy string-keyed runtime map. The state lives as a
* dynamic component on a per-weapon tool entity owned by the player, found via
* the generic contains-item relationship. The legacy map entry is only a
* per-call scratch view while a weapon is migrated.
* Does NOT own networking, input, or the legacy maps.
*/
#pragma once

#include <cstdint>
#include <string>

#include "network/server.h"

namespace MimitaNet {

// True when this weapon's runtime state is component-authoritative.
bool serverWeaponIsMigrated(const std::string& weaponId);

// The tool entity that owns this weapon's state (created/imported on first use).
std::uint64_t serverWeaponToolEntity(ServerPlayer& player, const std::string& weaponId,
                                     bool create);

// Sync the legacy scratch view from / to the authoritative component.
void serverWeaponStateLoad(ServerPlayer& player, const std::string& weaponId);
void serverWeaponStateStore(ServerPlayer& player, const std::string& weaponId);

// Direct component accessors (used by tests and by owners that bypass the map).
bool serverWeaponStateReadComponent(ServerPlayer& player, const std::string& weaponId,
                                    std::int32_t* magazineAmmo,
                                    std::int32_t* reserveAmmo,
                                    std::uint64_t* nextAllowedFireTick);
bool serverWeaponStateWriteComponent(ServerPlayer& player, const std::string& weaponId,
                                     std::int32_t magazineAmmo,
                                     std::int32_t reserveAmmo,
                                     std::uint64_t nextAllowedFireTick);

} // namespace MimitaNet
