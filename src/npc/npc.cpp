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
#include "physics/movement/physics-collision-shared.h"
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
        appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates, "targetCanSeeNpc");

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

InputState buildInputState(Npc& npc, glm::vec3 moveDir, bool jump, bool dash, bool attack, bool downDash, float dt)
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

    glm::vec3 desiredFwd;
    if (!npc.sensors.touchFloor && input.movementPressed)
    {
        glm::vec3 airMoveDir{moveDir.x, moveDir.y, 0.0f};
        float airLen = glm::length(airMoveDir);
        desiredFwd = airLen > 0.001f ? (airMoveDir / airLen) : glm::vec3{1.0f, 0.0f, 0.0f};
    }
    else if (npc.sensors.hasTarget)
    {
        glm::vec3 npcEye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);
        glm::vec3 aimDir = NpcCombat::aimAtTarget(npc, npcEye, npc.sensors.targetPos, npc.sensors.targetVel);
        desiredFwd = safePlanarNormal(aimDir, {1.0f, 0.0f, 0.0f});
    }
    else
    {
        desiredFwd = safePlanarNormal(moveDir, {1.0f, 0.0f, 0.0f});
    }

    // Apply turn speed limiting: smoothly rotate currentFacing toward desiredFwd
    float maxTurnAngle = npc.tuning.turnSpeed * dt;  // degrees this frame
    float angleDiff = glm::degrees(std::acos(
        std::clamp(glm::dot(npc.currentFacing, desiredFwd), -1.0f, 1.0f)));
    if (angleDiff > maxTurnAngle && maxTurnAngle > 0.0f) {
        float t = maxTurnAngle / angleDiff;
        npc.currentFacing = glm::normalize(
            glm::mix(npc.currentFacing, desiredFwd, t));
    } else {
        npc.currentFacing = desiredFwd;
    }
    input.camForward = npc.currentFacing;

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
    Perf::state().current.npcCount = (int)npcs.size();
    currentTime += dt;
    for (Npc& nc : npcs)
    {
        // Register NPC weapon fire for other NPCs' hearing
        if (nc.attackCooldown > 0.0f && nc.sensors.hasTarget)
            notifyCombatSound(nc.body.pos, 0.5f);

        auto tNpcStart = std::chrono::steady_clock::now();
        updateOneNpc(nc, world, player, dt);
        auto tNpcEnd = std::chrono::steady_clock::now();
        double npcMs = std::chrono::duration<double, std::milli>(tNpcEnd - tNpcStart).count();
        Perf::collectNpcProfile(nc.id, "total", npcMs);
    }
    Perf::flushNpcProfiles();
}

void NpcSystem::updateOneWithTarget(uint32_t npcId, const World& world, Player& player, float dt)
{
    for (Npc& nc : npcs)
    {
        if (nc.id != npcId)
            continue;
        // Register NPC weapon fire for other NPCs' hearing (same as update()).
        if (nc.attackCooldown > 0.0f && nc.sensors.hasTarget)
            notifyCombatSound(nc.body.pos, 0.5f);
        updateOneNpc(nc, world, player, dt);
        break;
    }
}

void NpcSystem::updateOneNpc(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.body.dead || npc.body.currentHp <= 0) {
        npc.body.updateModelWorldTransforms();
        return;
    }

    float safeDt = std::max(dt, 0.0001f);

    if (npc.wakeupTimer > 0.0f) {
        npc.wakeupTimer -= safeDt;
        npc.body.updateModelWorldTransforms();
        return;
    }

    npc.dashCooldown = std::max(0.0f, npc.dashCooldown - safeDt);
    npc.downDashCooldown = std::max(0.0f, npc.downDashCooldown - safeDt);
    npc.attackCooldown = std::max(0.0f, npc.attackCooldown - safeDt);
    npc.hitReactionTimer = std::max(0.0f, npc.hitReactionTimer - safeDt);
    npc.aimTimer = std::max(0.0f, npc.aimTimer);
    npc.stateMachine.stateTimer += safeDt;
    npc.stateMachine.nextDecisionTime -= safeDt;
    npc.stateMachine.retreatTimer += safeDt;

    npc.timeSinceLastShot += safeDt;
    npc.fireRhythmOffset = std::sin(npc.sensors.time * 0.4f + npc.id * 2.1f);

    // Update model-world transforms once per simulation tick
    // (collision code reads these without recomputing them)
    npc.body.updateModelWorldTransforms();

    // Cache weapon definition once per frame (avoids 3+ string-keyed map lookups)
    const WeaponDefinition* cachedWeaponDef = WeaponRegistry::instance().get(npc.body.equippedWeaponId);

    // Process reload in main update (not inside tryFire) so it ticks during movement states
    if (cachedWeaponDef)
    {
        auto& rt = npc.body.weaponRuntimes[cachedWeaponDef->id];
        if (rt.isReloading)
        {
            rt.reloadTimer -= safeDt;
            if (rt.reloadTimer <= 0.0f)
            {
                int toLoad = cachedWeaponDef->magazineSize - rt.currentAmmo;
                int available = std::min(toLoad, rt.reserveAmmo);
                rt.currentAmmo += available;
                rt.reserveAmmo -= available;
                rt.isReloading = false;
                Debug::log(Debug::Category::NpcCombat,
                    "[NPC RELOAD] npc=%u weapon=%s complete ammo=%d reserve=%d",
                    npc.id, cachedWeaponDef->id.c_str(), rt.currentAmmo, rt.reserveAmmo);
            }
        }
    }

    senseWorld(npc, player, safeDt);

    // Hearing: if no target, react to nearby combat sounds
    if (!npc.sensors.hasTarget && npc.stateMachine.lastKnownAge > 2.0f)
    {
        glm::vec3 soundSource;
        float hearRange = 20.0f + npc.tuning.awarenessRange * 0.5f;
        if (recentCombatSoundNear(npc.body.pos, 3.0f, hearRange, soundSource))
        {
            npc.stateMachine.lastKnownTarget = soundSource;
            npc.stateMachine.lastKnownAge = 0.0f;
        }
    }

    bool wantDownDash = false;
    if (npc.sensors.hasTarget && !npc.sensors.touchFloor && npc.downDashCooldown <= 0.0f)
    {
        float heightAbove = npc.body.pos.z - npc.sensors.targetPos.z;
        if (heightAbove > 3.0f)
            wantDownDash = true;
    }

    if (npc.hitReactionTimer > 0.0f)
    {
        npc.stateMachine.currentState = NpcState::Recover;
        npc.stateMachine.recoverTimer = npc.hitReactionTimer;
        npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, npc.hitReactionTimer + 0.1f);
    }

    if (npc.trainingMode != 2) {
        if (npc.trainingMode == 0) {
            npc.stateMachine.currentState = NpcState::Idle;
            npc.stateMachine.nextDecisionTime = 2.0f;
        } else if (npc.trainingMode == 1) {
            npc.stateMachine.currentState = NpcState::Retreat;
            npc.stateMachine.retreatTimer = 0.0f;
            npc.stateMachine.nextDecisionTime = 0.3f;
        }
    } else {
        if (npc.stateMachine.nextDecisionTime <= 0.0f)
        {
            Perf::ScopedTimer _decTimer("NpcSecondDecision");
            NpcState oldState = npc.stateMachine.currentState;
            NpcState newState = pickNextState(npc);

            if (newState == NpcState::Retreat && oldState != NpcState::Retreat)
                npc.stateMachine.retreatTimer = 0.0f;

            if (newState == NpcState::Recover)
                npc.stateMachine.recoverTimer = 0.2f + random01(npc.rngState) * 0.3f;

            if (newState == NpcState::Circle && oldState != NpcState::Circle)
            {
                npc.stateMachine.orbitSwapTimer = 0.1f + random01(npc.rngState) * 1.5f;
                glm::vec3 otherPos;
                float nearest;
                { Perf::ScopedTimer _t("NpcNearestOther"); nearest = nearestOtherNpc(npc.body.pos, npc.id, otherPos); }
                if (nearest < 8.0f)
                {
                    glm::vec2 toOther(otherPos.x - npc.body.pos.x, otherPos.y - npc.body.pos.y);
                    glm::vec2 toTarget(npc.sensors.targetPos.x - npc.body.pos.x, npc.sensors.targetPos.y - npc.body.pos.y);
                    float cross = toTarget.x * toOther.y - toTarget.y * toOther.x;
                    npc.stateMachine.orbitDirection = cross > 0.0f ? 1.0f : -1.0f;
                }
                else
                {
                    npc.stateMachine.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                }
                npc.stateMachine.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            if (newState == NpcState::Strafe && oldState != NpcState::Strafe)
            {
                glm::vec3 otherPos;
                float nearest;
                { Perf::ScopedTimer _t("NpcNearestOther"); nearest = nearestOtherNpc(npc.body.pos, npc.id, otherPos); }
                if (nearest < 8.0f)
                {
                    glm::vec2 toOther(otherPos.x - npc.body.pos.x, otherPos.y - npc.body.pos.y);
                    glm::vec2 toTarget(npc.sensors.targetPos.x - npc.body.pos.x, npc.sensors.targetPos.y - npc.body.pos.y);
                    float cross = toTarget.x * toOther.y - toTarget.y * toOther.x;
                    npc.stateMachine.strafeDirection = cross > 0.0f ? -1.0f : 1.0f;
                }
                else
                {
                    npc.stateMachine.strafeDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                }
                npc.stateMachine.strafeSwapTimer = 0.3f + random01(npc.rngState) * 2.0f;
            }

            logStateChange(npc, oldState, newState);
            npc.stateMachine.previousState = oldState;
            npc.stateMachine.currentState = newState;
            npc.stateMachine.stateTimer = 0.0f;

            float minT = stateMinTime(newState, difficulty01(npc.difficulty));
            float maxT = stateMaxTime(newState, difficulty01(npc.difficulty));
            npc.stateMachine.nextDecisionTime = minT + random01(npc.rngState) * (maxT - minT);
        }
    }

    // Pre-gather collision triangles once for local navigation checks (3m covers obstacle, climbable, wall avoid)
    glm::vec3 gatherCenter = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f);
    AABB localBounds{gatherCenter - glm::vec3(3.0f), gatherCenter + glm::vec3(3.0f)};
    std::vector<int> nearCandidates;
        appendChunkTrianglesForAABB(world, localBounds, 0.0f, nearCandidates, "npcNearCandidates");

    glm::vec3 moveDir;
    bool jump, dash, attack;
    computeStateMovement(npc, moveDir, jump, dash, attack, safeDt);

    // Situaltional jump if obstacle ahead or stuck
    if (npc.sensors.touchFloor && !jump && glm::length(moveDir) > 0.1f)
    {
        jump = NpcNavigation::obstacleInDirection(npc, moveDir, 1.8f, world, nearCandidates)
            || NpcNavigation::isStuck(npc);
    }

    // Wall climb
    if (npc.sensors.touchFloor && !jump && glm::length(moveDir) > 0.1f)
    {
        glm::vec3 wallNormal;
        if (NpcNavigation::isClimbableWall(npc, moveDir, world, wallNormal, nearCandidates))
            jump = true;
    }

    // Compute LOS once, cached for dash decision and combat line-of-sight.
    // Rate-limited: only check every 5 ticks to reduce chunk query cost.
    // Uses ray-based chunk-cell traversal instead of full AABB query.
    {
        static const int LOS_INTERVAL = 5;
        if (++npc.losTickCounter >= LOS_INTERVAL)
            npc.losTickCounter = 0;

        glm::vec3 fromPos = npc.body.pos + NpcCombat::npcMuzzleOffset();
        glm::vec3 toPos = npc.sensors.targetPos + NpcCombat::npcMuzzleOffset();
        glm::vec3 losDir = toPos - fromPos;
        float losDist = glm::length(losDir);

        if (losDist > 0.5f && npc.sensors.hasTarget && npc.losTickCounter == 0)
        {
            losDir /= losDist;

            // Ray-based chunk traversal: walk through chunk cells the ray passes
            // instead of querying all cells in the bounding box.
            npc.cachedLoSBlocked = false;
            float chunkSize = world.collisionChunkSize;
            if (chunkSize > 0.001f && !world.collisionChunks.empty())
            {
                glm::vec3 pos = fromPos;
                glm::vec3 step;
                step.x = losDir.x > 0.0f ? chunkSize : -chunkSize;
                step.y = losDir.y > 0.0f ? chunkSize : -chunkSize;
                step.z = losDir.z > 0.0f ? chunkSize : -chunkSize;

                glm::ivec3 cell(
                    (int)std::floor(pos.x / chunkSize),
                    (int)std::floor(pos.y / chunkSize),
                    (int)std::floor(pos.z / chunkSize));

                glm::vec3 tMax(
                    ((cell.x + (losDir.x > 0.0f ? 1 : 0)) * chunkSize - pos.x) / losDir.x,
                    ((cell.y + (losDir.y > 0.0f ? 1 : 0)) * chunkSize - pos.y) / losDir.y,
                    ((cell.z + (losDir.z > 0.0f ? 1 : 0)) * chunkSize - pos.z) / losDir.z);
                glm::vec3 tDelta(chunkSize / std::fabs(losDir.x + 0.0001f),
                                 chunkSize / std::fabs(losDir.y + 0.0001f),
                                 chunkSize / std::fabs(losDir.z + 0.0001f));
                glm::ivec3 stepDir(losDir.x > 0 ? 1 : -1,
                                   losDir.y > 0 ? 1 : -1,
                                   losDir.z > 0 ? 1 : -1);

                float remaining = losDist;
                int maxSteps = 200;
                while (remaining > 0.5f && maxSteps-- > 0)
                {
                    auto it = world.collisionChunks.find(cell);
                    if (it != world.collisionChunks.end())
                    {
                        for (int triIdx : it->second)
                        {
                            if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size())
                                continue;
                            const CollisionTriangle& tri = world.collisionMesh.triangles[triIdx];
                            glm::vec3 e1 = tri.b - tri.a;
                            glm::vec3 e2 = tri.c - tri.a;
                            glm::vec3 pVec = glm::cross(losDir, e2);
                            float det = glm::dot(e1, pVec);
                            if (std::fabs(det) < 0.0001f) continue;
                            float invDet = 1.0f / det;
                            glm::vec3 tVec = fromPos - tri.a;
                            float u = glm::dot(tVec, pVec) * invDet;
                            if (u < 0.0f || u > 1.0f) continue;
                            glm::vec3 qVec = glm::cross(tVec, e1);
                            float v = glm::dot(losDir, qVec) * invDet;
                            if (v < 0.0f || u + v > 1.0f) continue;
                            float t = glm::dot(e2, qVec) * invDet;
                            if (t > 0.1f && t < losDist - 0.5f)
                            {
                                npc.cachedLoSBlocked = true;
                                break;
                            }
                        }
                        if (npc.cachedLoSBlocked) break;
                    }

                    // Advance DDA to next cell
                    if (tMax.x < tMax.y)
                    {
                        if (tMax.x < tMax.z) { remaining -= tMax.x - (tMax.x - tDelta.x); tMax.x += tDelta.x; cell.x += stepDir.x; }
                        else { remaining -= tMax.z - (tMax.z - tDelta.z); tMax.z += tDelta.z; cell.z += stepDir.z; }
                    }
                    else
                    {
                        if (tMax.y < tMax.z) { remaining -= tMax.y - (tMax.y - tDelta.y); tMax.y += tDelta.y; cell.y += stepDir.y; }
                        else { remaining -= tMax.z - (tMax.z - tDelta.z); tMax.z += tDelta.z; cell.z += stepDir.z; }
                    }
                }
            }
            else
            {
                // Fallback: iterate all triangles
                for (const auto& tri : world.collisionMesh.triangles)
                {
                    glm::vec3 e1 = tri.b - tri.a;
                    glm::vec3 e2 = tri.c - tri.a;
                    glm::vec3 pVec = glm::cross(losDir, e2);
                    float det = glm::dot(e1, pVec);
                    if (std::fabs(det) < 0.0001f) continue;
                    float invDet = 1.0f / det;
                    glm::vec3 tVec = fromPos - tri.a;
                    float u = glm::dot(tVec, pVec) * invDet;
                    if (u < 0.0f || u > 1.0f) continue;
                    glm::vec3 qVec = glm::cross(tVec, e1);
                    float v = glm::dot(losDir, qVec) * invDet;
                    if (v < 0.0f || u + v > 1.0f) continue;
                    float t = glm::dot(e2, qVec) * invDet;
                    if (t > 0.1f && t < losDist - 0.5f)
                    {
                        npc.cachedLoSBlocked = true;
                        break;
                    }
                }
            }
        }
        // If LOS was not checked this tick, cachedLoSBlocked retains its previous value.
        // This means stale LOS results persist for up to LOS_INTERVAL ticks, which is fine
        // for dash decisions and AI targeting.
    }

    // Situaltional dash (skip LOS gather when dash is on cooldown)
    if (!dash && npc.sensors.hasTarget && npc.dashCooldown <= 0.0f)
    {
        bool targetCanSee = !npc.cachedLoSBlocked;
        dash = shouldDash(npc, difficulty01(npc.difficulty), npc.sensors.targetDistance, cachedWeaponDef, targetCanSee);
    }

    // Cover seeking
    if (npc.sensors.hasTarget && glm::length(moveDir) > 0.001f)
    {
        bool wantsCover = npc.stateMachine.currentState == NpcState::Recover;
        if (!wantsCover)
        {
            float healthFraction = (float)npc.body.currentHp / (float)npc.body.maxHp;
            wantsCover = healthFraction < 0.4f;
        }
        if (!wantsCover)
        {
            const auto& rt = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
            wantsCover = rt != npc.body.weaponRuntimes.end() && rt->second.isReloading;
        }

        if (wantsCover)
        {
            glm::vec3 coverDir = NpcNavigation::findCoverDirection(npc, npc.sensors.targetPos, world);
            if (glm::length(coverDir) > 0.001f)
            {
                float coverBlend = 0.5f;
                moveDir = glm::normalize(moveDir + coverDir * coverBlend);
            }
        }
    }

    if (npc.bombTagActive)
    {
        if (npc.bombTagHasBomb)
        {
            glm::vec3 toTarget = npc.bombTagChaseTarget - npc.body.pos;
            float dist = glm::length(toTarget);
            if (dist > 0.5f)
            {
                moveDir = toTarget / dist;
                jump = dist > 2.0f && npc.body.pos.z < npc.bombTagChaseTarget.z - 0.5f;
                attack = false;
                dash = dist > 4.0f && npc.dashCooldown <= 0.0f;
            }
            npc.sensors.hasTarget = true;
            npc.sensors.targetPos = npc.bombTagChaseTarget;
        }
        else
        {
            glm::vec3 fromTarget = npc.body.pos - npc.bombTagFleeFrom;
            float dist = glm::length(fromTarget);
            if (dist > 0.1f)
            {
                moveDir = fromTarget / dist;
                if (dist < 3.0f && npc.dashCooldown <= 0.0f)
                    dash = true;
            }
            if (dist < 8.0f)
                npc.sensors.hasTarget = true;
        }
    }

    {
        Perf::ScopedTimer _pathTimer("NpcPathfinding");
        if (glm::length(moveDir) > 0.001f)
            moveDir = NpcNavigation::wallAvoidDirection(npc, moveDir, world, nearCandidates);

        if (NpcNavigation::isStuck(npc))
        {
            npc.stateMachine.stuckTimer += safeDt;
            if (npc.stateMachine.stuckTimer > 0.3f)
            {
                moveDir = NpcNavigation::unstuckDirection(npc, npc.rngState, world, nearCandidates);
                jump = true;
                dash = npc.dashCooldown <= 0.0f;
                npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, 0.3f);
            }
        }
        else
        {
            npc.stateMachine.stuckTimer = 0.0f;
        }
    }

    // Freeze: occasionally freeze to dodge shots / break prediction
    bool freeze = false;
    if (npc.sensors.hasTarget && npc.sensors.touchFloor && npc.dashCooldown <= 0.0f
        && npc.body.freeze.freezeAvailable && npc.body.freeze.freezeTimer <= 0.0f)
    {
        freeze = random01(npc.rngState) < 0.003f;
    }

    InputState input = buildInputState(npc, moveDir, jump, dash, attack, wantDownDash, safeDt);
    input.freezeHeld = freeze;
    input.groundReturnPressed = false;
    if (input.dashPressed)
        npc.dashCommandConsumed = true;

    bool downDashAvailableBefore = npc.body.dash.downDashAvailable;
    {
        Perf::ScopedTimer _npcCollision("NpcCollision");
        char entityLabel[32];
        std::snprintf(entityLabel, sizeof(entityLabel), "NPC_%u", npc.id);
        setCollisionEntityContext(entityLabel, npc.id, true);

        glm::vec3 velocityBefore = npc.body.vel;
        float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));

        physicsMainUpdate(npc.body, world, input, safeDt, 2);

        clearCollisionEntityContext();

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
                "[NPC] id=%u state=%s jump=%d dash=%d freeze=%d move=(%.2f %.2f)\n",
                npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
                (int)input.jumpHeld, (int)input.dashPressed, (int)input.freezeHeld,
                input.wishMoveXY.x, input.wishMoveXY.y);

            std::string physKey = "npc-phys-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, physKey.c_str(), DebugConfig::PRINT_INTERVAL,
            "[NPC PHYS] id=%u floor=%d vel=(%.2f %.2f %.2f) finalSpeed=%.2f\n",
            npc.id, (int)npc.sensors.touchFloor,
            npc.body.vel.x, npc.body.vel.y, npc.body.vel.z,
            npc.lastFinalSpeed);
        }
    }

    if (input.dashPressed && npc.body.dash.didDash)
    {
        npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
        EffectPartSystem::instance().spawnDash(npc.body.pos);
        playWorldSound("entity/player/dash", npc.body.pos, 1.0f, 1.0f, 36.0f);
    }

    if (wantDownDash && downDashAvailableBefore && !npc.body.dash.downDashAvailable)
    {
        npc.downDashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.50f;
    }

    // No reaction timer — NPC fires immediately when cooldown expires
    npc.reactionTimer = 0.0f;

    if (attack && npc.attackCooldown <= 0.0f)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC FIRE] npc=%u timeSinceLastShot=%.3f\n",
            npc.id, npc.timeSinceLastShot);
        Perf::ScopedTimer _combatTimer("NpcCombat");
        bool fired = NpcCombat::tryFire(npc, world, player, safeDt);
        if (fired)
        {
            npc.timeSinceLastShot = 0.0f;
            // Do NOT reset nextDecisionTime — let the state machine keep its
            // current state for its minimum duration to prevent jitter.
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
