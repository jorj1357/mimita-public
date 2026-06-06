#include "combat/weapon-manager.h"

#include <algorithm>

#include "audio/audio.h"
#include "devtools/terminal.h"
#include "entities/player.h"

WeaponManager::WeaponManager()
{
    revolverDefinition = {"revolver", 1, 6};
}

void WeaponManager::update(const Camera& camera, Player& player, float dt)
{
    if (player.equippedSlot == revolverDefinition.slot)
        revolver.update(camera, player, dt);
}

void WeaponManager::render(const Camera& camera, const World& world) const
{
    revolver.render(camera, world);
}

RevolverShotResult WeaponManager::fire(Player& player, NpcSystem& npcs, const World& world)
{
    if (player.equippedSlot != revolverDefinition.slot) {
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.2f, 0.65f});
        return {};
    }
    if (player.revolverCylinder <= 0) {
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.25f, 0.55f});
        return {};
    }
    return revolver.fire(player, npcs, world);
}

bool WeaponManager::reload(Player& player)
{
    if (player.equippedSlot != revolverDefinition.slot)
        return false;
    int needed = revolverDefinition.cylinderCapacity - player.revolverCylinder;
    int loaded = std::min(needed, player.revolverReserve);
    if (loaded <= 0) {
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.25f, 0.7f});
        return false;
    }
    AudioManager::instance().play({"revolverpullback", AudioCategory::Weapons, false, {}, 0.8f});
    for (int i = 0; i < loaded; ++i)
        AudioManager::instance().play({"revolverbulletadd", AudioCategory::Weapons, false, {}, 0.65f});
    player.revolverCylinder += loaded;
    player.revolverReserve -= loaded;
    AudioManager::instance().play({"revolverchamber", AudioCategory::Weapons, false, {}, 0.9f});
    return true;
}

void WeaponManager::equip(Player& player, int slot)
{
    player.equippedSlot = slot;
    if (slot == revolverDefinition.slot)
        AudioManager::instance().play({"revolverequip", AudioCategory::Weapons, false, {}, 0.85f});
}

void WeaponManager::unequip(Player& player) { player.equippedSlot = 0; }

void WeaponManager::inspect() const
{
    Terminal::instance().addLog("[WEAPON] active module: revolver transform/physics/audio/recoil/ammo/fire/effects/damage");
}

const std::vector<std::string>& WeaponManager::killfeed() const { return revolver.killfeed(); }
