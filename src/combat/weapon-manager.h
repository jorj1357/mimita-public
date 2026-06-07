#pragma once

#include <string>
#include <vector>

#include "combat/revolver-system.h"

class Camera;
class Player;
class NpcSystem;
struct World;

enum class WeaponCrosshairState {
    Ready,
    Delay,
    Reloading
};

struct WeaponDefinition {
    std::string id;
    int slot = 0;
    int cylinderCapacity = 0;
};

class WeaponManager {
public:
    WeaponManager();

    void update(const Camera& camera, Player& player, float dt);
    void render(const Camera& camera, const Player& player) const;
    RevolverShotResult fire(const Camera& camera, Player& player, NpcSystem& npcs, const World& world);
    bool reload(Player& player);
    void equip(Player& player, int slot);
    void unequip(Player& player);
    void inspect() const;
    const std::vector<std::string>& killfeed() const;
    WeaponCrosshairState crosshairState(const Player& player) const;

private:
    WeaponDefinition revolverDefinition;
    RevolverSystem revolver;
    float shotCooldown = 0.0f;
    float reloadTimer = 0.0f;
    int pendingReloadRounds = 0;
};
