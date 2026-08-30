// 07 21 2026, 23 35
/* purpose
* Owns local weapon runtime, viewmodel updates, firing entrypoints, reload, equip, and rendering.
* Routes local and multiplayer weapon presentation through shared weapon definitions.
* Keeps single-player projectile behavior while letting multiplayer projectiles use network prediction.
* Does NOT validate server damage, packet authority, auth state, or remote player ownership.
* Does NOT own packet serialization, server projectile simulation, or multiplayer transport.
* Does NOT define weapon JSON parsing, collision mesh loading, or world tick scheduling.
*/

#include "weapon-system.h"
#include "physics/movement/physics-collision.h"
#include "weapon-audio.h"
#include "weapon-data.h"
#include "weapon-fire.h"
#include "weapon-grenade-launcher.h"
#include "weapon-registry.h"
#include "weapon-runtime.h"
#include "weapon-collision-config.h"
#include "combat/weapon-model-cache.h"
#include "combat/projectile-render.h"
#include "pobjects/persistent-physics.h"
#include "network/packets.h"
#include "debug/debug-visuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "audio/audio.h"
#include "analytics/analytics-manager.h"
#include "camera.h"
#include "config.h"
#include "config/player-settings.h"
#include "config/gameplay-config.h"
#include "crosshair/crosshair-config.h"
#include "debug/debug-log.h"

// Overlay triangle buffer for always-on-top rendering (shared with debug visuals)
struct TrailTriVert { glm::vec3 pos; glm::vec4 color; };
extern std::vector<TrailTriVert> gOverlayTriVerts;

bool gWeaponCollisionVisualsJson = false;
bool gWeaponCollisionVisualsProbes = false;
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"
#include "config/networking-config.h"
#include "network/multiplayer-context.h"

extern MimitaNet::MultiplayerContext* gpMpContext;

namespace {

bool serverAuthHits()
{
    if (!gpMpContext || !gpMpContext->active) return false;
    return NetworkingConfig::instance().data().serverAuthoritativeHits.enabled;
}

} // anonymous namespace

static float weaponParamOr(const WeaponDefinition& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

WeaponSystem::WeaponSystem() {
    WeaponData::registerBuiltinWeapons();
    Debug::log(Debug::Category::Weapons, "[WEAPON SYSTEM] initialized");
    printf("[WORLD XH] enabled=%d\n", (int)DebugConfig::WORLD_XH_ENABLED);
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

void WeaponSystem::update(Camera& camera, Player& player, NpcSystem& npcs, const World& world,
                          std::unordered_map<uint32_t, Player>* remoteNpcs, float dt) {
    // Upload any finished background weapon model parses (must run on the main
    // thread) before viewmodels try to adopt them this frame.
    WeaponModelCache::instance().finalizeWeaponModelsIfReady();

    if (WeaponData::reloadBuiltinWeaponsIfChanged())
        Terminal::instance().addLog("[WEAPON] Reloaded config/weapons.json");

    // Hot reload weapon collision config
    WeaponCollisionJsonConfig::instance().pollHotReload();

    mShotCooldown = std::max(0.0f, mShotCooldown - dt);
    mShootingTimer = std::max(0.0f, mShootingTimer - dt);
    mRecoilValue = std::max(0.0f, mRecoilValue - dt * 15.0f);
    mDisturbance = std::max(0.0f, mDisturbance - dt * 8.0f);

    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);

    if (def && rt) {
        if (rt->isReloading) {
            bool wasReloading = rt->isReloading;
            WeaponRuntimeHelper::tickReload(*rt, *def, dt);
            if (wasReloading && !rt->isReloading && !def->soundReload.empty())
                playWorldSound(def->soundReload, player.pos, 0.9f, 1.0f, 10.0f);
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
            WeaponGodball::checkOverlaps(mGodballPhys, *def, *rt, player, npcs,
                                          remoteNpcs, camera, dt);

            Debug::logThrottled(Debug::Category::Weapons, "godball-update", 1.0f,
                "[GODBALL_DBG] UPDATE slot=%d def=%s active=%d pos=(%.2f,%.2f,%.2f) "
                "playerPos=(%.2f,%.2f,%.2f) player=%s npcs=%zu",
                player.equippedSlot, def->id.c_str(), (int)mGodballPhys.active,
                mGodballPhys.position.x, mGodballPhys.position.y, mGodballPhys.position.z,
                player.pos.x, player.pos.y, player.pos.z,
                player.username.c_str(), npcs.all().size());
        } else if (def->behaviorType == WeaponBehaviorType::Swordsword) {
            WeaponSwordsword::update(mSwordswordState, *def, *rt, player, npcs, camera, world, dt);
        } else if (def->behaviorType == WeaponBehaviorType::Hafs) {
            WeaponHafs::update(mHafsState, *def, *rt, player, npcs, camera, world, dt);
        } else if (def->behaviorType == WeaponBehaviorType::QuickHit) {
            WeaponQuickHit::update(mQuickHitState, *def, *rt, player, npcs, camera, world, dt);
        } else if (def->behaviorType == WeaponBehaviorType::SpyKnife) {
            WeaponSpyKnife::update(mSpyKnifeState, *def, *rt, player, npcs, camera, world, dt);
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
        mPhysicalAimValid = false;
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

    // ── World-space aim crosshair (permanent) + physical laser sight ──
    const bool physicalAim =
        GameplayConfig::instance().aimMode() == GameplayAimMode::Physical;
    if (def && rt && (DebugConfig::WORLD_XH_ENABLED || physicalAim)) {
        int idx = slotIndex(def->slot);
        const WeaponViewModel& vm = mViewModels[idx];
        glm::vec3 muzzlePos = vm.muzzle;

        WeaponFire::AimSolution aim = WeaponFire::computeAim(
            camera, world, npcs, muzzlePos, vm.forward, nullptr);
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

        // Physical laser sight: store the barrel's hit point for the HUD and
        // draw an always-on thin red beam from the muzzle to the impact.
        if (physicalAim) {
            mPhysicalAimPoint = crossPos;
            mPhysicalAimValid = true;
            if (CrosshairConfig::instance().data().laserSight) {
                DebugVis::drawFilledBeam(camera, muzzlePos, hitPoint, 0.025f,
                    glm::vec4(1.0f, 0.06f, 0.06f, 0.9f));
            }
        }

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
                static thread_local std::vector<int> candidates;
                candidates.clear();
                appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates, "weaponCrosshairOcclusion");
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

            const GameplayAimMode aimMode = GameplayConfig::instance().aimMode();

            // Farpoint debug: draw camera forward ray + farpoint target + weapon direction
            if (aimMode == GameplayAimMode::Farpoint) {
                float farDist = GameplayConfig::instance().farpointDistance();
                glm::vec3 farTarget = camera.pos + camera.front * farDist;
                // BLUE: camera forward ray (shortened to 100m for visibility)
                glm::vec3 camRayEnd = camera.pos + camera.front * 100.0f;
                DebugVis::drawFilledBeam(camera, camera.pos, camRayEnd, 0.03f, glm::vec4(0.2f, 0.4f, 1.0f, 0.6f));
                // GREEN: farpoint target sphere
                DebugVis::drawFilledSphere(camera, farTarget, 0.15f, glm::vec4(0.2f, 1.0f, 0.2f, 0.8f));
                // RED: weapon direction ray (from muzzle toward farpoint)
                glm::vec3 wpnDir = glm::normalize(farTarget - muzzlePos);
                glm::vec3 wpnRayEnd = muzzlePos + wpnDir * 100.0f;
                DebugVis::drawFilledBeam(camera, muzzlePos, wpnRayEnd, 0.03f, glm::vec4(1.0f, 0.2f, 0.2f, 0.6f));
            }

            // Camforward debug: draw camera forward direction from muzzle (parallel to camera)
            if (aimMode == GameplayAimMode::CamForward) {
                glm::vec3 camDir = glm::normalize(camera.front);
                glm::vec3 muzzleRayEnd = muzzlePos + camDir * 100.0f;
                DebugVis::drawFilledBeam(camera, muzzlePos, muzzleRayEnd, 0.03f, glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
                DebugVis::drawFilledSphere(camera, muzzleRayEnd, 0.1f, glm::vec4(1.0f, 1.0f, 0.0f, 0.5f));
            }
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

    // ── Cool shot line (camforward aiming beam) — independent of world_xh_enabled ──
    {
        bool coolLineActive = DebugConfig::COOL_SHOT_LINE_ENABLED && def && rt;
        if (coolLineActive)
        {
            int idx = slotIndex(def->slot);
            const WeaponViewModel& vm = mViewModels[idx];
            glm::vec3 beamOrigin = vm.muzzle;
            glm::vec3 beamDir = glm::normalize(camera.front);
            float beamLen = std::max(0.1f, DebugConfig::COOL_SHOT_LINE_LENGTH);
            float startAlpha = glm::clamp(DebugConfig::COOL_SHOT_LINE_START_ALPHA, 0.0f, 1.0f);
            float endAlpha = glm::clamp(DebugConfig::COOL_SHOT_LINE_END_ALPHA, 0.0f, 1.0f);
            float startSize = std::max(0.0f, DebugConfig::COOL_SHOT_LINE_START_SIZE);
            float endSize = std::max(0.0f, DebugConfig::COOL_SHOT_LINE_END_SIZE);
            glm::vec4 beamColor(
                DebugConfig::COOL_SHOT_LINE_COLOR_R,
                DebugConfig::COOL_SHOT_LINE_COLOR_G,
                DebugConfig::COOL_SHOT_LINE_COLOR_B,
                1.0f);

            // Raycast along beam to find world obstruction
            float hitDist = beamLen;
            for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
                float d = 0.0f;
                if (WeaponFire::rayTriangle(beamOrigin, beamDir, tri, d) && d < hitDist) {
                    hitDist = d;
                }
            }

            // Draw segmented tapered beam
            constexpr int SEGMENTS = 32;
            float segLen = hitDist / (float)SEGMENTS;
            for (int s = 0; s < SEGMENTS; ++s) {
                float t0 = (float)s / (float)SEGMENTS;
                float t1 = (float)(s + 1) / (float)SEGMENTS;
                float a0 = glm::mix(startAlpha, endAlpha, t0);
                float a1 = glm::mix(startAlpha, endAlpha, t1);
                float size0 = glm::mix(startSize, endSize, t0);
                float size1 = glm::mix(startSize, endSize, t1);
                glm::vec3 segStart = beamOrigin + beamDir * (t0 * hitDist);
                glm::vec3 segEnd = beamOrigin + beamDir * (t1 * hitDist);
                glm::vec3 segMid = (segStart + segEnd) * 0.5f;
                float midAlpha = (a0 + a1) * 0.5f;
                float midSize = (size0 + size1) * 0.5f;
                if (midAlpha > 0.001f && midSize > 0.0001f)
                    DebugVis::drawFilledCylinder(camera, segMid, beamDir, midSize * 0.02f, segLen, glm::vec4(beamColor.r, beamColor.g, beamColor.b, midAlpha));
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
        glm::vec3 dummyPos(0.0f);
        WeaponSwordsword::render(camera, mSwordswordState, *def, dummyPos);
    }

    // QuickHit: render glowing capsule during active ticks
    if (def->behaviorType == WeaponBehaviorType::QuickHit && mQuickHitState.active) {
        Capsule cap = mQuickHitState.currentArmCapsule;
        if (glm::length(cap.b - cap.a) > 0.001f && cap.r > 0.001f) {
            DebugVis::drawWeaponCapsuleWire(camera, cap, {1.0f, 1.0f, 1.0f, 0.9f});
        }
    }

    // ── Helper to build ProjectileVisualConfig from weapon definition ──
    auto buildProjCfg = [&](const WeaponDefinition& wdef, bool isRocket) -> ProjectileVisualConfig {
        auto cp = [&](const char* key, float fallback) {
            return wdef.customParams.count(key) ? wdef.customParams.at(key) : fallback;
        };
        ProjectileVisualConfig cfg;
        cfg.texturePath = isRocket ? "assets/textureshq/colorful2.png" : "assets/textureshq/meat1.png";
        cfg.length = cp("projectileVisualLength", isRocket ? 1.5f : 1.8f);
        cfg.radius = cp("projectileVisualRadius", isRocket ? 0.18f : 0.28f);
        cfg.scale = glm::vec3(cp("projectileVisualScaleX", 1.0f), cp("projectileVisualScaleY", 1.0f), cp("projectileVisualScaleZ", 1.0f));
        cfg.rotationOffsetDegrees = glm::vec3(cp("projectileVisualRotationOffsetX", 0.0f), cp("projectileVisualRotationOffsetY", 0.0f), cp("projectileVisualRotationOffsetZ", 0.0f));
        cfg.textureTiling = glm::vec2(cp("projectileVisualTextureTilingU", 1.0f), cp("projectileVisualTextureTilingV", 1.0f));
        cfg.fillAlpha = cp("projectileFillAlpha", 1.0f);
        cfg.outlineEnabled = cp("projectileOutlineEnabled", 1.0f) > 0.0f;
        cfg.outlineColor = glm::vec3(cp("projectileOutlineColorR", 1.0f), cp("projectileOutlineColorG", 0.8f), cp("projectileOutlineColorB", 0.2f));
        cfg.outlineAlpha = cp("projectileOutlineAlpha", 0.4f);
        cfg.outlineScale = cp("projectileOutlineScale", 1.15f);
        cfg.glowEnabled = cp("projectileGlowEnabled", 1.0f) > 0.0f;
        cfg.glowColor = glm::vec3(cp("projectileGlowColorR", 1.0f), cp("projectileGlowColorG", 0.6f), cp("projectileGlowColorB", 0.0f));
        cfg.glowAlpha = cp("projectileGlowAlpha", 0.15f);
        cfg.glowRadiusMultiplier = cp("projectileGlowRadiusMultiplier", 3.0f);
        return cfg;
    };

    // ── Projectile rendering for rocket launcher ──
    if (def->behaviorType == WeaponBehaviorType::RocketLauncher) {
        ProjectileVisualConfig cfg = buildProjCfg(*def, true);
        for (const auto& rocket : mRocketState.activeRockets) {
            if (rocket.exploded) continue;
            renderProjectile(camera, rocket.position, rocket.orientation, cfg);
        }
    }

    // ── Projectile rendering for grenade launcher ──
    if (def->behaviorType == WeaponBehaviorType::GrenadeLauncher) {
        ProjectileVisualConfig cfg = buildProjCfg(*def, false);
        for (const PersistentPhysicsObject& obj : PersistentPhysicsSystem::instance().objects()) {
            if (obj.exploded || obj.weaponId != "grenade_launcher") continue;
            renderProjectile(camera, obj.position, obj.rotation, cfg);
        }
    }

    // Weapon collision debug visuals (draws actual runtime collision data)
    bool anyVis = gWeaponCollisionVisualsJson || gWeaponCollisionVisualsProbes;
    if (anyVis && player.weaponCollisionDebug.valid) {
        const auto& wcd = player.weaponCollisionDebug;

        // Count spheres by type for logging
        if (DebugConfig::DEBUG_WEAPON_COLLISION)
        {
            static float logTimer = 0.0f;
            logTimer -= 0.016f;
            if (logTimer <= 0.0f) {
                logTimer = 1.0f;
                int jsonCount = 0, probeCount = 0;
                for (const auto& s : wcd.spheres) {
                    if (!s.collidesWithWorld) continue;
                    if (s.sourceType == WeaponColliderDebugSphere::SourceType::JsonSphere)
                        ++jsonCount;
                    else
                        ++probeCount;
                }
                Debug::log(Debug::Category::Weapons,
                    "[WEAPON COLLISION VISUALS] weapon=%s json=%d probes=%d capsule=%d",
                    wcd.weaponId.c_str(), jsonCount, probeCount, (int)wcd.capsule.enabled);
            }
        }

        // Capsule wireframes (only shown with probes)
        if (gWeaponCollisionVisualsProbes && wcd.capsule.enabled) {
            // Current capsule wire (darker blue)
            Capsule curCap;
            curCap.a = wcd.capsule.currentStart;
            curCap.b = wcd.capsule.currentEnd;
            curCap.r = wcd.capsule.radius;
            DebugVis::drawWeaponCapsuleWire(camera, curCap, {0.0f, 0.5f, 1.0f, 0.5f});

            // Previous capsule wire (darker red)
            float prevLen = glm::length(wcd.capsule.previousEnd - wcd.capsule.previousStart);
            if (prevLen > 0.001f) {
                Capsule prevCap;
                prevCap.a = wcd.capsule.previousStart;
                prevCap.b = wcd.capsule.previousEnd;
                prevCap.r = wcd.capsule.radius;
                DebugVis::drawWeaponCapsuleWire(camera, prevCap, {0.5f, 0.0f, 0.0f, 0.4f});
            }
        }

        for (const auto& ds : wcd.spheres) {
            if (!ds.collidesWithWorld)
                continue;

            // Filter by source type: JSON spheres vs probe spheres
            bool isJson = (ds.sourceType == WeaponColliderDebugSphere::SourceType::JsonSphere);
            if (isJson && !gWeaponCollisionVisualsJson) continue;
            if (!isJson && !gWeaponCollisionVisualsProbes) continue;

            // Current sphere: turquoise, 50% transparent
            DebugVis::drawWeaponWireSphere(camera, ds.currentCenter, ds.radius, {0.0f, 1.0f, 1.0f, 0.5f});

            float prevDist = glm::length(ds.previousCenter - ds.currentCenter);
            if (prevDist > 0.001f) {
                // Previous sphere: red, 50% transparent
                DebugVis::drawWeaponWireSphere(camera, ds.previousCenter, ds.radius, {1.0f, 0.0f, 0.0f, 0.5f});
                // Sweep line: yellow
                DebugVis::drawWeaponLine(camera, ds.previousCenter, ds.currentCenter, {1.0f, 1.0f, 0.0f, 0.9f});
            }
        }
    }
}

RevolverShotResult WeaponSystem::fire(
    Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs) {
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
        return fireSwordsword(camera, player, npcs, remotePlayers);
    }

    if (def->behaviorType == WeaponBehaviorType::Hafs) {
        WeaponHafs::startSlash(mHafsState, *def, player);
        return {};
    }

    if (def->behaviorType == WeaponBehaviorType::QuickHit) {
        WeaponQuickHit::startAttack(mQuickHitState, *def, player, camera);
        return {};
    }

    if (def->behaviorType == WeaponBehaviorType::SpyKnife) {
        WeaponSpyKnife::startSwing(mSpyKnifeState, *def, player, camera);
        return {};
    }

    if (def->behaviorType == WeaponBehaviorType::RocketLauncher) {
        return fireRocketLauncher(camera, player, npcs, world, remotePlayers);
    }

    bool canInterruptReload = (def->behaviorType == WeaponBehaviorType::GrenadeLauncher);

    if (!canInterruptReload && (rt->isReloading || rt->fireCooldown > 0.0f)) {
        return {};
    }

    // Allow firing during reload if clip has ammo (interrupts reload for rocket/grenade)
    if (canInterruptReload && rt->isReloading && rt->currentAmmo > 0) {
        // Fire loaded projectile, interrupt reload, discard partial progress
    } else if (rt->fireCooldown > 0.0f) {
        return {};
    }

    if (rt->currentAmmo <= 0) {
        WeaponAudio::playDryFireSound(*def);
        RevolverShotResult dryResult;
        if (!rt->isReloading && rt->reserveAmmo > 0) {
            reload(player);
            dryResult.autoReloadTriggered = true;
        }
        return dryResult;
    }

    if (def->behaviorType == WeaponBehaviorType::GrenadeLauncher) {
        rt->currentAmmo--;
        rt->fireCooldown = def->fireDelay;
        // Interrupt reload on successful fire
        if (rt->isReloading) {
            rt->isReloading = false;
            rt->reloadTimer = 0.0f;
            if (DebugConfig::DEBUG_RELOAD)
                Debug::log(Debug::Category::General, "[RELOAD] interrupted by fire; partial progress discarded\n");
        }
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
            camera, world, npcs, muzzlePos, muzzleDir, nullptr);
        glm::vec3 dir = aim.direction;
        float recoilStrength = def->customParams.count("firingRecoilStrength")
            ? def->customParams.at("firingRecoilStrength") : 20.0f;
        player.externalImpulse -= dir * recoilStrength;
        RevolverShotResult result;
        result.fired = true;
        result.start = muzzlePos;
        result.end = muzzlePos + dir;
        result.direction = dir;
        result.hitNormal = -dir;
        // Always play sound — the actual grenade is created on the server
        // after it accepts the AttackRequest. No local grenade spawn here.
        WeaponAudio::playShootSound(*def, muzzlePos);

        Debug::log(Debug::Category::Weapons, "[GRENADE LAUNCHER] fired ammo=%d/%d pos=(%.2f %.2f %.2f) dir=(%.2f %.2f %.2f)\n",
                   rt->currentAmmo, rt->reserveAmmo,
                   muzzlePos.x, muzzlePos.y, muzzlePos.z,
                   dir.x, dir.y, dir.z);
        return result;
    }

    return fireHitscan(camera, player, npcs, world, remotePlayers, remoteNpcs);
}

void WeaponSystem::tagLatestLocalRocket(uint32_t fireSerial)
{
    WeaponRocketLauncher::tagLatestLocalRocket(mRocketState, fireSerial);
}

bool WeaponSystem::attachAuthoritativeRocket(uint32_t fireSerial, uint32_t projectileId)
{
    return WeaponRocketLauncher::attachAuthoritativeRocket(mRocketState, fireSerial, projectileId);
}

bool WeaponSystem::removeAuthoritativeRocket(uint32_t projectileId)
{
    return WeaponRocketLauncher::removeAuthoritativeRocket(mRocketState, projectileId);
}

bool WeaponSystem::removeLocalRocketByFireSerial(uint32_t fireSerial)
{
    return WeaponRocketLauncher::removeLocalRocketByFireSerial(mRocketState, fireSerial);
}

RevolverShotResult WeaponSystem::fireHitscan(
    const Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs) {
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
            remotePlayers, result, remoteNpcs);
    } else {
        result = WeaponFire::tryFireHitscan(
            *def, *rt, camera, player, npcs, world, muzzlePos, muzzleDir,
            remotePlayers, remoteNpcs);
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

        glm::vec3 hitPoint, hitNormal;
        if (!WeaponGodball::sweptSphereOverlap(
                mGodballPhys.prevPosition, mGodballPhys.position,
                mGodballPhys.radius, target.pos, 0.65f,
                hitPoint, hitNormal))
            continue;

        const int damage = std::clamp(
            (int)std::round(WeaponGodball::computeDamage(
                mGodballPhys, *def, player, target, hitPoint)),
            1, 200);

        RevolverShotResult hit;
        hit.fired = true;
        hit.hitEntity = true;
        hit.targetIsRemotePlayer = true;
        hit.targetId = targetId;
        hit.damage = (float)damage;
        hit.start = WeaponGodball::getHandPosition(player);
        hit.end = hitPoint;
        hit.direction = hitNormal;
        hits.push_back(hit);
        mRemoteGodballCooldowns[targetId] = tickInterval;

        if (!serverAuthHits()) hitmarker(damage);
        {
            HitEvent ev;
            ev.position = hit.end;
            ev.normal = hitNormal;
            ev.direction = hitNormal;
            ev.hitEntity = true;
            ev.damage = damage;
            ev.attacker = player.username;
            ev.victim = target.username;
            ev.weaponSource = "godball_remote";
            if (!serverAuthHits()) HitEffects::onHit(ev);
        }
        WeaponAudio::playGodballImpact(
            hit.end, std::clamp(hit.damage / 100.0f, 0.0f, 1.0f));
    }

    return hits;
}

RevolverShotResult WeaponSystem::fireRocketLauncher(Camera& camera, Player& player, NpcSystem& npcs, const World& world, const std::unordered_map<uint32_t, Player>* remotePlayers) {
    RevolverShotResult result;
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return result;

    if (rt->fireCooldown > 0.0f && !rt->isReloading) {
        playWorldSound("ui/click", player.pos, 0.4f, 0.75f, 10.0f);
        return result;
    }

    // Allow firing during reload if clip has ammo
    if (rt->currentAmmo > 0 && rt->isReloading) {
        // Fire loaded projectile, interrupt reload, discard partial progress
    } else if (rt->currentAmmo <= 0) {
        playWorldSound("ui/click", player.pos, 0.4f, 0.75f, 10.0f);
        if (!rt->isReloading && rt->reserveAmmo > 0) {
            reload(player);
            result.autoReloadTriggered = true;
        }
        return result;
    } else if (rt->fireCooldown > 0.0f) {
        return result;
    }

    // Interrupt reload on successful fire (WeaponRocketLauncher::fire handles ammo consumption)
    if (rt->isReloading) {
        rt->isReloading = false;
        rt->reloadTimer = 0.0f;
        if (DebugConfig::DEBUG_RELOAD)
            Debug::log(Debug::Category::General, "[RELOAD] interrupted by fire; partial progress discarded\n");
    }

    int idx = slotIndex(def->slot);
    const WeaponViewModel& vm = mViewModels[idx];
    glm::vec3 muzzlePos = vm.muzzle;
    glm::vec3 muzzleDir = vm.forward;

    WeaponFire::AimSolution aim = WeaponFire::computeAim(
        camera, world, npcs, muzzlePos, muzzleDir, remotePlayers);
    WeaponFire::logAimDebug("rocket_launcher", camera, aim);
    glm::vec3 dir = aim.direction;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        dir.x, dir.y, dir.z);

    // Firing recoil: push player opposite the rocket direction
    float recoilStrength = def->customParams.count("firingRecoilStrength")
        ? def->customParams.at("firingRecoilStrength") : 30.0f;
    player.externalImpulse -= dir * recoilStrength;

    result.fired = true;
    result.start = muzzlePos;
    result.end = muzzlePos + dir;
    result.direction = dir;
    result.hitNormal = -dir;
    if (remotePlayers)
    {
        rt->currentAmmo--;
        rt->fireCooldown = def->fireDelay;
        if (!def->soundShoot.empty())
            WeaponAudio::playShootSound(*def, muzzlePos);
    }
    else
    {
        WeaponRocketLauncher::fire(mRocketState, *def, *rt, player, muzzlePos, dir);
    }
    rt->shootEffectTimer = weaponParamOr(*def, "shootPoseTime", 0.12f);
    mShotCooldown = def->fireDelay;
    AnalyticsManager::instance().trackWeaponUsed(def->id);
    return result;
}

void WeaponSystem::fireGodball(Camera& camera, Player& player, NpcSystem& npcs, const World& world) {
    // Overlap damage is handled continuously in update().
    // Fire input is a no-op for godball (always "automatic").
}

RevolverShotResult WeaponSystem::fireSwordsword(
    Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers) {
    RevolverShotResult result;
    const WeaponDefinition* def = getCurrentDef(player);
    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!def || !rt) return result;

    if (rt->isReloading || rt->fireCooldown > 0.0f) return result;

    if (mSwordswordState.state != SwordswordState::AttackState::Idle &&
        mSwordswordState.state != SwordswordState::AttackState::SlashRecover) return result;

    float slashCooldown = weaponParamOr(*def, "slashCooldown", 0.25f);
    rt->fireCooldown = slashCooldown;
    mShotCooldown = slashCooldown;

    WeaponSwordsword::startSlash(mSwordswordState, *def, player);
    AnalyticsManager::instance().trackWeaponUsed(def->id);
    result.fired = true;
    result.start = player.pos;
    result.end = player.pos + player.aimDirection * weaponParamOr(*def, "bladeLength", 4.0f);
    result.hitNormal = -player.aimDirection;

    // In online mode, result.fired triggers mpSendMeleeHitRequest from the
    // caller (weapon-commands.cpp). The client no longer selects a target.
    // Attack type 1 = slash.
    if (remotePlayers) {
        result.targetIsRemotePlayer = false;
    }
    return result;
}

RevolverShotResult WeaponSystem::fireAlt(
    Camera& camera,
    Player& player,
    NpcSystem& npcs,
    const World& world) {
    if (player.dead) return {};

    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return {};

    WeaponRuntime* rt = getCurrentRuntime(player);
    if (!rt) return {};

    if (def->behaviorType == WeaponBehaviorType::Hafs) {
        WeaponHafs::startLunge(mHafsState, *def, player);
        RevolverShotResult res;
        res.fired = true;
        return res;
    }

    if (def->behaviorType != WeaponBehaviorType::Swordsword) return {};

    if (rt->isReloading || rt->fireCooldown > 0.0f) return {};

    if (mSwordswordState.state != SwordswordState::AttackState::Idle &&
        mSwordswordState.state != SwordswordState::AttackState::LungeRecover) return {};

    float lungeCooldown = weaponParamOr(*def, "lungeCooldown", 0.5f);
    rt->fireCooldown = lungeCooldown;
    mShotCooldown = lungeCooldown;

    WeaponSwordsword::startLunge(mSwordswordState, *def, player);
    AnalyticsManager::instance().trackWeaponUsed(def->id);

    RevolverShotResult res;
    res.fired = true;
    // In online mode, the caller (engine-tick-combat.cpp) will detect res.fired
    // and send a melee attack request. We mark the serial as pending.
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

uint8_t WeaponSystem::networkVisualState(const Player& player) const {
    uint8_t state = 0;
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return state;
    auto it = player.weaponRuntimes.find(def->id);
    if (it == player.weaponRuntimes.end())
        return state;
    const WeaponRuntime& rt = it->second;
    if (rt.shootEffectTimer > 0.0f)
        state |= MimitaNet::NET_WEAPON_STATE_FIRING;
    if (rt.isReloading)
        state |= MimitaNet::NET_WEAPON_STATE_RELOADING;
    if (rt.fireCooldown > 0.0f)
        state |= MimitaNet::NET_WEAPON_STATE_COOLDOWN;
    if (rt.currentAmmo <= 0)
        state |= MimitaNet::NET_WEAPON_STATE_EMPTY;
    return state;
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

void WeaponSystem::renderRemoteWeapon(uint32_t entityId, const Player& player, const Camera& camera, float dt) {
    if (player.dead) return;
    const WeaponDefinition* def = getCurrentDef(player);
    if (!def) return;
    const std::string key = std::to_string(entityId) + ":" + def->id;
    WeaponViewModel& vm = mRemoteViewModels[key];
    vm.update(camera, const_cast<Player&>(player), dt, def, true);
    if (DebugConfig::DEBUG_WEAPON_VIEWMODEL) {
        printf("[WEAPON VISUAL INSTANCE] entityId=%u weaponId=%s resourcePtr=%p "
               "instancePtr=%p runtimePtr=%p isReloading=%d fireCooldown=%.2f "
               "tint=(%.2f,%.2f,%.2f) reloadBlend=%.2f\n",
               entityId, def->id.c_str(), (const void*)vm.loadedModelPath.c_str(),
               (void*)&vm,
               player.weaponRuntimes.count(def->id)
                   ? (const void*)&player.weaponRuntimes.at(def->id)
                   : nullptr,
               (int)((player.networkWeaponState & MimitaNet::NET_WEAPON_STATE_RELOADING) != 0),
               (player.networkWeaponState & MimitaNet::NET_WEAPON_STATE_COOLDOWN) ? 1.0f : 0.0f,
               vm.mTint.r, vm.mTint.g, vm.mTint.b, vm.mReloadBlendCurrent);
    }
    vm.render(camera, player, def->slot);

    // Render godball sphere for remote players if godball is active
    if (player.godballActive)
    {
        float radius = 0.5f;
        const WeaponDefinition* gbDef = WeaponRegistry::instance().get("godball");
        if (gbDef)
        {
            auto it = gbDef->customParams.find("radius");
            if (it != gbDef->customParams.end())
                radius = it->second;
        }
        DebugVis::drawFilledSphere(camera, player.godballPosition, radius,
                                    {0.2f, 0.4f, 0.8f, 0.6f});
        DebugVis::drawWireSphere(camera, player.godballPosition, radius,
                                 {0.4f, 0.6f, 1.0f, 0.8f});

        // Rope: 6 beam segments from hand to ball
        const glm::vec3 handPos = WeaponGodball::getHandPosition(player);
        const int ropeSegments = 6;
        for (int i = 0; i < ropeSegments; ++i)
        {
            const float t0 = (float)i / (float)ropeSegments;
            const float t1 = (float)(i + 1) / (float)ropeSegments;
            const glm::vec3 a = handPos + (player.godballPosition - handPos) * t0;
            const glm::vec3 b = handPos + (player.godballPosition - handPos) * t1;
            DebugVis::drawFilledBeam(camera, a, b, 0.03f,
                                     {0.3f, 0.5f, 0.9f, 0.7f});
        }
    }

    // QuickHit: render glowing capsule for remote players during active ticks
    if (def->behaviorType == WeaponBehaviorType::QuickHit) {
        auto rtIt = player.weaponRuntimes.find(def->id);
        if (rtIt != player.weaponRuntimes.end() && rtIt->second.shootEffectTimer > 0.0f) {
            // Compute approximate capsule from remote player position + aim direction
            glm::vec3 forward = player.aimDirection;
            if (glm::length(forward) < 0.001f) forward = glm::vec3(0.0f, 1.0f, 0.0f);
            forward.z = 0.0f;
            if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
            else forward = glm::vec3(0.0f, 1.0f, 0.0f);

            float capsuleRadius = 0.22f;
            float capsuleLength = 0.85f;
            auto rIt = def->customParams.find("hitboxRadius");
            if (rIt != def->customParams.end()) capsuleRadius = rIt->second;
            auto lIt = def->customParams.find("hitboxLength");
            if (lIt != def->customParams.end()) capsuleLength = lIt->second;

            glm::vec3 shoulderOffset(0.0f, 0.0f, 1.2f);
            glm::vec3 armCenter = player.pos + shoulderOffset + forward * 0.6f;
            glm::vec3 armTip = armCenter + forward * capsuleLength;

            Capsule cap;
            cap.a = armCenter;
            cap.b = armTip;
            cap.r = capsuleRadius;
            DebugVis::drawWeaponCapsuleWire(camera, cap, {1.0f, 1.0f, 1.0f, 0.9f});
        }
    }
}
