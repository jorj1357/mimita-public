#include "weapon-swordsword.h"
#include "weapon-types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/death-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"

namespace WeaponSwordsword {

glm::vec3 getHandPosition(const Player& player) {
    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name == "rightArm") {
            glm::vec3 center = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 boundsSize = part.collider.localMax - part.collider.localMin;
            int axis = boundsSize.y > boundsSize.x ? 1 : 0;
            if (boundsSize.z > boundsSize[axis]) axis = 2;
            float minDist = std::fabs(part.collider.localMin[axis]);
            float maxDist = std::fabs(part.collider.localMax[axis]);
            center[axis] = maxDist >= minDist ? part.collider.localMax[axis] : part.collider.localMin[axis];
            return glm::vec3(part.worldTransform * glm::vec4(center, 1.0f));
        }
    }
    return player.pos + glm::vec3(0.0f, 0.5f, 1.0f);
}

static void applyHit(Player& owner, Player& target, const glm::vec3& hitPoint,
                      const glm::vec3& hitNormal, float damage, float knockbackForce) {
    if (target.dead || target.currentHp <= 0) return;

    glm::vec3 kbDir = glm::normalize(target.pos - owner.pos);
    kbDir.z = std::max(kbDir.z, 0.15f);

    target.takeDamage((int)damage, kbDir, knockbackForce);

    hitmarker();

    {
        HitEvent ev;
        ev.position = hitPoint;
        ev.normal = -hitNormal;
        ev.direction = hitNormal;
        ev.hitEntity = true;
        ev.damage = (int)damage;
        ev.attacker = owner.username;
        ev.victim = target.username;
        ev.weaponSource = "swordsword";
        HitEffects::onHit(ev);
    }

    int hitIdx = rand() % 4;
    std::string hitName = "swordswordhit" + std::to_string(hitIdx + 1);
    float vol = std::min(0.4f + (knockbackForce / 60.0f) * 0.6f, 1.0f);
    float pitch = std::max(1.0f - (knockbackForce / 60.0f) * 0.4f, 0.6f);
    playWorldSound(hitName, hitPoint, vol, pitch, 30.0f);

    if (target.currentHp <= 0) {
        DeathSystem::instance().kill(target, target.username, "player", owner.username, kbDir, knockbackForce);
    }

    if (DebugConfig::DEBUG_SWORDSWORD) {
        Debug::log(Debug::Category::General,
            "[SWORDSWORD HIT] damage=%.0f knockback=%.1f target=%s\n",
            damage, knockbackForce, target.username.c_str());
    }
}

static bool isTargetInArc(const Player& owner, const Player& target,
                           const glm::vec3& forwardDir, float range, float arcRad) {
    glm::vec3 toTarget = target.pos - owner.pos;
    float dist = glm::length(toTarget);
    if (dist > range || dist < 0.1f) return false;
    glm::vec3 dirNorm = toTarget / dist;
    float dot = glm::dot(dirNorm, forwardDir);
    return dot >= std::cos(arcRad);
}

static void checkSlashHits(SwordswordState& state, const WeaponDefinition& def,
                            Player& owner, NpcSystem& npcs) {
    float range = def.customParams.count("range") ? def.customParams.at("range") : 4.0f;
    float arcDeg = def.customParams.count("arcDegrees") ? def.customParams.at("arcDegrees") : 120.0f;
    float arcRad = glm::radians(arcDeg) * 0.5f;
    float damage = def.damage;
    float knockback = def.customParams.count("slashKnockback") ? def.customParams.at("slashKnockback") : 25.0f;
    glm::vec3 forwardDir = state.attackForward;

    if (DebugConfig::DEBUG_SWORDSWORD) {
        state.debugArcStart = owner.pos;
        state.debugArcEnd = owner.pos + forwardDir * range;
        state.debugArcAngle = arcRad;
        state.debugArcRange = range;
    }

    for (Npc& npc : npcs.all()) {
        if (npc.body.dead || npc.body.currentHp <= 0) continue;
        uint32_t id = npc.id;
        if (state.hitTargets.count(id)) continue;

        if (isTargetInArc(owner, npc.body, forwardDir, range, arcRad)) {
            glm::vec3 hitPoint = npc.body.pos;
            glm::vec3 hitNormal = glm::normalize(owner.pos - npc.body.pos);
            applyHit(owner, npc.body, hitPoint, hitNormal, damage, knockback);
            state.hitTargets[id] = true;

            if (DebugConfig::DEBUG_SWORDSWORD) {
                SwordswordState::DebugHit dh;
                dh.point = hitPoint;
                dh.normal = hitNormal;
                dh.hit = true;
                state.debugHits.push_back(dh);
            }
        }
    }
}

static void checkLungeHits(SwordswordState& state, const WeaponDefinition& def,
                            Player& owner, NpcSystem& npcs) {
    float range = def.customParams.count("lungeRange") ? def.customParams.at("lungeRange") : 6.0f;
    float damage = def.customParams.count("lungeDamage") ? def.customParams.at("lungeDamage") : def.damage * 1.5f;
    float knockback = def.customParams.count("lungeKnockback") ? def.customParams.at("lungeKnockback") : 40.0f;

    if (DebugConfig::DEBUG_SWORDSWORD) {
        state.debugTraceStart = owner.pos;
        state.debugTraceEnd = owner.pos + state.attackForward * range;
    }

    for (Npc& npc : npcs.all()) {
        if (npc.body.dead || npc.body.currentHp <= 0) continue;
        uint32_t id = npc.id;
        if (state.hitTargets.count(id)) continue;

        glm::vec3 toTarget = npc.body.pos - owner.pos;
        float dist = glm::length(toTarget);
        if (dist > range || dist < 0.1f) continue;

        glm::vec3 dirNorm = toTarget / dist;
        float dot = glm::dot(dirNorm, state.attackForward);
        float forwardness = (dot + 1.0f) * 0.5f;
        float effectiveRange = range * forwardness;

        if (dist <= effectiveRange || dot >= std::cos(glm::radians(20.0f))) {
            glm::vec3 hitPoint = npc.body.pos;
            glm::vec3 hitNormal = glm::normalize(owner.pos - npc.body.pos);
            applyHit(owner, npc.body, hitPoint, hitNormal, damage, knockback);
            state.hitTargets[id] = true;

            if (DebugConfig::DEBUG_SWORDSWORD) {
                SwordswordState::DebugHit dh;
                dh.point = hitPoint;
                dh.normal = hitNormal;
                dh.hit = true;
                state.debugHits.push_back(dh);
            }
        }
    }
}

void update(SwordswordState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            const Camera& camera, NpcSystem& npcs, float dt) {
    state.handPos = getHandPosition(owner);

    float speed = glm::length(owner.vel);
    float move01 = std::min(speed / 20.0f, 1.0f);

    state.swayTimer += dt * (2.0f + move01 * 4.0f);
    state.swayOffset = std::sin(state.swayTimer) * move01 * 0.15f;

    glm::vec3 forward2D = glm::normalize(glm::vec3(camera.front.x, camera.front.y, 0.0f));
    if (glm::length(forward2D) < 0.001f) forward2D = glm::vec3(0.0f, 1.0f, 0.0f);
    state.bladeDirection = glm::normalize(camera.front);

    float bladeLen = def.customParams.count("bladeLength") ? def.customParams.at("bladeLength") : 1.5f;
    state.bladeLength = bladeLen;

    if (state.currentAttack == SwordswordState::AttackType::Slash) {
        state.attackTimer -= dt;
        float t = 1.0f - std::max(state.attackTimer / state.attackDuration, 0.0f);
        t = std::clamp(t, 0.0f, 1.0f);

        state.slashAngle = t;

        glm::vec3 fwd = state.attackForward;
        glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 0.0f, 1.0f)));
        glm::vec3 up = glm::cross(rgt, fwd);

        float angle = glm::radians(100.0f) * t;
        glm::quat swingRot = glm::angleAxis(-angle, glm::normalize(rgt + up * 0.4f));
        state.bladeDirection = glm::normalize(swingRot * fwd);

        if (state.attackTimer > 0.0f && t > 0.1f) {
            checkSlashHits(state, def, owner, npcs);
        }

        if (state.attackTimer <= 0.0f) {
            state.currentAttack = SwordswordState::AttackType::None;
            state.hitTargets.clear();
            if (DebugConfig::DEBUG_SWORDSWORD) {
                Debug::log(Debug::Category::General, "[SWORDSWORD] slash ended\n");
            }
        }
    } else if (state.currentAttack == SwordswordState::AttackType::Lunge) {
        state.attackTimer -= dt;
        float t = 1.0f - std::max(state.attackTimer / state.attackDuration, 0.0f);
        t = std::clamp(t, 0.0f, 1.0f);

        state.lungeReach = t;

        glm::vec3 baseDir = state.attackForward;
        float pullT = std::max(0.0f, 1.0f - t * 4.0f);
        float thrustT = std::max(0.0f, (t - 0.25f) / 0.75f);
        glm::vec3 pullBack = baseDir * pullT * -0.4f;
        glm::vec3 thrust = baseDir * thrustT * 0.8f;
        state.bladeDirection = glm::normalize(baseDir + pullBack + thrust);

        if (state.attackTimer > 0.0f && t > 0.15f) {
            checkLungeHits(state, def, owner, npcs);
        }

        if (state.attackTimer <= 0.0f) {
            state.currentAttack = SwordswordState::AttackType::None;
            state.hitTargets.clear();
            if (DebugConfig::DEBUG_SWORDSWORD) {
                Debug::log(Debug::Category::General, "[SWORDSWORD] lunge ended\n");
            }
        }
    }

    state.bladeEnd = state.handPos + state.bladeDirection * state.bladeLength;

    glm::vec3 swayOffset3(0.0f);
    swayOffset3.x = state.swayOffset * 0.5f;
    swayOffset3.y = -state.swayOffset * 0.3f;
    state.bladeEnd += swayOffset3;
}

void startSlash(SwordswordState& state, const WeaponDefinition& def,
                Player& owner, const Camera& camera) {
    state.currentAttack = SwordswordState::AttackType::Slash;
    state.attackTimer = def.customParams.count("slashDuration") ? def.customParams.at("slashDuration") : 0.25f;
    state.attackDuration = state.attackTimer;
    state.attackForward = glm::normalize(camera.front);
    state.hitTargets.clear();
    state.slashAngle = 0.0f;
    state.debugHits.clear();

    float slashSpeed = def.customParams.count("slashSpeed") ? def.customParams.at("slashSpeed") : 25.0f;
    glm::vec3 horzDir = glm::normalize(glm::vec3(state.attackForward.x, state.attackForward.y, 0.0f));
    if (glm::length(horzDir) < 0.001f) horzDir = glm::vec3(0.0f, 1.0f, 0.0f);
    owner.vel = horzDir * slashSpeed;

    if (DebugConfig::DEBUG_SWORDSWORD) {
        Debug::log(Debug::Category::General, "[SWORDSWORD] slash launched vel=%.1f dir=(%.2f,%.2f,%.2f)\n",
            slashSpeed, horzDir.x, horzDir.y, horzDir.z);
    }
}

void startLunge(SwordswordState& state, const WeaponDefinition& def,
                Player& owner, const Camera& camera) {
    state.currentAttack = SwordswordState::AttackType::Lunge;
    state.attackTimer = def.customParams.count("lungeDuration") ? def.customParams.at("lungeDuration") : 0.3f;
    state.attackDuration = state.attackTimer;
    state.attackForward = glm::normalize(camera.front);
    state.hitTargets.clear();
    state.lungeReach = 0.0f;
    state.debugHits.clear();

    float lungeSpeed = def.customParams.count("lungeSpeed") ? def.customParams.at("lungeSpeed") : 40.0f;
    glm::vec3 horzDir = glm::normalize(glm::vec3(state.attackForward.x, state.attackForward.y, 0.0f));
    if (glm::length(horzDir) < 0.001f) horzDir = glm::vec3(0.0f, 1.0f, 0.0f);
    owner.vel = horzDir * lungeSpeed;

    if (DebugConfig::DEBUG_SWORDSWORD) {
        Debug::log(Debug::Category::General, "[SWORDSWORD] lunge launched vel=%.1f dir=(%.2f,%.2f,%.2f)\n",
            lungeSpeed, horzDir.x, horzDir.y, horzDir.z);
    }
}

void render(const Camera& camera, const SwordswordState& state,
            const glm::vec3& handPos) {
    if (glm::length(state.bladeEnd - handPos) < 0.01f) return;

    glm::vec3 mid = (handPos + state.bladeEnd) * 0.5f;
    glm::vec3 axis = state.bladeEnd - handPos;
    float len = glm::length(axis);
    if (len < 0.001f) return;

    DebugVis::drawFilledCylinder(camera, mid, glm::normalize(axis),
                                 0.07f, len, {0.15f, 0.4f, 1.0f, 0.9f});

    DebugVis::drawFilledCylinder(camera, mid, glm::normalize(axis),
                                 0.12f, len, {0.3f, 0.6f, 1.0f, 0.25f});

    DebugVis::drawFilledBeam(camera, handPos, state.bladeEnd, 0.04f,
                             {0.5f, 0.8f, 1.0f, 0.6f});
}

void renderDebug(const Camera& camera, const SwordswordState& state,
                 const glm::vec3& handPos) {
    if (!DebugConfig::DEBUG_SWORDSWORD) return;

    if (state.currentAttack == SwordswordState::AttackType::Slash) {
        glm::vec3 mid = (state.debugArcStart + state.debugArcEnd) * 0.5f;
        DebugVis::drawWireSphere(camera, mid, state.debugArcRange, {0.0f, 1.0f, 0.0f, 0.3f});

        if (state.debugArcAngle > 0.0f) {
            int numRays = 16;
            for (int i = 0; i < numRays; i++) {
                float angle = -state.debugArcAngle + (2.0f * state.debugArcAngle * i / (float)(numRays - 1));
                glm::quat rot = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
                glm::vec3 dir = rot * state.attackForward;
                glm::vec3 end = state.debugArcStart + dir * state.debugArcRange;
                DebugVis::drawLine(camera, state.debugArcStart, end, {0.0f, 1.0f, 0.0f, 0.15f});
            }
        }

        DebugVis::drawLine(camera, state.debugArcStart, state.debugArcEnd, {0.0f, 1.0f, 0.0f, 0.6f});
    }

    if (state.currentAttack == SwordswordState::AttackType::Lunge) {
        DebugVis::drawLine(camera, state.debugTraceStart, state.debugTraceEnd, {1.0f, 0.0f, 0.0f, 0.6f});
        DebugVis::drawWireSphere(camera, state.debugTraceEnd, 0.5f, {1.0f, 0.0f, 0.0f, 0.3f});
    }

    for (const auto& dh : state.debugHits) {
        if (dh.hit) {
            DebugVis::drawPointCross(camera, dh.point, 0.3f, {1.0f, 1.0f, 0.0f, 1.0f});
            DebugVis::drawLine(camera, dh.point, dh.point + dh.normal * 0.5f, {1.0f, 0.5f, 0.0f, 1.0f});
        }
    }

    DebugVis::drawWireSphere(camera, handPos, 0.08f, {0.0f, 0.5f, 1.0f, 1.0f});
    DebugVis::drawLine(camera, handPos, state.bladeEnd, {0.0f, 0.5f, 1.0f, 0.8f});
}

}
