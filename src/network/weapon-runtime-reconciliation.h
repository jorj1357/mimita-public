#pragma once

#include <cstdint>

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

} // namespace MimitaNet
