#pragma once

#include <cstdint>
#include <string>

struct MultiplayerContext;
class Player;

namespace MimitaNet {

bool reconcileAuthoritativeWeaponRuntime(
    MultiplayerContext& ctx,
    Player& player,
    uint16_t weaponDefNetworkId,
    int magazineAmmo,
    int reserveAmmo,
    uint64_t nextAllowedFireTick,
    bool reloading,
    uint64_t reloadCompleteTick,
    uint32_t stateRevision,
    uint32_t spawnGeneration,
    const char* source,
    bool applyAmmo = true);

// Fire-and-forget reload request for a SPECIFIC weapon (holstered or not).
// Used when a weapon starts its background reload on unequip/switch-away so
// the server reloads it too. Same packet as pressing R; the client keeps its
// own predicted ammo (applyAmmo=false on results), so nothing snaps.
void sendReloadRequestForWeapon(MultiplayerContext& ctx,
                                const std::string& weaponId);

} // namespace MimitaNet
