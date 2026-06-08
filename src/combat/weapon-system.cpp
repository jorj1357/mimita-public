#include "weapon-system.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-fire.h"
#include "weapon-registry.h"
#include "weapon-runtime.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "audio/audio.h"
#include "camera.h"
#include "config/player-settings.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "npc/npc.h"

WeaponSystem::WeaponSystem() {
    WeaponData::registerBuiltinWeapons();
    printf("[WEAPON SYSTEM] initialized\n");
}

const WeaponDefinition* WeaponSystem::getDefForSlot(int slot) const {
    if (slot <= 0) return nullptr;
    for (const auto& pair : WeaponRegistry::instance().all()) {
        if (pair.second.slot == slot) {
            return &pair.second;
        }
    }
    return nullptr;
}

const WeaponDefinition* WeaponSystem::getCurrentDef(const Player& player) const {
    return getDefForSlot(player.equippedSlot);
}

WeaponRuntime* WeaponSystem::getCurrentRuntime(Player& player) {
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return nullptr;
    auto it = player.weaponRuntimes.find(def->id);
    if (it != player.weaponRuntimes.end()) {
        return &it->second;
    }
    player.weaponRuntimes[def->id] = WeaponRuntime{};
    WeaponRuntime* rt = &player.weaponRuntimes[def->id];
    WeaponRuntimeHelper::initRuntime(*rt, *def);
    return rt;
}

void WeaponSystem::update(const Camera& camera, Player& player, NpcSystem& npcs, float dt) {
    mShotCooldown = std::max(0.0f, mShotCooldown - dt);
    mShootingTimer = std::max(0.0f, mShootingTimer - dt);
    mRecoilValue = std::max(0.0f, mRecoilValue - dt * 15.0f);
    mDisturbance = std::max(0.0f, mDisturbance - dt * 8.0f);

    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);

    if (def && rt) {
        if (rt->isReloading) {
            mReloadTimer = std::max(0.0f, mReloadTimer - dt);
            if (mReloadTimer <= 0.0f && mPendingReloadRounds > 0) {
                for (int i = 0; i < mPendingReloadRounds; ++i) {
                    playWorldSound("revolverbulletadd", player.pos, 0.65f, 1.0f, 10.0f);
                }
                rt->currentAmmo += mPendingReloadRounds;
                rt->reserveAmmo -= mPendingReloadRounds;
                mPendingReloadRounds = 0;
                rt->isReloading = false;
                mIsReloading = false;
                playWorldSound("revolverchamber", player.pos, 0.9f, 1.0f, 10.0f);
            }
        }

        rt->fireCooldown = std::max(0.0f, rt->fireCooldown - dt);
        rt->shootEffectTimer = std::max(0.0f, rt->shootEffectTimer - dt);

        int idx = slotIndex(def->slot);
        mViewModels[idx].update(camera, player, dt, def);

        if (def->behaviorType == WeaponBehaviorType::Godball) {
            if (!mGodballPhys.active) {
                WeaponGodball::spawnBall(mGodballPhys, *def, player);
            }
            WeaponGodball::updatePhysics(mGodballPhys, *def, *rt, player, camera, dt);
            // Continuous overlap damage every frame, not just on fire input
            WeaponGodball::checkOverlaps(mGodballPhys, *def, *rt, player, npcs, camera, dt);
        } else {
            if (mGodballPhys.active) {
                WeaponGodball::despawnBall(mGodballPhys);
            }
        }
    } else {
        if (mGodballPhys.active) {
            WeaponGodball::despawnBall(mGodballPhys);
        }
    }

    mCurrentSlot = player.equippedSlot;
}

void WeaponSystem::render(const Camera& camera, const Player& player) const {
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return;

    int idx = slotIndex(def->slot);
    mViewModels[idx].render(camera, player, def->slot);

    if (def->behaviorType == WeaponBehaviorType::Godball && mGodballPhys.active) {
        glm::vec3 handPos = WeaponGodball::getHandPosition(player);
        WeaponGodball::render(camera, mGodballPhys, handPos);
        if (DebugVis::enabled()) {
            // Need non-const access for runtime - use const_cast for render only
            auto it = player.weaponRuntimes.find(def->id);
            if (it != player.weaponRuntimes.end()) {
                WeaponGodball::renderDebug(camera, mGodballPhys, it->second, handPos);
            }
        }
    }
}

RevolverShotResult WeaponSystem::fire(const Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    if (player.dead) {
        Terminal::instance().addLog("[WEAPON] cannot fire - player is dead");
        return {};
    }

    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) {
        Terminal::instance().addLog("[WEAPON] no weapon equipped in slot " + std::to_string(player.equippedSlot));
        return {};
    }

    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!rt) return {};

    if (def->behaviorType == WeaponBehaviorType::Godball) {
        fireGodball(camera, player, npcs, world);
        return {};
    }

    if (rt->isReloading || rt->fireCooldown > 0.0f) {
        return {};
    }

    if (rt->currentAmmo <= 0) {
        WeaponAudio::playDryFireSound(*def);
        Terminal::instance().addLog("[WEAPON] out of ammo");
        return {};
    }

    return fireHitscan(camera, player, npcs, world);
}

RevolverShotResult WeaponSystem::fireHitscan(const Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return {};

    rt->currentAmmo--;
    rt->fireCooldown = def->fireDelay;
    mShotCooldown = def->fireDelay;
    mShootingTimer = 0.1f;

    int idx = slotIndex(def->slot);
    const WeaponViewModel& vm = mViewModels[idx];
    glm::vec3 muzzlePos = vm.muzzle;
    glm::vec3 muzzleDir = vm.forward;

    RevolverShotResult result = WeaponFire::tryFireHitscan(*def, *rt, camera, player, npcs, world, muzzlePos, muzzleDir);

    WeaponFire::applyRecoil(player, *def, result.end - muzzlePos, mRecoilValue, 1.0f / 60.0f);
    mDisturbance += 1.2f;

    printf("[WEAPON] hitscan fired: slot=%d weapon=%s ammo=%d\n",
           def->slot, def->id.c_str(), rt->currentAmmo);

    return result;
}

void WeaponSystem::fireGodball(const Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    // Overlap damage is handled continuously in update().
    // Fire input is a no-op for godball (always "automatic").
}

bool WeaponSystem::reload(Player& player) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return false;

    if (rt->isReloading) return false;
    if (def->behaviorType == WeaponBehaviorType::Godball) return false;

    int needed = def->magazineSize - rt->currentAmmo;
    int loaded = std::min(needed, rt->reserveAmmo);
    if (loaded <= 0) {
        WeaponAudio::playDryFireSound(*def);
        return false;
    }

    WeaponAudio::playReloadSound(*def);
    mPendingReloadRounds = loaded;
    mReloadTimer = def->reloadTime;
    rt->isReloading = true;
    mIsReloading = true;
    printf("[WEAPON] reload: loading %d rounds, timer=%.2f\n", loaded, def->reloadTime);
    return true;
}

void WeaponSystem::equip(Player& player, int slot) {
    player.equippedSlot = slot;
    mCurrentSlot = slot;

    const WeaponDefinition* def = getDefForSlot(slot);
    if (def) {
        mCurrentWeaponId = def->id;
        WeaponAudio::playEquipSound(*def);
        printf("[WEAPON] equipped '%s' slot %d\n", def->id.c_str(), slot);
    } else {
        mCurrentWeaponId.clear();
        printf("[WEAPON] equipped slot %d (no weapon)\n", slot);
    }
}

void WeaponSystem::unequip(Player& player) {
    player.equippedSlot = 0;
    mCurrentSlot = 0;
    if (mGodballPhys.active) {
        WeaponGodball::despawnBall(mGodballPhys);
    }
}

void WeaponSystem::inspect() const {
    if (mCurrentSlot > 0 && !mCurrentWeaponId.empty()) {
        const WeaponDefinition* def = WeaponRegistry::instance().get(mCurrentWeaponId);
        if (def) {
            Terminal::instance().addLog("[WEAPON] active: " + def->id + " (" + def->displayName + ")");
        } else {
            Terminal::instance().addLog("[WEAPON] active: " + mCurrentWeaponId);
        }
    } else {
        Terminal::instance().addLog("[WEAPON] no weapon equipped");
    }
}

WeaponCrosshairState WeaponSystem::crosshairState(const Player& player) const {
    if (mReloadTimer > 0.0f)
        return WeaponCrosshairState::Reloading;
    if (mShotCooldown > 0.0f)
        return WeaponCrosshairState::Delay;
    return WeaponCrosshairState::Ready;
}

void WeaponSystem::addKillLine(const std::string& line) {
    mKillfeed.push_back(line);
    if (mKillfeed.size() > 20)
        mKillfeed.erase(mKillfeed.begin());
}