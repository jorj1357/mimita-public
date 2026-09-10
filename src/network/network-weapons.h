// 07 21 2026, 23 38
/* purpose
* Declares network weapon id helpers for generic client/server weapon routing.
* Exposes stable weapon-definition id registration and lookup at the packet boundary.
* Groups compact weapon ids by execution family for migrated attack requests.
* Does NOT own weapon definition storage, JSON parsing, ammo, cooldown, or damage.
* Does NOT send packets, simulate projectiles, render effects, or mutate runtime state.
* Does NOT derive weapon ids from unordered spawn inventory packet order.
*/

#pragma once

#include <cstdint>
#include <string>

struct WeaponDefinition;

namespace MimitaNet {

// DEPRECATED: Do not use for new code. This function hardcodes weapon ID strings
// and will not map any weapon added after this date. Use the dynamic
// weaponDefNetworkId system (registerWeaponDefNetworkId / weaponDefNetworkIdFor)
// combined with WeaponRegistry lookups instead.
uint8_t networkWeaponTypeForDefinition(const WeaponDefinition& definition);
uint8_t networkWeaponTypeForSlot(int slot);
uint16_t registerWeaponDefNetworkId(const std::string& weaponId);
uint16_t weaponDefNetworkIdFor(const std::string& weaponId);
const std::string* weaponIdForDefNetworkId(uint16_t networkId);
// Returns the display name for a weapon given its dynamic network ID.
// Works for ANY registered weapon. Prefer this over networkWeaponTypeName.
const char* weaponDisplayName(uint16_t defNetworkId);
int slotForNetworkWeaponType(uint8_t type);
const char* networkWeaponTypeName(uint8_t type);
bool networkWeaponTypeIsProjectile(uint8_t type);
bool networkWeaponTypeIsHitscan(uint8_t type);
bool networkWeaponTypeIsMelee(uint8_t type);

} // namespace MimitaNet
