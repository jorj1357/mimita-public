#include "weapon-system.h"
#include "physics/movement/physics-collision.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-fire.h"
#include "weapon-grenade-launcher.h"
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

// Overlay triangle buffer for always-on-top rendering (shared with debug visuals)
struct TrailTriVert { glm::vec3 pos; glm::vec4 color; };
extern std::vector<TrailTriVert> gOverlayTriVerts;
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
        } else if (def->behaviorType == WeaponBehaviorType::GrenadeLauncher) {
            WeaponGrenadeLauncher::update(*def, *rt, player, npcs, world, camera, dt);
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

    // ── Off-hand reloads — tick every reloading weapon regardless of equip ──
    std::string equippedId = def ? def->id : "";
    for (auto& [wepId, wepRt] : player.weaponRuntimes) {
        if (wepId == equippedId) continue;
        if (!wepRt.isReloading) continue;
        const WeaponDefinition* wepDef = WeaponRegistry::instance().get(wepId);
        if (!wepDef) continue;
        WeaponRuntimeHelper::tickReload(wepRt, *wepDef, dt);
    }

    // ── World-space aim crosshair (permanent) ──────────────
    if (def && rt) {
        int idx = slotIndex(def->slot);
        const WeaponViewModel& vm = mViewModels[idx];
        glm::vec3 muzzlePos = vm.muzzle;

        WeaponFire::AimSolution aim = WeaponFire::computeAim(
            camera, world, npcs, muzzlePos, nullptr);
        glm::vec3 shotDir = aim.direction;

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

        // In crosshair mode the reticle is the camera-ray target. In world_hit
        // mode it keeps the old predicted muzzle-impact behavior.
        glm::vec3 crossPos = aim.usesCameraTarget ? aim.aimPoint : hitPoint;
        float crossDistance = aim.usesCameraTarget ? aim.cameraDistance : nearest;

        // ── World-space aim trail ───────────────────────────────
        int trailInterval = DebugConfig::WORLD_XH_TRAIL_SPAWN_INTERVAL;
        if (DebugConfig::WORLD_XH_TRAIL_ENABLED && trailInterval > 0 && (mTrailTick % trailInterval) == 0)
        {
            int maxPts = glm::clamp(DebugConfig::WORLD_XH_TRAIL_MAX_POINTS, 1, MAX_TRAIL_POINTS);
            mTrailPoints[mTrailHead] = {crossPos, hitNormal, mTrailTick};
            mTrailHead = (mTrailHead + 1) % maxPts;
            if (mTrailCount < maxPts) mTrailCount++;
        }
        mTrailTick++;

        // Visibility ray from camera to crossPos: if something is between
        // the camera and the hit point, the crosshair is hidden. Otherwise
        // it renders as an overlay (not occluded by the hit surface itself).
        bool crosshairVisible = true;
        {
            glm::vec3 camToHit = crossPos - camera.pos;
            float camDist = glm::length(camToHit);
            if (camDist > 0.5f)
            {
                glm::vec3 camDir = camToHit / camDist;
                // Check world triangles between camera and hit point
                AABB rayBounds;
                rayBounds.min = glm::min(camera.pos, crossPos);
                rayBounds.max = glm::max(camera.pos, crossPos);
                std::vector<int> candidates;
                appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates);
                for (int ti : candidates)
                {
                    if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
                    float d = 0.0f;
                    if (WeaponFire::rayTriangle(camera.pos, camDir, world.collisionMesh.triangles[ti], d) && d < camDist - 0.3f)
                    {
                        crosshairVisible = false;
                        break;
                    }
                }
                // Check NPCs between camera and hit point
                if (crosshairVisible)
                {
                    for (Npc& npc : npcs.all())
                    {
                        if (npc.body.currentHp <= 0) continue;
                        npc.body.updateModelWorldTransforms();
                        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts)
                        {
                            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                            glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                            float d = 0.0f;
                            glm::vec3 nml;
                            if (WeaponFire::rayAabb(camera.pos, camDir, center - half, center + half, d, nml) && d < camDist - 0.3f)
                            {
                                crosshairVisible = false;
                                break;
                            }
                        }
                        if (!crosshairVisible) break;
                    }
                }
            }
        }

        // ── Distance scaling ──
        // world_xh_maxsize → size at close range (maximum rendered size)
        // world_xh_minsize  → size at far  range (minimum rendered size)
        // Each variable independently controls its own end of the interpolation.
        // No swap: changing one never affects the other end.
        constexpr float NEAR_DIST = 5.0f;
        float closeSize = DebugConfig::WORLD_XH_MAXSIZE;
        float farSize   = DebugConfig::WORLD_XH_MINSIZE;

        float distScale = 1.0f;
        if (DebugConfig::WORLD_XH_DYNAMIC) {
            float t = glm::clamp((crossDistance - NEAR_DIST) / (MAX_DIST - NEAR_DIST), 0.0f, 1.0f);
            distScale = glm::mix(closeSize, farSize, t);
        }

        // ── Base dimensions ──
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
        bool showDot = DebugConfig::WORLD_XH_CENTERDOT;

        float sz = baseSize * distScale * lenMul;
        float gp = baseGap * distScale * gapMul;
        float tk = baseThickness * distScale * thickMul;

        // ── Outline pass (drawn first, behind main crosshair) ──
        if (DebugConfig::WORLD_XH_OUTLINE && crosshairVisible) {
            float oAlpha = 1.0f - DebugConfig::WORLD_XH_OUTLINE_ALPHA;
            glm::vec4 oCol(0.0f, 0.0f, 0.0f, oAlpha);
            float outlineWidth = tk * 0.35f;
            DebugVis::drawCrosshairBillboardOverlay(camera, crossPos,
                sz + outlineWidth,
                gp + outlineWidth,
                tk + outlineWidth * 2.0f,
                showDot, oCol);
        }

        // ── Main crosshair pass (drawn second, on top of outline) ──
        if (crosshairVisible)
            DebugVis::drawCrosshairBillboardOverlay(camera, crossPos,
                sz, gp, tk, showDot, col);

        // Debug shot line + sphere — only when wpn_shot_line is enabled
        if (DebugConfig::DEBUG_WPN_SHOT_LINE) {
            float visThickness = (def && def->beamThickness > 0.0f) ? def->beamThickness : 0.05f;
            DebugVis::drawFilledBeam(camera, muzzlePos, hitPoint, visThickness, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            DebugVis::drawFilledSphere(camera, hitPoint, 0.08f, glm::vec4(1.0f, 0.0f, 0.0f, 0.7f));
        }

        // ── Render aim trail ─────────────────────────────────────
        if (DebugConfig::WORLD_XH_TRAIL_ENABLED)
        {
            int maxPts = glm::clamp(DebugConfig::WORLD_XH_TRAIL_MAX_POINTS, 1, MAX_TRAIL_POINTS);
            int lifetimeTicks = std::max(1, DebugConfig::WORLD_XH_TRAIL_LIFETIME_TICKS);
            float trailAlpha = glm::clamp(DebugConfig::WORLD_XH_TRAIL_ALPHA, 0.0f, 1.0f);
            float trailSize = std::max(0.01f, DebugConfig::WORLD_XH_TRAIL_SIZE);
            glm::vec4 trailCol(
                DebugConfig::WORLD_XH_TRAIL_R,
                DebugConfig::WORLD_XH_TRAIL_G,
                DebugConfig::WORLD_XH_TRAIL_B,
                trailAlpha);
            bool fade = DebugConfig::WORLD_XH_TRAIL_FADE;
            int shape = DebugConfig::WORLD_XH_TRAIL_SHAPE;
            int mode = DebugConfig::WORLD_XH_TRAIL_MODE;

            auto pushTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec4& col) {
                gOverlayTriVerts.push_back({a, col});
                gOverlayTriVerts.push_back({b, col});
                gOverlayTriVerts.push_back({c, col});
            };
            auto billboardAxes = [&](const TrailPoint& pt, float& outSx, float& outSy, glm::vec3& outRight, glm::vec3& outUp) {
                outSx = trailSize;
                outSy = trailSize;
                if (mode == 1 && glm::dot(pt.normal, pt.normal) > 0.001f) {
                    glm::vec3 n = glm::normalize(pt.normal);
                    glm::vec3 worldUp(0.0f, 0.0f, 1.0f);
                    if (std::fabs(glm::dot(n, worldUp)) > 0.99f)
                        worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
                    outRight = glm::normalize(glm::cross(n, worldUp));
                    outUp = glm::normalize(glm::cross(outRight, n));
                } else {
                    outRight = camera.right;
                    outUp = camera.up;
                }
            };
            int segCount = (shape == 0) ? 16 : (shape == 3) ? 5 : 4;

            for (int i = 0; i < mTrailCount; ++i)
            {
                int idx = (mTrailHead - 1 - i + maxPts) % maxPts;
                const TrailPoint& pt = mTrailPoints[idx];
                int age = mTrailTick - pt.spawnTick;
                if (age >= lifetimeTicks) continue;

                float alpha = trailAlpha;
                if (fade)
                    alpha *= 1.0f - (float)age / (float)lifetimeTicks;

                trailCol.a = alpha;

                glm::vec3 rAxis, uAxis;
                float sx, sy;
                billboardAxes(pt, sx, sy, rAxis, uAxis);
                glm::vec3 r = rAxis * sx;
                glm::vec3 u = uAxis * sy;

                if (shape == 1) {
                    // Box: 2-tri quad
                    glm::vec3 v0 = pt.pos - r - u;
                    glm::vec3 v1 = pt.pos + r - u;
                    glm::vec3 v2 = pt.pos + r + u;
                    glm::vec3 v3 = pt.pos - r + u;
                    pushTri(v0, v1, v2, trailCol);
                    pushTri(v0, v2, v3, trailCol);
                } else if (shape == 3) {
                    // Star 2D (5-pointed)
                    float inner = 0.4f;
                    for (int j = 0; j < segCount; ++j) {
                        float a1 = (float)j * 1.256637f - 1.570796f;
                        float a2 = a1 + 1.256637f;
                        float aMid = a1 + 0.6283185f;
                        glm::vec3 outer1 = pt.pos + (r * std::cos(a1) + u * std::sin(a1));
                        glm::vec3 outer2 = pt.pos + (r * std::cos(a2) + u * std::sin(a2));
                        glm::vec3 innerP = pt.pos + (r * std::cos(aMid) + u * std::sin(aMid)) * inner;
                        pushTri(outer1, innerP, outer2, trailCol);
                    }
                } else {
                    // Circle (and fallback): fan of triangles
                    for (int j = 0; j < segCount; ++j) {
                        float a1 = (float)j * 6.2831855f / (float)segCount;
                        float a2 = (float)(j + 1) * 6.2831855f / (float)segCount;
                        glm::vec3 e1 = pt.pos + (r * std::cos(a1) + u * std::sin(a1));
                        glm::vec3 e2 = pt.pos + (r * std::cos(a2) + u * std::sin(a2));
                        pushTri(pt.pos, e1, e2, trailCol);
                    }
                }
            }
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
        if (!rt->isReloading && rt->reserveAmmo > 0) {
            reload(player);
        }
        return {};
    }

    if (def->behaviorType == WeaponBehaviorType::GrenadeLauncher) {
        rt->currentAmmo--;
        rt->fireCooldown = def->fireDelay;
        rt->shootEffectTimer = weaponParamOr(*def, "shootPoseTime", 0.12f);
        mShotCooldown = def->fireDelay;
        WeaponFire::applyRecoil(player, *def,
            camera.front, mRecoilValue, 1.0f / 60.0f);
        mDisturbance += 1.2f;
        AnalyticsManager::instance().trackWeaponUsed(def->id);

        int idx = slotIndex(def->slot);
        const WeaponViewModel& vm = mViewModels[idx];
        glm::vec3 muzzlePos = vm.muzzle;
        glm::vec3 muzzleDir = vm.forward;
        WeaponFire::AimSolution aim = WeaponFire::computeAim(
            camera, world, npcs, muzzlePos, nullptr);
        glm::vec3 dir = aim.direction;
        float recoilStrength = def->customParams.count("firingRecoilStrength")
            ? def->customParams.at("firingRecoilStrength") : 20.0f;
        player.externalImpulse -= dir * recoilStrength;
        WeaponGrenadeLauncher::fire(*def, *rt, player, muzzlePos, dir);

        Debug::log(Debug::Category::Weapons, "[GRENADE LAUNCHER] fired ammo=%d/%d pos=(%.2f %.2f %.2f) dir=(%.2f %.2f %.2f)\n",
                   rt->currentAmmo, rt->reserveAmmo,
                   muzzlePos.x, muzzlePos.y, muzzlePos.z,
                   dir.x, dir.y, dir.z);
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
        if (def->id == "admin_revolver")
            Debug::log(Debug::Category::Weapons,
                "[ADMIN REVOLVER] Shots Fired  Current Clip: %d  Fire Interval: %.4f  Spread: %.1f\n",
                rt->currentAmmo, def->fireDelay, def->spread);

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

    WeaponFire::AimSolution aim = WeaponFire::computeAim(
        camera, world, npcs, muzzlePos, nullptr);
    WeaponFire::logAimDebug("rocket_launcher", camera, aim);
    glm::vec3 dir = aim.direction;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        dir.x, dir.y, dir.z);

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
