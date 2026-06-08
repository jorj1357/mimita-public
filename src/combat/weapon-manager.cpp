#include "combat/weapon-manager.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-system.h"

#include <algorithm>

#include "audio/audio.h"
#include "devtools/terminal.h"
#include "entities/player.h"

namespace {
constexpr float REVOLVER_SHOT_COOLDOWN = 0.08f;
constexpr float REVOLVER_RELOAD_TIME = 1.0f;
}

WeaponManager::WeaponManager()
{
    const WeaponDefinition* def = WeaponRegistry::instance().get("revolver");
    if (def) {
        revolverDefinition = *def;
    }
}

void WeaponManager::update(const Camera& camera, Player& player, float dt)
{
    shotCooldown = std::max(0.0f, shotCooldown - dt);
    shootingTimer = std::max(0.0f, shootingTimer - dt);
    if (reloadTimer > 0.0f) {
        reloadTimer = std::max(0.0f, reloadTimer - dt);
        if (reloadTimer <= 0.0f && pendingReloadRounds > 0) {
            for (int i = 0; i < pendingReloadRounds; ++i)
                AudioManager::instance().play({"revolverbulletadd", AudioCategory::Weapons, false, {}, 0.65f});
            auto it = player.weaponRuntimes.find("revolver");
            if (it != player.weaponRuntimes.end()) {
                it->second.currentAmmo += pendingReloadRounds;
                it->second.reserveAmmo -= pendingReloadRounds;
            }
            pendingReloadRounds = 0;
            AudioManager::instance().play({"revolverchamber", AudioCategory::Weapons, false, {}, 0.9f});
        }
    }
    if (player.equippedSlot == revolverDefinition.slot)
        revolver.update(camera, player, dt);
}

void WeaponManager::render(const Camera& camera, const Player& player) const
{
    revolver.render(camera, player);
}

RevolverShotResult WeaponManager::fire(const Camera& camera, Player& player, NpcSystem& npcs, const World& world)
{
    if (player.dead) {
        Terminal::instance().addLog("[WEAPON] cannot fire - player is dead");
        return {};
    }
    if (player.equippedSlot != revolverDefinition.slot) {
        Terminal::instance().addLog("[WEAPON] cannot fire - equipped slot " + std::to_string(player.equippedSlot) + " != revolver slot " + std::to_string(revolverDefinition.slot));
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.2f, 0.65f});
        return {};
    }
    auto rtIt = player.weaponRuntimes.find("revolver");
    int currentAmmo = (rtIt != player.weaponRuntimes.end()) ? rtIt->second.currentAmmo : 0;
    if (reloadTimer > 0.0f || shotCooldown > 0.0f || currentAmmo <= 0) {
        Terminal::instance().addLog("[WEAPON] cannot fire - reload=" + std::to_string(reloadTimer) + " cooldown=" + std::to_string(shotCooldown) + " ammo=" + std::to_string(currentAmmo));
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.25f, 0.55f});
        return {};
    }
    RevolverShotResult result = revolver.fire(camera, player, npcs, world);
    if (result.fired) {
        shotCooldown = REVOLVER_SHOT_COOLDOWN;
        shootingTimer = 0.1f;
    }
    return result;
}

bool WeaponManager::reload(Player& player)
{
    if (player.equippedSlot != revolverDefinition.slot)
        return false;
    if (reloadTimer > 0.0f)
        return false;
    auto rtIt = player.weaponRuntimes.find("revolver");
    int currentAmmo = (rtIt != player.weaponRuntimes.end()) ? rtIt->second.currentAmmo : 0;
    int reserveAmmo = (rtIt != player.weaponRuntimes.end()) ? rtIt->second.reserveAmmo : 0;
    int needed = revolverDefinition.magazineSize - currentAmmo;
    int loaded = std::min(needed, reserveAmmo);
    if (loaded <= 0) {
        AudioManager::instance().play({"ui/click", AudioCategory::Weapons, false, {}, 0.25f, 0.7f});
        return false;
    }
    AudioManager::instance().play({"revolverpullback", AudioCategory::Weapons, false, {}, 0.8f});
    pendingReloadRounds = loaded;
    reloadTimer = REVOLVER_RELOAD_TIME;
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
    Terminal::instance().addLog("[WEAPON] active module: revolver (deprecated path)");
}

const std::vector<std::string>& WeaponManager::killfeed() const { return revolver.killfeed(); }

WeaponCrosshairState WeaponManager::crosshairState(const Player& player) const
{
    if (reloadTimer > 0.0f)
        return WeaponCrosshairState::Reloading;
    if (shotCooldown > 0.0f)
        return WeaponCrosshairState::Delay;
    return WeaponCrosshairState::Ready;
}