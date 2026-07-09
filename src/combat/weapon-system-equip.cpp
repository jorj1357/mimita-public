#include "weapon-system.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-registry.h"
#include "weapon-runtime.h"
#include "weapon-godball.h"
#include "weapon-swordsword.h"
#include "weapon-rocket-launcher.h"
#include "pobjects/persistent-physics.h"

#include <cstdio>

#include "debug/debug-log.h"
#include "config.h"
#include "audio/audio.h"
#include "entities/player.h"

static void tryAutoReloadOffhand(Player& player, const std::string& weaponId)
{
    if (weaponId.empty()) return;
    auto it = player.weaponRuntimes.find(weaponId);
    if (it == player.weaponRuntimes.end()) return;
    WeaponRuntime& rt = it->second;
    if (rt.isReloading) return;
    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def) return;
    WeaponRuntimeHelper::startReload(rt, *def);
    if (rt.isReloading) {
        if (!def->soundReload.empty())
            playWorldSound(def->soundReload, player.pos, 0.7f, 1.0f, 20.0f);
        if (DebugConfig::DEBUG_RELOAD)
            Debug::log(Debug::Category::General, "[RELOAD] auto-reload started for holstered '%s'\n",
                       weaponId.c_str());
    }
}

bool WeaponSystem::reload(Player& player) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return false;

    if (rt->isReloading) {
        rt->reloadBufferTimer = 0.4f;
        if (DebugConfig::DEBUG_RELOAD)
            Debug::log(Debug::Category::General, "[RELOAD] buffered (already reloading)\n");
        return false;
    }
    if (def->behaviorType == WeaponBehaviorType::Godball ||
        def->behaviorType == WeaponBehaviorType::Swordsword) return false;

    int needed = def->magazineSize - rt->currentAmmo;
    int loaded = std::min(needed, rt->reserveAmmo);
    if (loaded <= 0) {
        WeaponAudio::playDryFireSound(*def);
        return false;
    }

    WeaponAudio::playReloadSound(*def);
    rt->pendingReloadRounds = loaded;
    rt->reloadTimer = def->reloadTime;
    rt->isReloading = true;
    rt->reloadBufferTimer = 0.0f;
    if (DebugConfig::DEBUG_RELOAD)
        Debug::log(Debug::Category::General, "[RELOAD] started: %d rounds, timer=%.2f\n",
                   loaded, def->reloadTime);
    return true;
}

void WeaponSystem::equip(Player& player, int slot) {
    std::string oldWeaponId = player.equippedWeaponId;
    const WeaponDefinition* def = getDefForSlot(slot);
    if (def) {
        if (def->id != oldWeaponId)
            tryAutoReloadOffhand(player, oldWeaponId);
        player.equippedSlot = slot;
        player.hasValidWeapon = true;
        player.equippedWeaponId = def->id;
        mCurrentSlot = slot;
        mCurrentWeaponId = def->id;
        if (WeaponRuntime* rt = getCurrentRuntime(player)) {
            auto it = def->customParams.find("equipPoseTime");
            rt->customFloats["equipTimer"] =
                it != def->customParams.end() ? it->second : 0.18f;
        }
        WeaponAudio::playEquipSound(*def);
        printf("[WEAPON] equipped '%s' slot %d\n", def->id.c_str(), slot);
        if (def->id == "aa12")
            Debug::log(Debug::Category::Weapons, "[AA12] Equipped: aa12 (slot %d)", slot);
        if (def->id == "admin_revolver") {
            auto it = def->customParams.find("reserveAmmo");
            int reserve = (it != def->customParams.end()) ? (int)it->second : 0;
            Debug::warn(Debug::Category::Weapons,
                "\nDeveloper Weapon:\n"
                "Admin Revolver Equipped\n"
                "Configuration:\n"
                "  Fire Interval: %.4f\n"
                "  Spread: %.1f\n"
                "  Clip: %d\n"
                "  Reserve: %d\n",
                def->fireDelay, def->spread, def->magazineSize, reserve);
        }
    } else {
        unequip(player);
    }
}

void WeaponSystem::unequip(Player& player) {
    std::string oldWeaponId = player.equippedWeaponId;
    tryAutoReloadOffhand(player, oldWeaponId);
    player.equippedSlot = 0;
    player.hasValidWeapon = false;
    player.equippedWeaponId.clear();
    player.collision.hasWeaponCollisionCapsule = false;
    player.weaponCollisionName.clear();
    mCurrentSlot = 0;
    mCurrentWeaponId.clear();
    for (PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name == "leftArm" || part.name == "rightArm") {
            part.translationSpring = SpringState{};
            part.rotationSpring = SpringState{};
            part.pose = ProceduralPose{};
            part.perfectPose = ProceduralPose{};
        }
    }
    if (mGodballPhys.active) {
        WeaponGodball::despawnBall(mGodballPhys);
    }
    mSwordswordState = SwordswordState{};
    WeaponRocketLauncher::clear(mRocketState);
    PersistentPhysicsSystem::instance().clear();
}
