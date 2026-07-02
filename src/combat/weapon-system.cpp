#include "weapon-system.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-fire.h"
#include "weapon-registry.h"
#include "weapon-runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "audio/audio.h"
#include "analytics/analytics-manager.h"
#include "camera.h"
#include "config.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"

static float weaponParamOr(const WeaponDefinition& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

WeaponSystem::WeaponSystem() {
    WeaponData::registerBuiltinWeapons();
    Debug::log(Debug::Category::Weapons, "[WEAPON SYSTEM] initialized");
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

void WeaponSystem::update(Camera& camera, Player& player, NpcSystem& npcs, const World& world, float dt) {
    if (WeaponData::reloadBuiltinWeaponsIfChanged())
        Terminal::instance().addLog("[WEAPON] Reloaded config/weapons.json");

    mShotCooldown = std::max(0.0f, mShotCooldown - dt);
    mShootingTimer = std::max(0.0f, mShootingTimer - dt);
    mRecoilValue = std::max(0.0f, mRecoilValue - dt * 15.0f);
    mDisturbance = std::max(0.0f, mDisturbance - dt * 8.0f);

    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);

    if (def && rt) {
        if (rt->isReloading) {
            rt->reloadTimer = std::max(0.0f, rt->reloadTimer - dt);
            if (rt->reloadTimer <= 0.0f && rt->pendingReloadRounds > 0) {
                rt->currentAmmo += rt->pendingReloadRounds;
                rt->reserveAmmo -= rt->pendingReloadRounds;
                rt->pendingReloadRounds = 0;
                rt->isReloading = false;
                if (def && !def->soundReload.empty())
                    playWorldSound(def->soundReload, player.pos, 0.9f, 1.0f, 10.0f);
            }
        }

        if (rt->reloadBufferTimer > 0.0f) {
            if (!rt->isReloading) {
                rt->reloadBufferTimer = 0.0f;
                if (DebugConfig::DEBUG_RELOAD)
                    Debug::log(Debug::Category::General, "[RELOAD] buffer consumed -> auto-start\n");
                reload(player);
            } else {
                rt->reloadBufferTimer = std::max(0.0f, rt->reloadBufferTimer - dt);
                if (rt->reloadBufferTimer <= 0.0f && DebugConfig::DEBUG_RELOAD)
                    Debug::log(Debug::Category::General, "[RELOAD] buffer expired\n");
            }
        }

        rt->fireCooldown = std::max(0.0f, rt->fireCooldown - dt);
        rt->shootEffectTimer = std::max(0.0f, rt->shootEffectTimer - dt);
        auto equipTimer = rt->customFloats.find("equipTimer");
        if (equipTimer != rt->customFloats.end())
            equipTimer->second = std::max(0.0f, equipTimer->second - dt);

        int idx = slotIndex(def->slot);
        if (DebugConfig::DEBUG_WEAPON_VIEWMODEL)
            printf("[VMTRACE] WeaponSystem::update calling mViewModels[%d].update for %s\n", idx, def->id.c_str());
        mViewModels[idx].update(camera, player, dt, def, true, &world);
        if (DebugConfig::DEBUG_WEAPON_VIEWMODEL)
            printf("[VMTRACE] WeaponSystem::update done for %s (slot=%d idx=%d)\n", def->id.c_str(), def->slot, idx);

        if (def->behaviorType == WeaponBehaviorType::Godball) {
            if (!mGodballPhys.active) {
                WeaponGodball::spawnBall(mGodballPhys, *def, player);
            }
            WeaponGodball::updatePhysics(mGodballPhys, *def, *rt, player, camera, dt);
            // Continuous overlap damage every frame, not just on fire input
            WeaponGodball::checkOverlaps(mGodballPhys, *def, *rt, player, npcs, camera, dt);
        } else if (def->behaviorType == WeaponBehaviorType::Swordsword) {
            WeaponSwordsword::update(mSwordswordState, *def, *rt, player, camera, npcs, dt);
        } else if (def->behaviorType == WeaponBehaviorType::RocketLauncher) {
            WeaponRocketLauncher::update(mRocketState, *def, *rt, player, npcs, world, camera, dt);
        } else {
            if (mGodballPhys.active) {
                WeaponGodball::despawnBall(mGodballPhys);
            }
        }
    } else {
        player.collision.hasWeaponCollisionCapsule = false;
        player.weaponCollisionName.clear();
        if (mGodballPhys.active) {
            WeaponGodball::despawnBall(mGodballPhys);
        }
    }

    // ── World-space aim crosshair (permanent) ──────────────
    if (def && rt) {
        int idx = slotIndex(def->slot);
        const WeaponViewModel& vm = mViewModels[idx];
        glm::vec3 muzzlePos = vm.muzzle;

        constexpr float AIM_DISTANCE = 100.0f;
        glm::vec3 aimPoint = camera.pos + camera.front * AIM_DISTANCE;
        glm::vec3 shotDir = glm::normalize(aimPoint - muzzlePos);

        constexpr float MAX_DIST = 100.0f;
        float nearest = MAX_DIST;
        glm::vec3 hitPoint = muzzlePos + shotDir * MAX_DIST;
        glm::vec3 hitNormal = glm::vec3(0.0f, 0.0f, 1.0f);

        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            float d = 0.0f;
            if (WeaponFire::rayTriangle(muzzlePos, shotDir, tri, d) && d < nearest) {
                nearest = d;
                hitPoint = muzzlePos + shotDir * d;
                hitNormal = tri.normal;
            }
        }
        for (Npc& npc : npcs.all()) {
            if (npc.body.currentHp <= 0) continue;
            npc.body.updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                if (WeaponFire::rayAabb(muzzlePos, shotDir, center - half, center + half, d, nml) && d < nearest) {
                    nearest = d;
                    hitPoint = muzzlePos + shotDir * d;
                    hitNormal = nml;
                }
            }
        }

        // World-space crosshair: a billboard crosshair at the predicted hit point,
        // always rendered on top of world geometry (depth-test disabled).
        glm::vec3 crossPos = hitPoint + hitNormal * 0.003f;

        float distScale = 1.0f;
        if (DebugConfig::WORLD_XH_DYNAMIC) {
            float raw = 20.0f / std::max(nearest, 1.0f);
            distScale = glm::clamp(raw, DebugConfig::WORLD_XH_MINSIZE, DebugConfig::WORLD_XH_MAXSIZE);
        }

        const float baseSize = 0.15f;
        const float baseGap = 0.025f;
        const float baseThickness = 0.025f;

        float lenMul = DebugConfig::WORLD_XH_LENGTH;
        float gapMul = DebugConfig::WORLD_XH_GAP;
        float thickMul = DebugConfig::WORLD_XH_THICKNESS;
        float alpha = 1.0f - DebugConfig::WORLD_XH_ALPHA;
        glm::vec4 col = glm::vec4(
            DebugConfig::WORLD_XH_R,
            DebugConfig::WORLD_XH_G,
            DebugConfig::WORLD_XH_B,
            alpha);

        DebugVis::drawCrosshairBillboardOverlay(camera, crossPos,
            baseSize * distScale * lenMul,
            baseGap * distScale * gapMul,
            baseThickness * distScale * thickMul,
            true, col);

        // Outline pass (if enabled)
        if (DebugConfig::WORLD_XH_OUTLINE) {
            float oAlpha = 1.0f - DebugConfig::WORLD_XH_OUTLINE_ALPHA;
            float outlineW = baseThickness * distScale * thickMul * 0.3f;
            DebugVis::drawCrosshairBillboardOverlay(camera, crossPos,
                baseSize * distScale * lenMul + outlineW,
                baseGap * distScale * gapMul,
                baseThickness * distScale * thickMul + outlineW * 2.0f,
                false, glm::vec4(0.0f, 0.0f, 0.0f, oAlpha));
        }

        // Debug shot line + sphere — only when wpn_shot_line is enabled
        if (DebugConfig::DEBUG_WPN_SHOT_LINE) {
            DebugVis::drawFilledBeam(camera, muzzlePos, hitPoint, 0.05f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            DebugVis::drawFilledSphere(camera, hitPoint, 0.08f, glm::vec4(1.0f, 0.0f, 0.0f, 0.7f));
        }
    }

    mCurrentSlot = player.equippedSlot;
}

void WeaponSystem::render(const Camera& camera, const Player& player) const {
    const WeaponDefinition* def = getCurrentDef(player);
    if (DebugConfig::DEBUG_WEAPON_VIEWMODEL)
        printf("[VMTRACE] WeaponSystem::render: def=%p id=%s slot=%d\n",
               (void*)def, def ? def->id.c_str() : "(null)", def ? def->slot : -1);
    if (!def) return;

    int idx = slotIndex(def->slot);
    if (DebugConfig::DEBUG_WEAPON_VIEWMODEL)
        printf("[VMTRACE] WeaponSystem::render calling mViewModels[%d].render\n", idx);
    mViewModels[idx].render(camera, player, def->slot);
    if (DebugConfig::DEBUG_WEAPON_VIEWMODEL)
        printf("[VMTRACE] WeaponSystem::render done\n");

    if (def->behaviorType == WeaponBehaviorType::Godball && mGodballPhys.active) {
        glm::vec3 handPos = WeaponGodball::getHandPosition(player);
        WeaponGodball::render(camera, mGodballPhys, handPos);
        if (DebugVis::enabled() || DebugConfig::DEBUG_GODBALL) {
            auto it = player.weaponRuntimes.find(def->id);
            if (it != player.weaponRuntimes.end()) {
                WeaponGodball::renderDebug(camera, mGodballPhys, it->second, handPos);
            }
        }
    }
    if (def->behaviorType == WeaponBehaviorType::Swordsword) {
        WeaponSwordsword::render(camera, mSwordswordState, mSwordswordState.handPos);
        if (DebugConfig::DEBUG_SWORDSWORD) {
            WeaponSwordsword::renderDebug(camera, mSwordswordState, mSwordswordState.handPos);
        }
    }
}

RevolverShotResult WeaponSystem::fire(
    Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world,
    const std::unordered_map<uint32_t, Player>* remotePlayers) {
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

    if (def->behaviorType == WeaponBehaviorType::Swordsword) {
        fireSwordsword(camera, player, npcs);
        return {};
    }

    if (def->behaviorType == WeaponBehaviorType::RocketLauncher) {
        fireRocketLauncher(camera, player, npcs, world);
        return {};
    }

    if (rt->isReloading || rt->fireCooldown > 0.0f) {
        return {};
    }

    if (rt->currentAmmo <= 0) {
        WeaponAudio::playDryFireSound(*def);
        Terminal::instance().addLog("[WEAPON] out of ammo");
        if (!rt->isReloading && rt->reserveAmmo > 0) {
            reload(player);
        }
        return {};
    }

    return fireHitscan(camera, player, npcs, world, remotePlayers);
}

RevolverShotResult WeaponSystem::fireHitscan(
    const Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world,
    const std::unordered_map<uint32_t, Player>* remotePlayers) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return {};

    rt->currentAmmo--;
    rt->fireCooldown = def->fireDelay;
    rt->shootEffectTimer = weaponParamOr(*def, "shootPoseTime", 0.12f);
    mShotCooldown = def->fireDelay;
    mShootingTimer = 0.1f;

    int idx = slotIndex(def->slot);
    const WeaponViewModel& vm = mViewModels[idx];
    glm::vec3 muzzlePos = vm.muzzle;
    glm::vec3 muzzleDir = vm.forward;

    RevolverShotResult result;

    if (def->pelletCount > 1) {
        WeaponFire::fireMultiPellet(
            *def, *rt, camera, player, npcs, world, muzzlePos, muzzleDir,
            remotePlayers, result);
    } else {
        result = WeaponFire::tryFireHitscan(
            *def, *rt, camera, player, npcs, world, muzzlePos, muzzleDir,
            remotePlayers);
    }

    WeaponFire::applyRecoil(player, *def, result.end - muzzlePos, mRecoilValue, 1.0f / 60.0f);
    mDisturbance += 1.2f;
    AnalyticsManager::instance().trackWeaponUsed(def->id);

        Debug::log(Debug::Category::Weapons, "[WEAPON] hitscan fired: slot=%d weapon=%s ammo=%d",
           def->slot, def->id.c_str(), rt->currentAmmo);
        if (def->id == "aa12")
            Debug::log(Debug::Category::Weapons, "[AA12] Fired: ammo=%d, cooldown=%.2f", rt->currentAmmo, rt->fireCooldown);

    return result;
}

std::vector<RevolverShotResult> WeaponSystem::collectRemoteGodballHits(
    Player& player,
    const std::unordered_map<uint32_t, Player>& remotePlayers,
    float dt)
{
    std::vector<RevolverShotResult> hits;
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def || def->behaviorType != WeaponBehaviorType::Godball ||
        !mGodballPhys.active)
        return hits;

    for (auto& entry : mRemoteGodballCooldowns)
        entry.second = std::max(0.0f, entry.second - dt);

    const float tickInterval = def->customParams.count("damageTickInterval")
        ? def->customParams.at("damageTickInterval") : 0.1f;

    for (const auto& entry : remotePlayers)
    {
        const uint32_t targetId = entry.first;
        const Player& target = entry.second;
        if (target.dead || target.currentHp <= 0 ||
            mRemoteGodballCooldowns[targetId] > 0.0f)
            continue;

        const glm::vec3 toTarget = target.pos - mGodballPhys.position;
        const float distance = glm::length(toTarget);
        if (distance >= mGodballPhys.radius + 0.65f)
            continue;

        const glm::vec3 direction = distance > 0.001f
            ? toTarget / distance
            : glm::vec3(0.0f, 1.0f, 0.0f);
        const int damage = std::clamp(
            (int)std::round(WeaponGodball::computeDamage(
                mGodballPhys, *def, player, target,
                mGodballPhys.position + direction * mGodballPhys.radius)),
            1, 200);

        RevolverShotResult hit;
        hit.fired = true;
        hit.hitEntity = true;
        hit.targetIsRemotePlayer = true;
        hit.targetId = targetId;
        hit.damage = (float)damage;
        hit.start = WeaponGodball::getHandPosition(player);
        hit.end = target.pos + glm::vec3(0.0f, 0.0f, 0.8f);
        hits.push_back(hit);
        mRemoteGodballCooldowns[targetId] = tickInterval;

        hitmarker(damage);
        {
            HitEvent ev;
            ev.position = hit.end;
            ev.normal = direction;
            ev.direction = direction;
            ev.hitEntity = true;
            ev.damage = damage;
            ev.attacker = player.username;
            ev.victim = target.username;
            ev.weaponSource = "godball_remote";
            HitEffects::onHit(ev);
        }
        WeaponAudio::playGodballImpact(
            hit.end, std::clamp(hit.damage / 100.0f, 0.0f, 1.0f));
    }

    return hits;
}

void WeaponSystem::fireRocketLauncher(Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return;

    if (rt->isReloading || rt->fireCooldown > 0.0f) {
        // Dry-fire on cooldown (heavy click, lower pitch)
        playWorldSound("ui/click", player.pos, 0.4f, 0.75f, 10.0f);
        return;
    }

    if (rt->currentAmmo <= 0) {
        // Heavy dry-fire for out of ammo (lower pitch)
        playWorldSound("ui/click", player.pos, 0.4f, 0.75f, 10.0f);
        if (!rt->isReloading && rt->reserveAmmo > 0)
            reload(player);
        return;
    }

    int idx = slotIndex(def->slot);
    const WeaponViewModel& vm = mViewModels[idx];
    glm::vec3 muzzlePos = vm.muzzle;

    // Shared aiming: fixed point 100m in front of camera (same as all other weapons)
    constexpr float AIM_DISTANCE = 100.0f;
    glm::vec3 aimPoint = camera.pos + camera.front * AIM_DISTANCE;
    glm::vec3 dir = glm::normalize(aimPoint - muzzlePos);

    // Firing recoil: push player opposite the rocket direction
    float recoilStrength = def->customParams.count("firingRecoilStrength")
        ? def->customParams.at("firingRecoilStrength") : 30.0f;
    player.externalImpulse -= dir * recoilStrength;

    WeaponRocketLauncher::fire(mRocketState, *def, *rt, player, muzzlePos, dir);
    rt->shootEffectTimer = weaponParamOr(*def, "shootPoseTime", 0.12f);
    mShotCooldown = def->fireDelay;
    AnalyticsManager::instance().trackWeaponUsed(def->id);
}

void WeaponSystem::fireGodball(Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    // Overlap damage is handled continuously in update().
    // Fire input is a no-op for godball (always "automatic").
}

void WeaponSystem::fireSwordsword(Camera& camera, Player& player, NpcSystem& npcs) {
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return;

    if (rt->isReloading || rt->fireCooldown > 0.0f) return;

    if (mSwordswordState.currentAttack != SwordswordState::AttackType::None) return;

    rt->fireCooldown = def->fireDelay;
    rt->shootEffectTimer = weaponParamOr(*def, "shootPoseTime", 0.12f);
    mShotCooldown = def->fireDelay;

    WeaponSwordsword::startSlash(mSwordswordState, *def, player, camera);
    AnalyticsManager::instance().trackWeaponUsed(def->id);

    if (!def->soundShoot.empty()) {
        WeaponAudio::playShootSound(*def, player.pos);
    }
}

RevolverShotResult WeaponSystem::fireAlt(
    Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world) {
    if (player.dead) return {};

    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return {};

    if (def->behaviorType != WeaponBehaviorType::Swordsword) return {};

    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!rt) return {};

    if (rt->isReloading || rt->fireCooldown > 0.0f) return {};

    if (mSwordswordState.currentAttack != SwordswordState::AttackType::None) return {};

    rt->fireCooldown = def->customParams.count("lungeCooldown")
        ? def->customParams.at("lungeCooldown") : 0.5f;
    mShotCooldown = rt->fireCooldown;

    WeaponSwordsword::startLunge(mSwordswordState, *def, player, camera);
    AnalyticsManager::instance().trackWeaponUsed(def->id);

    if (!def->soundShoot.empty()) {
        WeaponAudio::playShootSound(*def, player.pos);
    }

    RevolverShotResult res;
    res.fired = true;
    return res;
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

bool WeaponSystem::isReloading(const Player& player) const {
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return false;
    auto it = player.weaponRuntimes.find(def->id);
    return it != player.weaponRuntimes.end() && it->second.isReloading;
}

WeaponCrosshairState WeaponSystem::crosshairState(const Player& player) const {
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return WeaponCrosshairState::Ready;
    auto it = player.weaponRuntimes.find(def->id);
    if (it != player.weaponRuntimes.end()) {
        if (it->second.isReloading)
            return WeaponCrosshairState::Reloading;
        if (it->second.fireCooldown > 0.0f)
            return WeaponCrosshairState::Delay;
    }
    return WeaponCrosshairState::Ready;
}

void WeaponSystem::addKillLine(const std::string& line) {
    mKillfeed.push_back(line);
    if (mKillfeed.size() > 20)
        mKillfeed.erase(mKillfeed.begin());
}

void WeaponSystem::renderRemoteWeapon(const Player& player, const Camera& camera) {
    if (player.dead) return;
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return;
    int idx = slotIndex(def->slot);
    mViewModels[idx].update(camera, const_cast<Player&>(player), 0.0f, def, true);
    mViewModels[idx].render(camera, player, def->slot);
}
