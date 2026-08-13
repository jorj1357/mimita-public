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

// Start a background reload for a holstered weapon. Returns true if a reload
// actually started (so the caller can tell the server too).
static bool tryAutoReloadOffhand(Player& player, const std::string& weaponId)
{
    if (weaponId.empty()) return false;
    auto it = player.weaponRuntimes.find(weaponId);
    if (it == player.weaponRuntimes.end()) return false;
    WeaponRuntime& rt = it->second;
    if (rt.isReloading) return false;
    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def) return false;
    WeaponRuntimeHelper::startReload(rt, *def);
    if (rt.isReloading) {
        if (!def->soundReload.empty())
            playWorldSound(def->soundReload, player.pos, 0.7f, 1.0f, 20.0f);
        if (DebugConfig::DEBUG_RELOAD)
            Debug::log(Debug::Category::General, "[RELOAD] auto-reload started for holstered '%s'\n",
                       weaponId.c_str());
        return true;
    }
    return false;
}

bool WeaponSystem::reload(Player& player) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return false;

    if (rt->isReloading) {
        if (rt->reloadTimer <= 0.0f) {
            // Stale "reloading" state with no timer (e.g. a delayed network
            // result re-stuck the weapon). Clear it and start a fresh reload
            // so pressing R always recovers.
            rt->isReloading = false;
            if (DebugConfig::DEBUG_RELOAD)
                Debug::log(Debug::Category::General, "[RELOAD] stale stuck reload cleared; restarting\n");
        } else {
            rt->reloadBufferTimer = 0.4f;
            if (DebugConfig::DEBUG_RELOAD)
                Debug::log(Debug::Category::General, "[RELOAD] buffered (already reloading)\n");
            return false;
        }
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

std::string WeaponSystem::equip(Player& player, int slot) {
    std::string oldWeaponId = player.equippedWeaponId;
    // The slot is the source of truth. After a spawn/boot/reset the id is
    // empty while equippedSlot still says we hold a weapon, so derive the id
    // from the slot to make the off-hand auto-reload work on every switch.
    if (oldWeaponId.empty() && player.equippedSlot > 0) {
        const WeaponDefinition* oldDef = getDefForSlot(player.equippedSlot);
        if (oldDef) oldWeaponId = oldDef->id;
    }
    const WeaponDefinition* def = getDefForSlot(slot);
    if (def) {
        // Switching away from a weapon starts its background reload (every time).
        std::string offhandReloaded;
        if (def->id != oldWeaponId && tryAutoReloadOffhand(player, oldWeaponId))
            offhandReloaded = oldWeaponId;
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
        return offhandReloaded;
    } else {
        unequip(player);
        return "";
    }
}

std::string WeaponSystem::unequip(Player& player) {
    std::string oldWeaponId = player.equippedWeaponId;
    if (oldWeaponId.empty() && player.equippedSlot > 0) {
        const WeaponDefinition* oldDef = getDefForSlot(player.equippedSlot);
        if (oldDef) oldWeaponId = oldDef->id;
    }
    std::string offhandReloaded;
    if (tryAutoReloadOffhand(player, oldWeaponId))
        offhandReloaded = oldWeaponId;
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
    return offhandReloaded;
}
