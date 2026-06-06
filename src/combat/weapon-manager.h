#pragma once

#include <string>
#include <vector>

#include "combat/revolver-system.h"

class Camera;
class Player;
class NpcSystem;
struct World;

struct WeaponDefinition {
    std::string id;
    int slot = 0;
    int cylinderCapacity = 0;
};

class WeaponManager {
public:
    WeaponManager();

    void update(const Camera& camera, Player& player, float dt);
    void render(const Camera& camera, const World& world) const;
    RevolverShotResult fire(Player& player, NpcSystem& npcs, const World& world);
    bool reload(Player& player);
    void equip(Player& player, int slot);
    void unequip(Player& player);
    void inspect() const;
    const std::vector<std::string>& killfeed() const;

private:
    WeaponDefinition revolverDefinition;
    RevolverSystem revolver;
};
