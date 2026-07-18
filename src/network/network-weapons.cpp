#include "network/network-weapons.h"

#include "combat/weapon-types.h"
#include "network/packets.h"

namespace MimitaNet {

uint8_t networkWeaponTypeForDefinition(const WeaponDefinition& definition)
{
    if (definition.id == "revolver" ||
        definition.id == "op_revolver" ||
        definition.id == "admin_revolver")
        return NETWORK_WEAPON_REVOLVER;
    if (definition.id == "godball")
        return NETWORK_WEAPON_GODBALL;
    if (definition.id == "shotgun")
        return NETWORK_WEAPON_SHOTGUN;
    if (definition.id == "aa12")
        return NETWORK_WEAPON_AA12;
    if (definition.id == "swordsword")
        return NETWORK_WEAPON_SWORDSWORD;
    if (definition.id == "rocket_launcher")
        return NETWORK_WEAPON_ROCKET_LAUNCHER;
    if (definition.id == "grenade_launcher")
        return NETWORK_WEAPON_GRENADE_LAUNCHER;
    if (definition.id == "hafs")
        return NETWORK_WEAPON_HAFS;
    return NETWORK_WEAPON_NONE;
}

uint8_t networkWeaponTypeForSlot(int slot)
{
    switch (slot)
    {
    case 1:
    case 5:
    case 9:
        return NETWORK_WEAPON_REVOLVER;
    case 2:
        return NETWORK_WEAPON_GODBALL;
    case 3:
        return NETWORK_WEAPON_SHOTGUN;
    case 6:
        return NETWORK_WEAPON_AA12;
    case 7:
        return NETWORK_WEAPON_ROCKET_LAUNCHER;
    case 8:
        return NETWORK_WEAPON_GRENADE_LAUNCHER;
    case 10:
        return NETWORK_WEAPON_HAFS;
    case 4:
    case 11:
        return NETWORK_WEAPON_SWORDSWORD;
    default:
        return NETWORK_WEAPON_NONE;
    }
}

// ── Weapon definition network ID (stable uint16 per definition) ─────
// Registered during WeaponData::registerBuiltinWeapons().
static std::unordered_map<std::string, uint16_t> s_weaponDefNetworkIds;
static uint16_t s_nextWeaponDefNetworkId = 1;

uint16_t registerWeaponDefNetworkId(const std::string& weaponId)
{
    auto it = s_weaponDefNetworkIds.find(weaponId);
    if (it != s_weaponDefNetworkIds.end())
        return it->second;
    uint16_t id = s_nextWeaponDefNetworkId++;
    s_weaponDefNetworkIds[weaponId] = id;
    return id;
}

uint16_t weaponDefNetworkIdFor(const std::string& weaponId)
{
    auto it = s_weaponDefNetworkIds.find(weaponId);
    return it != s_weaponDefNetworkIds.end() ? it->second : 0;
}

const std::string* weaponIdForDefNetworkId(uint16_t networkId)
{
    for (const auto& kv : s_weaponDefNetworkIds)
        if (kv.second == networkId)
            return &kv.first;
    return nullptr;
}

int slotForNetworkWeaponType(uint8_t type)
{
    switch (type)
    {
    case NETWORK_WEAPON_REVOLVER: return 1;
    case NETWORK_WEAPON_GODBALL: return 2;
    case NETWORK_WEAPON_SHOTGUN: return 3;
    case NETWORK_WEAPON_SWORDSWORD: return 4;
    case NETWORK_WEAPON_AA12: return 6;
    case NETWORK_WEAPON_ROCKET_LAUNCHER: return 7;
    case NETWORK_WEAPON_GRENADE_LAUNCHER: return 8;
    case NETWORK_WEAPON_HAFS: return 10;
    default: return -1;
    }
}

const char* networkWeaponTypeName(uint8_t type)
{
    switch (type)
    {
    case NETWORK_WEAPON_NONE: return "none";
    case NETWORK_WEAPON_REVOLVER: return "revolver";
    case NETWORK_WEAPON_GODBALL: return "godball";
    case NETWORK_WEAPON_SHOTGUN: return "shotgun";
    case NETWORK_WEAPON_SWORDSWORD: return "swordsword";
    case NETWORK_WEAPON_ROCKET_LAUNCHER: return "rocket_launcher";
    case NETWORK_WEAPON_HAFS: return "hafs";
    case NETWORK_WEAPON_GRENADE_LAUNCHER: return "grenade_launcher";
    case NETWORK_WEAPON_AA12: return "aa12";
    default: return "unknown";
    }
}

bool networkWeaponTypeIsProjectile(uint8_t type)
{
    return type == NETWORK_WEAPON_ROCKET_LAUNCHER ||
        type == NETWORK_WEAPON_GRENADE_LAUNCHER;
}

bool networkWeaponTypeIsHitscan(uint8_t type)
{
    return type == NETWORK_WEAPON_REVOLVER ||
        type == NETWORK_WEAPON_SHOTGUN ||
        type == NETWORK_WEAPON_AA12;
}

bool networkWeaponTypeIsMelee(uint8_t type)
{
    return type == NETWORK_WEAPON_SWORDSWORD ||
        type == NETWORK_WEAPON_HAFS ||
        type == NETWORK_WEAPON_GODBALL;
}

} // namespace MimitaNet
