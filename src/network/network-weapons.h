#pragma once

#include <cstdint>

struct WeaponDefinition;

namespace MimitaNet {

uint8_t networkWeaponTypeForDefinition(const WeaponDefinition& definition);
uint8_t networkWeaponTypeForSlot(int slot);
int slotForNetworkWeaponType(uint8_t type);
const char* networkWeaponTypeName(uint8_t type);
bool networkWeaponTypeIsProjectile(uint8_t type);
bool networkWeaponTypeIsHitscan(uint8_t type);
bool networkWeaponTypeIsMelee(uint8_t type);

} // namespace MimitaNet
