#include "weapon-swordsword.h"
#include "weapon-types.h"
#include "weapon-audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/death-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "perf/perf-spike.h"
#include "physics/movement/physics-collision.h"
#include "physics/config.h"
#include "physics/movement/physics-collision-shared.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"


extern TextureStore gTextures;

namespace WeaponSwordsword {

static float cp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

void initResources(SwordswordState&) {}

// ── Swept sphere vs sphere (from Godball) ────────────────────

static bool sweptSphereOverlap(
    glm::vec3 prevCenter, glm::vec3 currCenter, float radius,
    glm::vec3 targetCenter, float targetRadius,
    float& outDist, glm::vec3& outClosest)
{
    glm::vec3 seg = currCenter - prevCenter;
    float segLen = glm::length(seg);
    if (segLen < 0.0001f) {
        float d = glm::length(currCenter - targetCenter);
        if (d < radius + targetRadius) {
            outDist = d;
            outClosest = currCenter;
            return true;
        }
        return false;
    }
    glm::vec3 segDir = seg / segLen;
    glm::vec3 toTarget = targetCenter - prevCenter;
    float t = glm::clamp(glm::dot(toTarget, segDir), 0.0f, segLen);
    glm::vec3 closest = prevCenter + segDir * t;
    float d = glm::length(closest - targetCenter);
    if (d < radius + targetRadius) {
        outDist = d;
        outClosest = closest;
        return true;
    }
    return false;
}

// ── Godball-style force damage ──────────────────────────────

static float computeContactDamage(
    const WeaponDefinition& def,
    float weaponSpeed,
    const glm::vec3& weaponDir,
    const glm::vec3& toTarget,
    const glm::vec3& contactNormal,
    bool isLunge, bool isSlash,
    float& outKnockback)
{
    float baseDamage = cp(def, isLunge ? "lungeBaseDamage" : "slashBaseDamage", isLunge ? 18.0f : 10.0f);
    float speedFactor = cp(def, isLunge ? "lungeSpeedDamageFactor" : "slashSpeedDamageFactor", isLunge ? 28.0f : 18.0f);
    float angleFactor = cp(def, isLunge ? "lungeAngleDamageFactor" : "slashAngleDamageFactor", 12.0f);
    float maxCap = cp(def, isLunge ? "lungeMaxDamage" : "slashMaxDamage", 999999.0f);
    float baseKb = cp(def, isLunge ? "lungeBaseKnockback" : "slashBaseKnockback", isLunge ? 50.0f : 25.0f);
    float speedKbFactor = cp(def, isLunge ? "lungeSpeedKnockbackFactor" : "slashSpeedKnockbackFactor", isLunge ? 6.0f : 3.0f);
    float maxKb = cp(def, isLunge ? "lungeMaxKnockback" : "slashMaxKnockback", isLunge ? 120.0f : 60.0f);

    float speedContrib = weaponSpeed * speedFactor * 0.01f;
    float totalDmg = baseDamage + speedContrib;

    float bLen = glm::length(weaponDir);
    float tLen = glm::length(toTarget);
    if (bLen > 0.001f && tLen > 0.001f && weaponSpeed > 0.001f) {
        float dotVal = glm::dot(weaponDir / bLen, toTarget / tLen);
        totalDmg += std::max(0.0f, dotVal) * angleFactor;
    }

    float forceMul = 1.0f;
    if (isSlash)      forceMul = cp(def, "slashForceMultiplier", 0.5f);
    else if (isLunge) forceMul = cp(def, "lungeForceMultiplier", 1.1f);
    else              forceMul = cp(def, "dashForceMultiplier", 1.0f);
    totalDmg *= forceMul;

    float kb = baseKb + weaponSpeed * speedKbFactor;
    if (bLen > 0.001f && tLen > 0.001f && weaponSpeed > 0.001f) {
        float dotVal = std::max(0.0f, glm::dot(weaponDir / bLen, toTarget / tLen));
        kb += dotVal * angleFactor * 1.5f;
    }
    kb *= forceMul;

    float globalDmgMul = cp(def, "globalDamageMultiplier", 1.0f);
    float globalKbMul = cp(def, "globalKnockbackMultiplier", 1.0f);

    totalDmg *= globalDmgMul;
    kb *= globalKbMul;

    totalDmg = std::clamp(totalDmg, 0.0f, maxCap * globalDmgMul);
    outKnockback = std::clamp(kb, 0.0f, maxKb * globalKbMul);

    return totalDmg;
}

// ── Weapon collision spheres overlap vs NPC body parts ─────

static void checkCapsuleHits(SwordswordState& state, const WeaponDefinition& def,
                               Player& owner, NpcSystem& npcs) {
    MIMITA_PERF_SCOPE("Swordsword::Collision");
    float tickInterval = cp(def, "damageTickInterval", 0.05f);

    bool isSlash = (state.state == SwordswordState::AttackState::SlashActive);
    bool isLunge = (state.state == SwordswordState::AttackState::LungeActive);

    // Use weapon collision spheres from JSON config (large, positioned along blade)
    const auto& spheres = owner.weaponCollisionDebug.spheres;
    if (spheres.empty()) return;

    float playerSpeed = glm::length(owner.vel);

    for (Npc& npc : npcs.all()) {
        if (npc.body.dead || npc.body.currentHp <= 0) continue;

        auto& cd = state.hitCooldowns[npc.id];
        if (cd > 0.0f) continue;

        for (const auto& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 worldCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 halfSize = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float partRadius = glm::length(halfSize) * 1.25f;

            // Test each weapon collision sphere against body part
            bool hit = false;
            glm::vec3 hitPoint, contactNormal;
            for (const auto& ws : spheres) {
                float d = 0.0f;
                glm::vec3 closest;
                if (sweptSphereOverlap(ws.previousCenter, ws.currentCenter, ws.radius,
                                       worldCenter, partRadius, d, closest)) {
                    hit = true;
                    hitPoint = closest;
                    contactNormal = glm::normalize(closest - worldCenter);
                    if (glm::length(contactNormal) < 0.001f)
                        contactNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                    break;
                }
            }

            if (!hit) continue;

            // Blade direction from first to last sphere
            glm::vec3 bladeDir = spheres.back().currentCenter - spheres.front().currentCenter;
            glm::vec3 grip = spheres.front().currentCenter;

            // Effective speed = sword animation velocity + player velocity
            float animSpeed = state.swordSpeed;
            float totalSpeed = animSpeed + playerSpeed * 0.5f;

            glm::vec3 toTarget = npc.body.pos - grip;

            float knockback = 0.0f;
            float damage = computeContactDamage(def, totalSpeed, bladeDir, toTarget,
                                                 contactNormal, isLunge, isSlash, knockback);
            if (damage <= 0.0f) continue;

            glm::vec3 kbDir = glm::normalize(npc.body.pos - grip);
            kbDir.z = std::max(kbDir.z, 0.15f);
            kbDir = glm::normalize(kbDir);

            // Apply per-axis knockback multipliers
            float kbH = cp(def, "knockbackHorizontalMultiplier", 1.0f);
            float kbV = cp(def, "knockbackVerticalMultiplier", 1.0f);
            glm::vec3 kbVec(kbDir.x * knockback * kbH,
                            kbDir.y * knockback * kbH,
                            kbDir.z * knockback * kbV);
            npc.body.takeDamage((int)damage, glm::normalize(kbVec), glm::length(kbVec));
            cd = tickInterval;

            float vol = std::min(0.5f + (knockback / 80.0f) * 0.5f, 1.0f);
            playWorldSound("weapon/hafs/hafsknockback", npc.body.pos, vol, 1.0f, 30.0f);

            float kbSoundMin = cp(def, "knockbackSoundMinForce", 40.0f);
            if (knockback > kbSoundMin)
                playWorldSound("weapon/hafs/hafsknockback", npc.body.pos, 0.8f, 1.0f, 30.0f);

            if (DebugConfig::DEBUG_SWORDSWORD) {
                printf("[SWORDSWORD HIT] target=%u part=%s speed=%.1f total=%.1f dmg=%.1f kb=%.1f\n",
                       npc.id, part.name.c_str(), animSpeed, totalSpeed, damage, knockback);
                SwordswordState::DebugHit dh;
                dh.point = hitPoint;
                dh.normal = contactNormal;
                dh.damage = damage;
                dh.knockback = knockback;
                state.debugHits.push_back(dh);
            }

            if (npc.body.currentHp <= 0) {
                DeathSystem::instance().kill(npc.body, "npc_" + std::to_string(npc.id), "npc",
                                              owner.username, kbDir, knockback);
            }
            break;
        }
    }
}

// ── World collision self-knockback ──────────────────────────

static void applyWorldHitKnockback(SwordswordState& state, const WeaponDefinition& def,
                                     Player& owner, const glm::vec3& contactPos,
                                     const glm::vec3& contactNormal, float hitSpeed) {
    if (state.worldHitCooldown > 0.0f) return;

    float cooldown = cp(def, "worldHitSoundCooldown", 0.3f);
    float kbStrength = hitSpeed * cp(def, "selfKnockbackMultiplier", 0.8f);
    float kbH = cp(def, "knockbackHorizontalMultiplier", 1.0f);
    float kbV = cp(def, "knockbackVerticalMultiplier", 0.1f);

    // Knockback away from contact point, scaled by speed
    glm::vec3 dir = glm::normalize(owner.pos - contactPos + contactNormal);
    dir.z = std::max(dir.z, 0.1f);
    dir = glm::normalize(dir);

    glm::vec3 kbVec(dir.x * kbStrength * kbH,
                    dir.y * kbStrength * kbH,
                    dir.z * kbStrength * kbV);
    owner.vel += kbVec;

    playWorldSound("weapon/hafs/hafsWorldHit", contactPos, 0.6f, 1.0f, 20.0f);
    state.worldHitCooldown = cooldown;

    if (DebugConfig::DEBUG_SWORDSWORD)
        printf("[SWORDSWORD WORLD] speed=%.1f kb=(%.1f %.1f %.1f)\n",
               hitSpeed, kbVec.x, kbVec.y, kbVec.z);
}

// ── Attack sphere helpers ──────────────────────────────────

static void spawnWalkSphere(SwordswordState& state, const WeaponDefinition& def,
                              const glm::vec2& wishDir, const Player& owner) {
    SwordswordState::AttackSphere s;
    s.position = owner.pos + glm::vec3(wishDir.x, wishDir.y, 0.0f) * 1.5f;
    s.velocity = glm::vec3(wishDir.x * 8.0f, wishDir.y * 8.0f, 0.0f);
    s.radius = 1.5f;
    s.lifetime = 15;
    s.minDamage = 10.0f;
    s.knockbackStrength = 20.0f;
    s.kbDir = glm::vec3(wishDir.x, wishDir.y, 0.3f);
    state.spheres.push_back(std::move(s));
}

static void spawnJumpSphere(SwordswordState& state, const WeaponDefinition& def,
                              const Player& owner, float minDmg, float radius) {
    SwordswordState::AttackSphere s;
    s.position = owner.pos + glm::vec3(0.0f, 0.0f, 2.0f);
    s.velocity = glm::vec3(0.0f, 0.0f, 10.0f);
    s.radius = radius;
    s.lifetime = 15;
    s.minDamage = minDmg;
    s.knockbackStrength = 30.0f;
    s.kbDir = glm::vec3(0.0f, 0.0f, 1.0f);
    state.spheres.push_back(std::move(s));
}

static void spawnQSphere(SwordswordState& state, const WeaponDefinition& def,
                           const Player& owner, bool airborne) {
    SwordswordState::AttackSphere s;
    if (airborne) {
        // Big downward smack
        s.position = owner.pos + glm::vec3(0.0f, 0.0f, -3.0f);
        s.velocity = glm::vec3(0.0f, 0.0f, -15.0f);
        s.radius = 4.0f;
        s.lifetime = 15;
        s.minDamage = 30.0f;
        s.knockbackStrength = 60.0f;
        s.kbDir = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        // Low flat sphere, upward knockback
        s.position = owner.pos + glm::vec3(0.0f, 0.0f, 0.2f);
        s.velocity = glm::vec3(0.0f);
        s.radius = 3.0f;
        s.lifetime = 10;
        s.minDamage = 15.0f;
        s.knockbackStrength = 50.0f;
        s.kbDir = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    state.spheres.push_back(std::move(s));
}

static void spawnFreezeSphere(SwordswordState& state, const WeaponDefinition& def,
                                const Player& owner) {
    SwordswordState::AttackSphere s;
    s.position = owner.pos;
    s.velocity = glm::vec3(0.0f);
    s.radius = 5.0f;
    s.lifetime = 2;
    s.minDamage = 40.0f;
    s.knockbackStrength = 70.0f;
    s.kbDir = glm::vec3(0.0f, 0.0f, 0.5f);
    state.spheres.push_back(std::move(s));
}

static void updateSpheres(SwordswordState& state, const WeaponDefinition& def,
                           Player& owner, NpcSystem& npcs) {
    float tickInterval = cp(def, "damageTickInterval", 0.05f);
    auto& cdMap = state.sphereHitCooldowns;

    for (auto it = state.spheres.begin(); it != state.spheres.end(); ) {
        it->position += it->velocity * (1.0f / 60.0f);
        it->velocity *= 0.85f;
        it->lifetime--;

        // Damage vs NPCs
        for (Npc& npc : npcs.all()) {
            if (npc.body.dead) continue;
            auto& cd = cdMap[npc.id];
            if (cd > 0.0f) continue;

            glm::vec3 toNpc = npc.body.pos - it->position;
            float dist = glm::length(toNpc);
            if (dist < it->radius) {
                float speed = glm::length(it->velocity);
                float damage = it->minDamage + speed * 2.0f;
                float kb = it->knockbackStrength + speed * 1.5f;
                glm::vec3 kbDir = glm::normalize(it->kbDir + glm::vec3(0, 0, 0.1f));
                kbDir = glm::normalize(kbDir);

                npc.body.takeDamage((int)damage, kbDir, kb);
                cd = tickInterval;

                if (npc.body.currentHp <= 0)
                    DeathSystem::instance().kill(npc.body, "npc_" + std::to_string(npc.id), "npc",
                                                  owner.username, kbDir, kb);

                if (DebugConfig::DEBUG_SWORDSWORD)
                    printf("[SWORD SPHERE] hit npc=%u dmg=%.1f kb=%.1f\n",
                           npc.id, damage, kb);
                break;
            }
        }

        if (it->lifetime <= 0)
            it = state.spheres.erase(it);
        else
            ++it;
    }

    // Decay sphere cooldowns
    for (auto it = cdMap.begin(); it != cdMap.end(); ) {
        it->second -= (1.0f / 60.0f);
        if (it->second <= 0.0f) it = cdMap.erase(it); else ++it;
    }
}

// ── Check weapon spheres vs world for self-knockback ────────

static void checkWorldCollision(SwordswordState& state, const WeaponDefinition& def,
                                  Player& owner, const World& world) {
    const auto& spheres = owner.weaponCollisionDebug.spheres;
    if (spheres.empty() || state.swordSpeed < cp(def, "worldHitSoundMinSpeed", 5.0f))
        return;

    // Check each weapon sphere against world blocks (quick OBB test)
    for (const auto& ws : spheres) {
        glm::vec3 center = ws.currentCenter;
        float r = ws.radius;
        for (const Block& block : world.blocks) {
            glm::vec3 half = block.size * 0.5f;
            if (center.x + r < block.pos.x - half.x || center.x - r > block.pos.x + half.x) continue;
            if (center.y + r < block.pos.y - half.y || center.y - r > block.pos.y + half.y) continue;
            if (center.z + r < block.pos.z - half.z || center.z - r > block.pos.z + half.z) continue;

            // Contact! Compute direction away from block center
            glm::vec3 contactNormal = glm::normalize(center - block.pos);
            if (glm::length(contactNormal) < 0.001f)
                contactNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 contactPos = block.pos + contactNormal * (glm::length(half) + r);
            applyWorldHitKnockback(state, def, owner, contactPos, contactNormal, state.swordSpeed);
            return;
        }
    }

    // Check collision mesh triangles (tight AABB around each sphere)
    for (const auto& ws : spheres) {
        AABB bounds;
        bounds.min = ws.currentCenter - glm::vec3(ws.radius);
        bounds.max = ws.currentCenter + glm::vec3(ws.radius);
        static std::vector<int> candidates;
        candidates.clear();
        appendChunkTrianglesForAABB(const_cast<World&>(world), bounds, 0.1f, candidates, "swordswordWorldCheck");
        for (int triIdx : candidates) {
            if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size()) continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIdx];
            glm::vec3 closest = tri.a;
            float minD = glm::length(ws.currentCenter - tri.a);
            float d;
            d = glm::length(ws.currentCenter - tri.b); if (d < minD) { minD = d; closest = tri.b; }
            d = glm::length(ws.currentCenter - tri.c); if (d < minD) { minD = d; closest = tri.c; }
            if (minD < ws.radius) {
                glm::vec3 contactNormal = glm::normalize(ws.currentCenter - closest);
                if (glm::length(contactNormal) < 0.001f)
                    contactNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                applyWorldHitKnockback(state, def, owner, closest, contactNormal, state.swordSpeed);
                return;
            }
        }
    }
}

// ── Attack start ─────────────────────────────────────────────

void startSlash(SwordswordState& state, const WeaponDefinition& def, Player& owner) {
    state.state = SwordswordState::AttackState::SlashWindup;
    state.stateTimer = 0.0f;
    state.animTimer = 0.0f;
    state.hitCooldowns.clear();
    state.debugHits.clear();

    // Half dash power forward, horizontal only
    glm::vec2 dir2D = glm::normalize(glm::vec2(owner.aimDirection.x, owner.aimDirection.y));
    float slashImpulse = DASH_IMPULSE * 0.5f;
    owner.vel.x += dir2D.x * slashImpulse;
    owner.vel.y += dir2D.y * slashImpulse;

    playWorldSound("weapon/hafs/hafsswing", owner.pos, 0.8f, 1.0f, 30.0f);
    Debug::log(Debug::Category::Weapons, "[SWORDSWORD] slash start impulse=%.1f dir=(%.2f %.2f)\n",
               slashImpulse, dir2D.x, dir2D.y);
}

void startLunge(SwordswordState& state, const WeaponDefinition& def, Player& owner) {
    state.state = SwordswordState::AttackState::LungeWindup;
    state.stateTimer = 0.0f;
    state.animTimer = 0.0f;
    state.hitCooldowns.clear();
    state.debugHits.clear();

    // Full dash power forward, horizontal only
    glm::vec2 dir2D = glm::normalize(glm::vec2(owner.aimDirection.x, owner.aimDirection.y));
    float lungeImpulse = DASH_IMPULSE * 1.0f;
    owner.vel.x += dir2D.x * lungeImpulse;
    owner.vel.y += dir2D.y * lungeImpulse;

    playWorldSound("weapon/hafs/hafslunge", owner.pos, 0.8f, 1.0f, 30.0f);
    Debug::log(Debug::Category::Weapons, "[SWORDSWORD] lunge start impulse=%.1f dir=(%.2f %.2f)\n",
               lungeImpulse, dir2D.x, dir2D.y);
}

// ── Per-frame update ─────────────────────────────────────────

void update(SwordswordState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            NpcSystem& npcs, const Camera& camera, const World& world, float dt) {
    MIMITA_PERF_SCOPE("Swordsword::Update");
    (void)camera;

    dt = std::min(dt, 0.05f);

    // Reset pose state each frame; active states override below
    runtime.customFloats["swordPoseState"] = 0.0f;

    if (state.worldHitCooldown > 0.0f)
        state.worldHitCooldown -= dt;

    // Save previous capsule positions
    state.prevWeaponGrip = owner.weaponCollisionCapsule.a;
    state.prevWeaponTip = owner.weaponCollisionCapsule.b;

    // Decay cooldowns
    for (auto it = state.hitCooldowns.begin(); it != state.hitCooldowns.end(); ) {
        it->second -= dt;
        if (it->second <= 0.0f)
            it = state.hitCooldowns.erase(it);
        else
            ++it;
    }

    // ── Sphere attack input handling ───────────────────────
    // WASD: spawn directional sphere in movement direction
    if (glm::length(owner.inputWishMove) > 0.1f) {
        glm::vec2 moveDir = glm::normalize(owner.inputWishMove);
        spawnWalkSphere(state, def, moveDir, owner);
    }

    // Jump: upward sphere
    // Detect jump via didGroundJump or didAirJump flags
    if (owner.jump.didAirJump)
        spawnJumpSphere(state, def, owner, 30.0f, 3.0f);
    else if (owner.jump.didGroundJump)
        spawnJumpSphere(state, def, owner, 20.0f, 2.0f);

    // Dash (Q): detect via didDash flag
    if (owner.dash.didDash) {
        bool airborne = !owner.ground.onGround;
        spawnQSphere(state, def, owner, airborne);
    }

    // Freeze (E): big sphere while held
    if (owner.freeze.freezeActive) {
        if (!state.freezeHeld)
            spawnFreezeSphere(state, def, owner);
        state.freezeHeld = true;
    } else {
        state.freezeHeld = false;
    }

    // Update active spheres
    updateSpheres(state, def, owner, npcs);

    float slashWindup  = cp(def, "slashWindupTime", 0.08f);
    float slashActive  = cp(def, "slashActiveTime", 0.15f);
    float slashRecover = cp(def, "slashRecoverTime", 0.10f);
    float lungeWindup  = cp(def, "lungeWindupTime", 0.10f);
    float lungeActive  = cp(def, "lungeActiveTime", 0.20f);
    float lungeRecover = cp(def, "lungeRecoverTime", 0.12f);

    if (state.state == SwordswordState::AttackState::Idle) {
        state.swordSpeed = 0.0f;

    } else if (state.state == SwordswordState::AttackState::SlashWindup) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / slashWindup;
        if (state.stateTimer >= slashWindup) {
            state.state = SwordswordState::AttackState::SlashActive;
            state.stateTimer = 0.0f;
        }

    } else if (state.state == SwordswordState::AttackState::SlashActive) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / slashActive;

        // Weapon speed from capsule movement
        glm::vec3 curTip = owner.weaponCollisionCapsule.b;
        state.swordVelocity = (curTip - state.prevWeaponTip) / std::max(dt, 0.0001f);
        state.swordSpeed = glm::length(state.swordVelocity);

        // Slash pose
            runtime.customFloats["swordPoseState"] = 1.0f;
        runtime.shootEffectTimer = std::max(runtime.shootEffectTimer, 0.05f);

        recomputeWeaponCapsule(owner);
        checkCapsuleHits(state, def, owner, npcs);

        if (state.worldHitCooldown <= 0.0f)
            checkWorldCollision(state, def, owner, world);

        if (state.stateTimer >= slashActive) {
            state.state = SwordswordState::AttackState::SlashRecover;
            state.stateTimer = 0.0f;
        }

    } else if (state.state == SwordswordState::AttackState::SlashRecover) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / slashRecover;
        if (state.stateTimer >= slashRecover) {
            state.state = SwordswordState::AttackState::Idle;
            state.hitCooldowns.clear();
        }

    } else if (state.state == SwordswordState::AttackState::LungeWindup) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / lungeWindup;
        if (state.stateTimer >= lungeWindup) {
            state.state = SwordswordState::AttackState::LungeActive;
            state.stateTimer = 0.0f;
        }

    } else if (state.state == SwordswordState::AttackState::LungeActive) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / lungeActive;

        glm::vec3 curTip = owner.weaponCollisionCapsule.b;
        state.swordVelocity = (curTip - state.prevWeaponTip) / std::max(dt, 0.0001f);
        state.swordSpeed = glm::length(state.swordVelocity);

        // Lunge pose
        runtime.customFloats["swordPoseState"] = 2.0f;
        runtime.shootEffectTimer = std::max(runtime.shootEffectTimer, 0.05f);

        // Force spike at peak of lunge: multiply sword speed for damage calc
        float spikeMul = cp(def, "lungeForceSpikeMultiplier", 5.0f);
        float spikeCenter = cp(def, "lungeForceSpikeCenter", 0.5f);
        float spikeWidth = cp(def, "lungeForceSpikeWidth", 0.15f);
        float distFromCenter = std::fabs(state.animTimer - spikeCenter);
        float spikeFactor = 1.0f;
        if (distFromCenter < spikeWidth) {
            float t = 1.0f - (distFromCenter / spikeWidth);
            spikeFactor = 1.0f + (spikeMul - 1.0f) * t;
        }
        state.swordSpeed *= spikeFactor;

        recomputeWeaponCapsule(owner);
        checkCapsuleHits(state, def, owner, npcs);

        if (state.worldHitCooldown <= 0.0f)
            checkWorldCollision(state, def, owner, world);

        float lungeDrag = cp(def, "lungeDrag", 3.0f);
        if (lungeDrag > 0.0f)
            owner.vel *= (1.0f - lungeDrag * dt);

        if (state.stateTimer >= lungeActive) {
            state.state = SwordswordState::AttackState::LungeRecover;
            state.stateTimer = 0.0f;
        }

    } else if (state.state == SwordswordState::AttackState::LungeRecover) {
        state.stateTimer += dt;
        state.animTimer = state.stateTimer / lungeRecover;
        if (state.stateTimer >= lungeRecover) {
            state.state = SwordswordState::AttackState::Idle;
            state.hitCooldowns.clear();
        }
    }

    if (DebugConfig::DEBUG_SWORDSWORD) {
        static float perfTimer = 0.0f;
        perfTimer += dt;
        if (perfTimer >= 1.0f) {
            perfTimer = 0.0f;
            printf("[SWORDSWORD PERF] state=%d speed=%.1f cooldowns=%zu\n",
                   (int)state.state, state.swordSpeed, state.hitCooldowns.size());
        }
    }
}

// ── Render (no-op — viewmodel renders the GLB) ───────────────

void render(const Camera& camera, const SwordswordState& state, const WeaponDefinition& def, const glm::vec3& handPos) {
    MIMITA_PERF_SCOPE("Swordsword::Render");
    (void)camera;
    (void)state;
    (void)def;
    (void)handPos;
    // The viewmodel system renders the HAFS GLB model.
    // This function can be used for debug overlays.
}

} // namespace WeaponSwordsword
