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

uint8_t networkWeaponTypeForDefinition(const WeaponDefinition& definition);
uint8_t networkWeaponTypeForSlot(int slot);
uint16_t registerWeaponDefNetworkId(const std::string& weaponId);
uint16_t weaponDefNetworkIdFor(const std::string& weaponId);
const std::string* weaponIdForDefNetworkId(uint16_t networkId);
int slotForNetworkWeaponType(uint8_t type);
const char* networkWeaponTypeName(uint8_t type);
bool networkWeaponTypeIsProjectile(uint8_t type);
bool networkWeaponTypeIsHitscan(uint8_t type);
bool networkWeaponTypeIsMelee(uint8_t type);

} // namespace MimitaNet
