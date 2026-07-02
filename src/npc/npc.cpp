#include "npc.h"
#include "npc/npc-internal.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "combat/weapon-hit.h"
#include "debug/debug-log.h"
#include "perf/perf.h"
#include "physics/config.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "devtools/dev-npc-selection.h"
#include "npc/npc-navigation.h"
#include "npc/npc-combat.h"
#include "combat/weapon-registry.h"
#include "perf/perf.h"
#include "npc/npc-state-machine.h"

static constexpr float SEARCH_TIMEOUT = 8.0f;

float targetCanSeeNpc(const Npc& npc, const World& world)
{
    glm::vec3 fromTarget = npc.sensors.targetPos;
    fromTarget.z += 0.8f;
    glm::vec3 toNpc = npc.body.pos - fromTarget;
    float dist = glm::length(toNpc);
    if (dist < 0.5f) return 0.0f;
    toNpc /= dist;

    AABB rayBounds;
    rayBounds.min = glm::min(fromTarget, npc.body.pos);
    rayBounds.max = glm::max(fromTarget, npc.body.pos);
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates);

    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
        glm::vec3 e1 = tri.b - tri.a;
        glm::vec3 e2 = tri.c - tri.a;
        glm::vec3 pVec = glm::cross(toNpc, e2);
        float det = glm::dot(e1, pVec);
        if (std::fabs(det) < 0.0001f) continue;
        float invDet = 1.0f / det;
        glm::vec3 tVec = fromTarget - tri.a;
        float u = glm::dot(tVec, pVec) * invDet;
        if (u < 0.0f || u > 1.0f) continue;
        glm::vec3 qVec = glm::cross(tVec, e1);
        float v = glm::dot(toNpc, qVec) * invDet;
        if (v < 0.0f || u + v > 1.0f) continue;
        float t = glm::dot(e2, qVec) * invDet;
        if (t > 0.1f && t < dist - 0.5f)
            return 0.0f;
    }
    return 1.0f;
}

bool shouldJump(Npc& npc, float d01, const World& world)
{
    if (glm::length(npc.lastMoveInput) > 0.1f)
    {
        glm::vec3 moveDir{npc.lastMoveInput.x, npc.lastMoveInput.y, 0.0f};
        if (NpcNavigation::obstacleInDirection(npc, moveDir, 1.8f, world))
            return true;
    }
    if (NpcNavigation::isStuck(npc))
        return true;
    return random01(npc.rngState) < (0.02f + d01 * 0.05f);
}

bool shouldDash(Npc& npc, float d01, float distance, const WeaponDefinition* def, bool targetCanSeeMe)
{
    if (npc.dashCooldown > 0.0f)
        return false;

    float healthFraction = (float)npc.body.currentHp / (float)npc.body.maxHp;

    if (distance > 10.0f && d01 > 0.3f)
        return random01(npc.rngState) < 0.6f;

    if (healthFraction < 0.35f && distance < 8.0f)
        return random01(npc.rngState) < 0.7f;

    if (targetCanSeeMe && d01 > 0.2f)
        return random01(npc.rngState) < (0.15f + d01 * 0.25f);

    return random01(npc.rngState) < (0.05f + d01 * 0.15f);
}

namespace {

glm::vec3 safePlanarNormal(glm::vec3 v, glm::vec3 fallback)
{
    v.z = 0.0f;
    float len = glm::length(v);
    if (len < 0.0001f)
        return fallback;
    return v / len;
}

glm::vec3 rotatePlanar(glm::vec3 v, float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c,
        0.0f
    };
}

static float reactionDelayForDifficulty(float difficulty) {
    float d01 = std::clamp(difficulty / 10.0f, 0.0f, 1.0f);
    float baseMs = 1500.0f - d01 * 1500.0f;
    float jitterMs = 200.0f + (1.0f - d01) * 300.0f;
    float totalMs = baseMs + (float)(rand() % (int)(jitterMs + 1.0f));
    return totalMs / 1000.0f;
}

static glm::vec3 delayedTarget(const Npc& npc, const glm::vec3& currentPos,
                                const glm::vec3& currentVel, float delaySeconds) {
    if (delaySeconds <= 0.001f || npc.posRingCount == 0)
        return currentPos;

    int tail = (npc.posRingHead - 1 + Npc::MAX_HISTORY_SAMPLES) % Npc::MAX_HISTORY_SAMPLES;
    float targetTime = npc.posRing[tail].time - delaySeconds;
    glm::vec3 bestPos = currentPos;
    glm::vec3 bestVel = currentVel;

    int count = std::min(npc.posRingCount, Npc::MAX_HISTORY_SAMPLES);
    for (int j = 0; j < count; ++j) {
        int i = (tail - j + Npc::MAX_HISTORY_SAMPLES) % Npc::MAX_HISTORY_SAMPLES;
        const auto& s = npc.posRing[i];
        if (s.time >= targetTime) {
            bestPos = s.pos;
            bestVel = s.vel;
        } else {
            int nextIdx = (i + 1) % Npc::MAX_HISTORY_SAMPLES;
            if (j > 0 && nextIdx < Npc::MAX_HISTORY_SAMPLES) {
                float t = (targetTime - s.time) / (npc.posRing[nextIdx].time - s.time);
                t = std::clamp(t, 0.0f, 1.0f);
                bestPos = glm::mix(s.pos, npc.posRing[nextIdx].pos, t);
                bestVel = glm::mix(s.vel, npc.posRing[nextIdx].vel, t);
            }
            break;
        }
    }

    float predictTime = delaySeconds * (1.0f - std::clamp((npc.difficulty / 10.0f), 0.0f, 1.0f));
    return bestPos + bestVel * predictTime;
}

void senseWorld(Npc& npc, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + npc.body.externalImpulse;
    sensors.touchFloor = npc.body.ground.hasWorldContact;

    {
        int i = npc.posRingHead;
        npc.posRing[i] = {player.pos, player.vel + player.externalImpulse, npc.sensors.time + dt};
        npc.posRingHead = (i + 1) % Npc::MAX_HISTORY_SAMPLES;
        if (npc.posRingCount < Npc::MAX_HISTORY_SAMPLES)
            npc.posRingCount++;
    }

    // DEBUG MODE: no reaction delay, immediate target tracking (temporary)
    glm::vec3 rawPos = player.pos;
    glm::vec3 rawVel = player.vel + player.externalImpulse;
    sensors.targetPos = rawPos;
    sensors.targetVel = rawVel;
    sensors.toTarget = sensors.targetPos - npc.body.pos;
    sensors.targetDistance = glm::length(sensors.toTarget);
    sensors.hasTarget = true;
    sensors.predictedTarget = sensors.targetPos;

    npc.previousPosition = npc.body.pos;

    // During search phase, use last known position as pseudo-target
    if (!sensors.hasTarget && npc.stateMachine.lastKnownAge < SEARCH_TIMEOUT && npc.stateMachine.lastKnownAge > 0.5f)
    {
        sensors.targetPos = npc.stateMachine.lastKnownTarget;
        sensors.toTarget = sensors.targetPos - npc.body.pos;
        sensors.targetDistance = glm::length(sensors.toTarget);
        sensors.hasTarget = sensors.targetDistance <= npc.tuning.awarenessRange * 1.5f;
        sensors.targetVel = glm::vec3(0.0f);
        sensors.predictedTarget = sensors.targetPos;
    }

    npc.sensors = sensors;

    if (sensors.hasTarget && npc.stateMachine.lastKnownAge < 0.1f)
    {
        npc.stateMachine.lastKnownTarget = sensors.targetPos;
        npc.stateMachine.lastKnownAge = 0.0f;
    }
    else if (!sensors.hasTarget)
    {
        npc.stateMachine.lastKnownAge += dt;
    }

    if (sensors.hasTarget && npc.lastTargetLogDistance < 0.0f)
    {
        Debug::log(Debug::Category::General,
                   "[NPC] id=%u target acquired difficulty=%.1f distance=%.2f\n",
                   npc.id, npc.difficulty, sensors.targetDistance);
    }
    if (!sensors.hasTarget && npc.lastTargetLogDistance >= 0.0f)
    {
        Debug::log(Debug::Category::General,
                   "[NPC] id=%u target lost difficulty=%.1f\n",
                   npc.id);
    }
    npc.lastTargetLogDistance = sensors.hasTarget ? sensors.targetDistance : -1.0f;
}

void logStateChange(const Npc& npc, NpcState oldState, NpcState newState)
{
    if (oldState == newState)
        return;
    Debug::log(
        Debug::Category::General,
        "[NPC] id=%u difficulty=%.1f %s -> %s target=%d dist=%.2f\n",
        npc.id,
        npc.difficulty,
        npcStateName(oldState).c_str(),
        npcStateName(newState).c_str(),
        (int)npc.sensors.hasTarget,
        npc.sensors.targetDistance
    );
}

InputState buildInputState(const Npc& npc, glm::vec3 moveDir, bool jump, bool dash, bool attack, bool downDash)
{
    InputState input;
    input.wishMoveXY = {moveDir.x, moveDir.y};
    input.movementPressed = glm::length(moveDir) > 0.001f;
    input.jumpHeld = jump;
    input.jumpPressed = jump;
    input.dashPressed = dash;
    input.groundReturnPressed = false;
    input.downDashPressed = downDash;
    input.freezeHeld = false;

    if (!npc.sensors.touchFloor && input.movementPressed)
    {
        // Air strafe: align camera forward with movement direction for air control
        glm::vec3 airMoveDir{moveDir.x, moveDir.y, 0.0f};
        float airLen = glm::length(airMoveDir);
        if (airLen > 0.001f)
            input.camForward = airMoveDir / airLen;
        else
            input.camForward = {1.0f, 0.0f, 0.0f};
    }
    else if (npc.sensors.hasTarget)
    {
        glm::vec3 toTarget = npc.sensors.predictedTarget - npc.body.pos;
        input.camForward = safePlanarNormal(toTarget, {1.0f, 0.0f, 0.0f});
    }
    else
    {
        input.camForward = safePlanarNormal(moveDir, {1.0f, 0.0f, 0.0f});
    }

    return input;
}

} // anonymous namespace

void NpcSystem::notifyCombatSound(glm::vec3 position, float intensity)
{
    int i = heardSoundHead;
    heardSounds[i] = {position, currentTime, intensity};
    heardSoundHead = (i + 1) % MAX_HEARD_SOUNDS;
    if (heardSoundCount < MAX_HEARD_SOUNDS)
        heardSoundCount++;
}

bool NpcSystem::recentCombatSoundNear(glm::vec3 pos, float maxAge, float maxDist, glm::vec3& outSource) const
{
    int count = std::min(heardSoundCount, MAX_HEARD_SOUNDS);
    int tail = (heardSoundHead - 1 + MAX_HEARD_SOUNDS) % MAX_HEARD_SOUNDS;
    for (int j = 0; j < count; ++j)
    {
        int i = (tail - j + MAX_HEARD_SOUNDS) % MAX_HEARD_SOUNDS;
        const auto& s = heardSounds[i];
        if (currentTime - s.time > maxAge)
            continue;
        float d = glm::length(s.position - pos);
        if (d < maxDist)
        {
            outSource = s.position;
            return true;
        }
    }
    return false;
}

bool NpcSystem::isNpcNear(glm::vec3 pos, float radius, uint32_t excludeId) const
{
    for (const Npc& n : npcs)
    {
        if (n.id == excludeId) continue;
        if (glm::length(n.body.pos - pos) < radius)
            return true;
    }
    return false;
}

float NpcSystem::nearestOtherNpc(glm::vec3 pos, uint32_t excludeId, glm::vec3& outPos) const
{
    float bestDist = 1e9f;
    for (const Npc& n : npcs)
    {
        if (n.id == excludeId) continue;
        float d = glm::length(n.body.pos - pos);
        if (d < bestDist)
        {
            bestDist = d;
            outPos = n.body.pos;
        }
    }
    return bestDist;
}

int NpcSystem::npcCountNear(glm::vec3 pos, float radius) const
{
    int count = 0;
    for (const Npc& n : npcs)
    {
        if (glm::length(n.body.pos - pos) < radius)
            count++;
    }
    return count;
}

void NpcSystem::update(const World& world, Player& player, float dt)
{
    Perf::ScopedTimer _updateTimer("NpcUpdate");
    currentTime += dt;
    for (Npc& nc : npcs)
    {
        // Register NPC weapon fire for other NPCs' hearing
        if (nc.attackCooldown > 0.0f && nc.sensors.hasTarget)
            notifyCombatSound(nc.body.pos, 0.5f);
        updateOneNpc(nc, world, player, dt);
    }
}

void NpcSystem::updateOneNpc(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.body.dead) {
        npc.body.updateModelWorldTransforms();
        return;
    }

    float safeDt = std::max(dt, 0.0001f);

    npc.dashCooldown = std::max(0.0f, npc.dashCooldown - safeDt);
    npc.downDashCooldown = std::max(0.0f, npc.downDashCooldown - safeDt);
    npc.attackCooldown = std::max(0.0f, npc.attackCooldown - safeDt);
    npc.hitReactionTimer = std::max(0.0f, npc.hitReactionTimer - safeDt);
    npc.aimTimer = std::max(0.0f, npc.aimTimer);
    npc.stateMachine.stateTimer += safeDt;
    npc.stateMachine.nextDecisionTime -= safeDt;
    npc.stateMachine.retreatTimer += safeDt;

    // Process reload in main update (not inside tryFire) so it ticks during movement states
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
        if (def)
        {
            auto& rt = npc.body.weaponRuntimes[def->id];
            if (rt.isReloading)
            {
                rt.reloadTimer -= safeDt;
                if (rt.reloadTimer <= 0.0f)
                {
                    int toLoad = def->magazineSize - rt.currentAmmo;
                    int available = std::min(toLoad, rt.reserveAmmo);
                    rt.currentAmmo += available;
                    rt.reserveAmmo -= available;
                    rt.isReloading = false;
                    Debug::log(Debug::Category::NpcCombat,
                        "[NPC RELOAD] npc=%u weapon=%s complete ammo=%d reserve=%d",
                        npc.id, def->id.c_str(), rt.currentAmmo, rt.reserveAmmo);
                }
            }
        }
    }

    senseWorld(npc, player, safeDt);

    // DEBUG MODE: no hearing, no hit reaction override, no state machine (temporary)
    // Force Attack state whenever target exists
    npc.stateMachine.currentState = npc.sensors.hasTarget ? NpcState::Attack : NpcState::Idle;
    npc.hitReactionTimer = 0.0f;

    // DEBUG MODE: minimal movement, face target and shoot (temporary)
    glm::vec3 moveDir{0.0f};
    bool jump = false;
    bool dash = false;
    bool attack = npc.sensors.hasTarget;

    InputState input = buildInputState(npc, moveDir, jump, dash, attack, false);
    {
        Perf::ScopedTimer _npcCollision("NpcCollision");
        glm::vec3 velocityBefore = npc.body.vel;
        float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));

        physicsMainUpdate(npc.body, world, input, safeDt);

        float planarSpeedAfter = glm::length(glm::vec2(npc.body.vel.x, npc.body.vel.y));
        npc.lastMoveInput = input.wishMoveXY;
        npc.lastAcceleration = (npc.body.vel - velocityBefore) / safeDt;
        npc.lastGravityDelta = npc.body.vel.z - velocityBefore.z;
        npc.lastFrictionDelta = input.movementPressed ? 0.0f : planarSpeedAfter - planarSpeedBefore;
        npc.lastFinalSpeed = glm::length(npc.body.vel + npc.body.externalImpulse);

        if (DebugConfig::DEBUG_NPC)
        {
            std::string cmdKey = "npc-cmd-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, cmdKey.c_str(), DebugConfig::PRINT_INTERVAL,
                "[NPC] id=%u state=%s jump=%d dash=%d move=(%.2f %.2f)\n",
                npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
                (int)input.jumpHeld, (int)input.dashPressed,
                input.wishMoveXY.x, input.wishMoveXY.y);

            std::string physKey = "npc-phys-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, physKey.c_str(), DebugConfig::PRINT_INTERVAL,
            "[NPC PHYS] id=%u floor=%d vel=(%.2f %.2f %.2f) finalSpeed=%.2f\n",
            npc.id, (int)npc.sensors.touchFloor,
            npc.body.vel.x, npc.body.vel.y, npc.body.vel.z,
            npc.lastFinalSpeed);
    }

    if (DebugConfig::DEBUG_NPC_COMBAT && npc.sensors.hasTarget)
    {
        float aimErrDeg = NpcCombat::aimErrorDegrees(npc.difficulty);
        printf("[NPC] id=%u state=%s dist=%.1f aimError=%.2f canSee=%d "
               "aimTimer=%.2f reactionTimer=%.2f\n",
               npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
               npc.sensors.targetDistance, aimErrDeg,
               (int)(npc.sensors.targetDistance <= npc.tuning.awarenessRange),
               npc.aimTimer, npc.reactionTimer);
    }
    }

    // DEBUG MODE: reactionTimer always 0, no dashing (temporary)
    npc.reactionTimer = 0.0f;

    if (attack && npc.attackCooldown <= 0.0f)
    {
        Perf::ScopedTimer _combatTimer("NpcCombat");
        bool fired = NpcCombat::tryFire(npc, world, player, safeDt);
        if (fired)
        {
            npc.stateMachine.nextDecisionTime = 0.0f;
        }
    }
}

void NpcSystem::render(const Camera& camera) const
{
    Perf::ScopedTimer _t("NpcRender");
    for (const Npc& npc : npcs)
        renderPlayer(npc.body, camera);
}

void NpcSystem::drawDebug(const Camera& camera) const
{
    DebugVis::drawNpcDebugStuff(debugInfo(), camera);

    if (!DebugVis::masterEnabled() || !DebugConfig::DEBUG_NPC)
        return;

    for (const Npc& npc : npcs)
    {
        glm::vec3 eye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);

        // Line of sight to player target
        glm::vec3 playerEye = npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f);
        DebugVis::drawLine(camera, eye, playerEye, glm::vec4(0.0f, 1.0f, 0.0f, 0.3f));

        // Perfect aim direction (yellow)
        glm::vec3 toTarget = playerEye - eye;
        float tLen = glm::length(toTarget);
        if (tLen > 0.1f)
        {
            glm::vec3 idealDir = toTarget / tLen;
            DebugVis::drawLine(camera, eye, eye + idealDir * 10.0f, glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
        }

        // Target point (red sphere)
        DebugVis::drawWireSphere(camera, playerEye, 0.1f, glm::vec4(1.0f, 0.0f, 0.0f, 0.9f));

        // Accuracy info label
        float maxErr = NpcCombat::maxAngularErrorForAccuracy(gNpcAimAccuracy);
        char label[192];
        int n = snprintf(label, sizeof(label), "NPC %u aimAcc=%.2f maxErr=%.1fdeg",
            npc.id, gNpcAimAccuracy, maxErr);
        const WeaponDefinition* wDef = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
        if (wDef)
        {
            auto it = npc.body.weaponRuntimes.find(wDef->id);
            if (it != npc.body.weaponRuntimes.end())
            {
                snprintf(label + n, sizeof(label) - n,
                    " ammo=%d%s cd=%.3f",
                    it->second.currentAmmo,
                    it->second.isReloading ? " RELOAD" : "",
                    npc.attackCooldown);
            }
        }
        DebugVis::drawWorldLabel(eye + glm::vec3(0.0f, 0.0f, 0.5f), label,
            glm::vec4(0.0f, 1.0f, 1.0f, 0.9f));
    }
}

std::vector<DebugVis::NpcDebugInfo> NpcSystem::debugInfo() const
{
    std::vector<DebugVis::NpcDebugInfo> out;
    out.reserve(npcs.size());

    for (const Npc& npc : npcs)
    {
        DebugVis::NpcDebugInfo info;
        info.position = npc.body.pos;
        info.velocity = npc.body.vel + npc.body.externalImpulse;
        info.acceleration = npc.lastAcceleration;
        info.targetPosition = npc.sensors.targetPos;
        info.moveDirection = glm::vec3(npc.lastMoveInput, 0.0f);
        info.pathTarget = npc.stateMachine.wanderTarget;
        info.action = npcStateName(npc.stateMachine.currentState);
        info.difficulty = npc.difficulty;
        info.awarenessRadius = npc.tuning.awarenessRange;
        info.finalSpeed = npc.lastFinalSpeed;
        info.onFloor = npc.sensors.touchFloor;
        info.hasTarget = npc.sensors.hasTarget;
        out.push_back(info);
    }

    return out;
}
