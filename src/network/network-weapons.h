#pragma once

#include <cstdint>

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
