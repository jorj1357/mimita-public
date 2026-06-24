#include "weapon-system.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-registry.h"
#include "weapon-godball.h"
#include "weapon-swordsword.h"

#include <cstdio>

#include "debug/debug-log.h"
#include "config.h"
#include "entities/player.h"

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
    const WeaponDefinition* def = getDefForSlot(slot);
    if (def) {
        player.equippedSlot = slot;
        player.hasValidWeapon = true;
        player.equippedWeaponId = def->id;
        mCurrentSlot = slot;
        mCurrentWeaponId = def->id;
        WeaponAudio::playEquipSound(*def);
        printf("[WEAPON] equipped '%s' slot %d\n", def->id.c_str(), slot);
        if (def->id == "aa12")
            Debug::log(Debug::Category::Weapons, "[AA12] Equipped: aa12 (slot %d)", slot);
    } else {
        unequip(player);
    }
}

void WeaponSystem::unequip(Player& player) {
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
}
