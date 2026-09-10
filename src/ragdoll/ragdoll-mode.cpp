#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>
#include <glm/gtx/quaternion.hpp>

#include "config.h"
#include "camera.h"
#include "entities/player.h"
#include "input/input-state.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/ray-utils.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "debug/structured-log.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

static constexpr int PBD_ITERATIONS = 4;
static constexpr float GRAB_SPRING = 5000.0f;
static constexpr float GRAB_DAMPING = 200.0f;
static constexpr float MAX_PART_SPEED = 30.0f;
static constexpr float POSITION_CORRECTION = 0.8f;

static int findPartByName(const std::vector<RagdollModePart>& parts, const std::string& name)
{
    for (int i = 0; i < (int)parts.size(); ++i)
        if (parts[i].name == name) return i;
    return -1;
}

static bool raycastWorld(const World& world, const glm::vec3& origin,
                          const glm::vec3& dir, float maxDist,
                          glm::vec3& hitPoint, glm::vec3& hitNormal)
{
    float closestDist = maxDist;
    bool hit = false;

    AABB queryBounds;
    queryBounds.min = glm::min(origin, origin + dir * maxDist) - glm::vec3(1.0f);
    queryBounds.max = glm::max(origin, origin + dir * maxDist) + glm::vec3(1.0f);
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates, "ragdollModeGrab");

    for (int ti : candidates) {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
            continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
        float dist = 0.0f;
        glm::vec3 normal, point;
        if (sweptSphereTriangle(origin, dir, 0.01f, tri, maxDist, dist, normal, point)) {
            if (dist < closestDist) {
                closestDist = dist;
                hitPoint = point;
                hitNormal = normal;
                hit = true;
            }
        }
    }
    return hit;
}

RagdollModeSystem& RagdollModeSystem::instance()
{
    static RagdollModeSystem sys;
    return sys;
}

void RagdollModeSystem::activate(Player& player)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    if (!cfg.enabled) return;

    mActive = true;
    mActivationTime = 0.0f;
    mTorsoPosition = player.pos;
    mTorsoRotation = glm::angleAxis(glm::radians(player.yaw), glm::vec3(0, 0, 1));

    mLeftGrab = {};
    mRightGrab = {};

    initParts(player);

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Activated — %zu parts\n", mParts.size());

    // Structured log: RAGDOLL_ENTER
    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        char msg[2048];
        int off = 0;
        off += snprintf(msg + off, sizeof(msg) - off,
            "[RAGDOLLDEBUG] RAGDOLL_ENTER player=%s pos=(%.3f,%.3f,%.3f) "
            "config=config/ragdoll.json grounded=%d yaw=%.3f tick=%u ",
            player.username.c_str(),
            player.pos.x, player.pos.y, player.pos.z,
            (int)player.ground.onGround,
            player.yaw,
            (uint32_t)player.movementSimulationTick);
        for (const auto& part : mParts) {
            off += snprintf(msg + off, sizeof(msg) - off,
                "part=%s pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) "
                "capsule_center=(%.3f,%.3f,%.3f) capsule_r=%.3f capsule_hh=%.3f ",
                part.name.c_str(),
                part.position.x, part.position.y, part.position.z,
                part.velocity.x, part.velocity.y, part.velocity.z,
                part.position.x, part.position.y, part.position.z,
                part.capsuleRadius, part.capsuleHalfHeight);
        }
        off += snprintf(msg + off, sizeof(msg) - off,
            "grab_left=%d grab_right=%d",
            (int)mLeftGrab.active, (int)mRightGrab.active);
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Ragdoll;
        e.level = StructuredLevel::Trace;
        e.eventId = "RAGDOLL_ENTER";
        e.correlationId = player.username;
        e.reason = "Player entered ragdoll mode";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.tick = (uint32_t)player.movementSimulationTick;
        e.message = msg;
        StructuredLogger::instance().write(e);
    }
}

void RagdollModeSystem::deactivate(Player& player)
{
    if (mTorsoIndex >= 0 && mTorsoIndex < (int)mParts.size()) {
        player.pos = mParts[mTorsoIndex].position;
        player.vel = mParts[mTorsoIndex].velocity;
    }
    mActive = false;
    mParts.clear();
    mLeftGrab = {};
    mRightGrab = {};

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Deactivated\n");
}

void RagdollModeSystem::initParts(const Player& player)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    mParts.clear();

    struct PartDef {
        const char* name;
        float mass;
    };

    const PartDef defs[] = {
        {"torso",    5.0f},
        {"head",     2.0f},
        {"leftArm",  1.5f},
        {"rightArm", 1.5f},
        {"leftLeg",  2.0f},
        {"rightLeg", 2.0f},
    };

    for (const auto& d : defs) {
        RagdollModePart part;
        part.name = d.name;
        part.mass = d.mass;

        auto capIt = cfg.capsules.find(d.name);
        if (capIt != cfg.capsules.end()) {
            part.capsuleRadius = capIt->second.radius;
            part.capsuleHalfHeight = capIt->second.halfHeight;
            part.restOffset = capIt->second.offset;
        }

        part.position = mTorsoPosition + glm::vec3(mTorsoRotation * glm::vec4(part.restOffset, 0.0f));
        part.previousPosition = part.position;
        part.velocity = player.vel;
        part.rotation = mTorsoRotation;

        int idx = (int)mParts.size();
        mParts.push_back(std::move(part));

        if (d.name == "torso") mTorsoIndex = idx;
        else if (d.name == "head") mHeadIndex = idx;
        else if (d.name == "leftArm") mLeftArmIndex = idx;
        else if (d.name == "rightArm") mRightArmIndex = idx;
        else if (d.name == "leftLeg") mLeftLegIndex = idx;
        else if (d.name == "rightLeg") mRightLegIndex = idx;
    }

    const char* attachNames[] = {"head", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    for (const char* name : attachNames) {
        int childIdx = findPartByName(mParts, name);
        if (childIdx < 0) continue;

        auto attIt = cfg.attachments.find(name);
        if (attIt == cfg.attachments.end()) continue;

        int parentIdx = findPartByName(mParts, attIt->second.parent);
        if (parentIdx < 0) continue;

        mParts[childIdx].parentIndex = parentIdx;
        mParts[childIdx].parentAttachmentOffset = attIt->second.offset;
        mParts[childIdx].coneLimitDeg = attIt->second.coneLimitDeg;

        glm::vec3 parentAttach = mParts[parentIdx].position + attIt->second.offset;
        mParts[childIdx].restLength = glm::length(mParts[childIdx].position - parentAttach);
    }
}

void RagdollModeSystem::update(float dt, const World& world, Player& player,
                                const InputState& input, const Camera& camera)
{
    if (!mActive) return;

    const auto& cfg = RagdollModeConfig::instance().data();
    mActivationTime += dt;

    // Step 0: Gravity + integration (ALL parts including torso)
    for (auto& part : mParts) {
        part.previousPosition = part.position;
        part.velocity.z -= 9.81f * cfg.gravityScale * dt;
        part.velocity *= (1.0f - cfg.linearDamping * dt);
        part.angularVelocity *= (1.0f - cfg.angularDamping * dt);

        float speed = glm::length(part.velocity);
        if (speed > MAX_PART_SPEED)
            part.velocity *= MAX_PART_SPEED / speed;

        if (glm::length(part.angularVelocity) > 0.0001f) {
            float angSpeed = glm::length(part.angularVelocity);
            glm::quat delta = glm::angleAxis(angSpeed * dt, glm::normalize(part.angularVelocity));
            part.rotation = glm::normalize(delta * part.rotation);
        }

        part.position += part.velocity * dt;
    }

    // Step 1: Process grab inputs
    processGrab(input, camera, world);

    // Step 2: Process extend inputs
    processExtend(input, camera);

    // Step 3: Apply grab spring forces (PBD-style: directly correct position)
    auto applyGrabCorrection = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0) return;
        RagdollModePart& part = mParts[grab.partIndex];
        glm::vec3 delta = grab.grabPoint - part.position;
        float dist = glm::length(delta);
        if (dist < 0.001f) return;
        glm::vec3 dir = delta / dist;
        float correction = dist * POSITION_CORRECTION;
        part.position += dir * correction;
        part.velocity += dir * correction / dt * 0.3f;
    };

    applyGrabCorrection(mLeftGrab);
    applyGrabCorrection(mRightGrab);

    // Step 4: PBD constraint solving
    solveConstraints(dt);

    // Step 5: World collision
    if (cfg.worldCollision)
        worldCollision(world);

    // Step 6: Sync torso position back to player
    syncToPlayer(player);

    // Structured log: RAGDOLL_TICK (every tick)
    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        float totalKE = 0.0f;
        for (const auto& part : mParts) {
            float spd = glm::length(part.velocity);
            totalKE += 0.5f * part.mass * spd * spd;
        }
        char msg[3072];
        int off = 0;
        off += snprintf(msg + off, sizeof(msg) - off,
            "[RAGDOLLDEBUG] RAGDOLL_TICK player=%s time=%.3f grounded=%d "
            "yaw=%.3f tick=%u grab_left=%d(grab_pt=(%.3f,%.3f,%.3f) hold=%.3f) "
            "grab_right=%d(grab_pt=(%.3f,%.3f,%.3f) hold=%.3f) kinetic_energy=%.3f ",
            player.username.c_str(), mActivationTime,
            (int)player.ground.onGround, player.yaw,
            (uint32_t)player.movementSimulationTick,
            (int)mLeftGrab.active,
            mLeftGrab.grabPoint.x, mLeftGrab.grabPoint.y, mLeftGrab.grabPoint.z,
            mActivationTime,
            (int)mRightGrab.active,
            mRightGrab.grabPoint.x, mRightGrab.grabPoint.y, mRightGrab.grabPoint.z,
            mActivationTime,
            totalKE);
        for (const auto& part : mParts) {
            off += snprintf(msg + off, sizeof(msg) - off,
                "part=%s pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) "
                "capsule_c=(%.3f,%.3f,%.3f) r=%.3f hh=%.3f ",
                part.name.c_str(),
                part.position.x, part.position.y, part.position.z,
                part.velocity.x, part.velocity.y, part.velocity.z,
                part.position.x, part.position.y, part.position.z,
                part.capsuleRadius, part.capsuleHalfHeight);
        }
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Ragdoll;
        e.level = StructuredLevel::Trace;
        e.eventId = "RAGDOLL_TICK";
        e.correlationId = player.username;
        e.reason = "Ragdoll mode tick";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.tick = (uint32_t)player.movementSimulationTick;
        e.message = msg;
        StructuredLogger::instance().write(e);
    }
}

void RagdollModeSystem::processGrab(const InputState& input, const Camera& camera, const World& world)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    bool leftHeld = input.grabLeftHeld;
    if (leftHeld && !mLeftGrab.wasActive && mLeftArmIndex >= 0) {
        RagdollModePart& arm = mParts[mLeftArmIndex];
        glm::vec3 rayDir = glm::normalize(camera.front);
        glm::vec3 hitPoint, hitNormal;
        if (raycastWorld(world, arm.position, rayDir, cfg.grabReach, hitPoint, hitNormal)) {
            mLeftGrab.active = true;
            mLeftGrab.grabPoint = hitPoint;
            mLeftGrab.grabNormal = hitNormal;
            mLeftGrab.handPosition = arm.position;
            mLeftGrab.partIndex = mLeftArmIndex;
            Debug::log(Debug::Category::Ragdoll,
                "[RAGDOLL MODE] Left grab at (%.2f, %.2f, %.2f)\n",
                hitPoint.x, hitPoint.y, hitPoint.z);
        }
    } else if (!leftHeld && mLeftGrab.wasActive) {
        mLeftGrab.active = false;
    }
    mLeftGrab.wasActive = mLeftGrab.active;

    bool rightHeld = input.grabRightHeld;
    if (rightHeld && !mRightGrab.wasActive && mRightArmIndex >= 0) {
        RagdollModePart& arm = mParts[mRightArmIndex];
        glm::vec3 rayDir = glm::normalize(camera.front);
        glm::vec3 hitPoint, hitNormal;
        if (raycastWorld(world, arm.position, rayDir, cfg.grabReach, hitPoint, hitNormal)) {
            mRightGrab.active = true;
            mRightGrab.grabPoint = hitPoint;
            mRightGrab.grabNormal = hitNormal;
            mRightGrab.handPosition = arm.position;
            mRightGrab.partIndex = mRightArmIndex;
            Debug::log(Debug::Category::Ragdoll,
                "[RAGDOLL MODE] Right grab at (%.2f, %.2f, %.2f)\n",
                hitPoint.x, hitPoint.y, hitPoint.z);
        }
    } else if (!rightHeld && mRightGrab.wasActive) {
        mRightGrab.active = false;
    }
    mRightGrab.wasActive = mRightGrab.active;
}

void RagdollModeSystem::processExtend(const InputState& input, const Camera& camera)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 forceDir = glm::normalize(camera.front);

    if (input.extendLeftMouse && mLeftArmIndex >= 0) {
        RagdollModePart& arm = mParts[mLeftArmIndex];
        arm.position += forceDir * cfg.extendForce * (1.0f / 60.0f) / arm.mass;
    }

    if (input.extendRightMouse && mRightArmIndex >= 0) {
        RagdollModePart& arm = mParts[mRightArmIndex];
        arm.position += forceDir * cfg.extendForce * (1.0f / 60.0f) / arm.mass;
    }
}

void RagdollModeSystem::solveConstraints(float dt)
{
    (void)dt;
    for (int iter = 0; iter < PBD_ITERATIONS; ++iter) {
        for (auto& part : mParts) {
            if (part.name == "torso") continue;
            if (part.parentIndex < 0) continue;

            RagdollModePart& parent = mParts[part.parentIndex];
            glm::vec3 parentAttachPoint = parent.position + part.parentAttachmentOffset;
            glm::vec3 delta = part.position - parentAttachPoint;
            float dist = glm::length(delta);
            if (dist < 0.0001f) continue;
            glm::vec3 dir = delta / dist;

            // PBD distance constraint: directly correct positions
            float error = dist - part.restLength;
            float totalInvMass = (1.0f / part.mass);
            if (part.name != "torso")
                totalInvMass += (1.0f / parent.mass);

            if (totalInvMass < 0.0001f) continue;

            glm::vec3 correction = dir * (error / totalInvMass) * POSITION_CORRECTION;

            part.position -= correction * (1.0f / part.mass);

            // Cone limit: project child back into parent's cone
            if (part.coneLimitDeg < 180.0f) {
                glm::vec3 newDelta = part.position - parentAttachPoint;
                float newDist = glm::length(newDelta);
                if (newDist > 0.001f) {
                    glm::vec3 newDir = newDelta / newDist;
                    glm::vec3 localDir = glm::inverse(parent.rotation) * newDir;
                    float coneRad = glm::radians(part.coneLimitDeg);
                    float angleFromY = std::acos(glm::clamp(localDir.y, -1.0f, 1.0f));
                    if (angleFromY > coneRad) {
                        float correctionAngle = angleFromY - coneRad;
                        glm::vec3 axis = glm::normalize(glm::cross(localDir, glm::vec3(0, 1, 0)));
                        if (glm::length(axis) < 0.001f) axis = glm::vec3(1, 0, 0);
                        glm::vec3 corrected = glm::mat3_cast(glm::angleAxis(-correctionAngle, axis)) * localDir;
                        part.position = parentAttachPoint + parent.rotation * corrected * newDist;
                    }
                }
            }
        }
    }

    // Derive velocities from position changes (PBD standard)
    float invDt = 1.0f / dt;
    for (auto& part : mParts) {
        part.velocity = (part.position - part.previousPosition) * invDt;
        float speed = glm::length(part.velocity);
        if (speed > MAX_PART_SPEED)
            part.velocity *= MAX_PART_SPEED / speed;
    }
}

void RagdollModeSystem::worldCollision(const World& world)
{
    std::vector<int> candidates;
    constexpr float TICK_DT = 1.0f / 60.0f;

    for (auto& part : mParts) {
        float r = part.capsuleRadius;
        float halfH = part.capsuleHalfHeight;
        float maxStep = std::max(r * 0.5f, 0.1f);
        int steps = std::min(
            (int)std::ceil(glm::length(part.velocity * TICK_DT) / maxStep) + 1, 8);
        glm::vec3 stepVel = part.velocity * TICK_DT / (float)steps;

        for (int s = 0; s < steps; ++s) {
            part.position += stepVel;

            AABB queryBounds;
            queryBounds.min = part.position - glm::vec3(r + 2.0f);
            queryBounds.max = part.position + glm::vec3(r + 2.0f);
            appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates, "ragdollModeCollision");

            for (int ti : candidates) {
                if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
                    continue;
                const CollisionTriangle& tri = world.collisionMesh.triangles[ti];

                Contact contact;
                if (sphereTriangleContact(part.position, r, tri, contact)) {
                    if (contact.penetration > 0.0f) {
                        part.position += contact.normal * contact.penetration;
                        float velDot = glm::dot(part.velocity, contact.normal);
                        if (velDot < 0.0f) {
                            part.velocity -= contact.normal * velDot * 1.15f;
                            part.angularVelocity *= 0.5f;
                        }
                    }
                }
            }
            candidates.clear();
        }
    }
}

void RagdollModeSystem::syncToPlayer(Player& player)
{
    if (mTorsoIndex >= 0 && mTorsoIndex < (int)mParts.size()) {
        mTorsoPosition = mParts[mTorsoIndex].position;
        player.pos = mTorsoPosition;
        player.vel = mParts[mTorsoIndex].velocity;
    }
}

glm::vec3 RagdollModeSystem::getHeadPosition() const
{
    if (mHeadIndex >= 0 && mHeadIndex < (int)mParts.size())
        return mParts[mHeadIndex].position;
    return mTorsoPosition + glm::vec3(0, 0, 1.15f);
}

glm::mat4 RagdollModeSystem::getHeadTransform() const
{
    if (mHeadIndex >= 0 && mHeadIndex < (int)mParts.size()) {
        const auto& head = mParts[mHeadIndex];
        return glm::translate(glm::mat4(1.0f), head.position)
             * glm::mat4_cast(head.rotation);
    }
    return glm::translate(glm::mat4(1.0f), mTorsoPosition + glm::vec3(0, 0, 1.15f));
}

void RagdollModeSystem::render(const Camera& camera) const
{
    if (!mActive) return;

    for (size_t i = 0; i < mParts.size(); ++i) {
        const auto& part = mParts[i];

        glm::vec4 color(0.2f, 0.8f, 1.0f, 0.9f);
        if (part.name == "head") color = glm::vec4(1.0f, 0.3f, 0.3f, 0.9f);
        else if (part.name == "torso") color = glm::vec4(0.2f, 0.8f, 1.0f, 0.9f);
        else if (part.name == "leftArm") color = glm::vec4(0.3f, 1.0f, 0.3f, 0.9f);
        else if (part.name == "rightArm") color = glm::vec4(0.3f, 0.3f, 1.0f, 0.9f);
        else if (part.name == "leftLeg") color = glm::vec4(1.0f, 1.0f, 0.3f, 0.9f);
        else if (part.name == "rightLeg") color = glm::vec4(1.0f, 0.3f, 1.0f, 0.9f);

        // Draw capsule using weapon line buffer (ungated, always visible like spyknife hitbox)
        Capsule cap;
        cap.a = part.position - glm::vec3(0, 0, part.capsuleHalfHeight);
        cap.b = part.position + glm::vec3(0, 0, part.capsuleHalfHeight);
        cap.r = part.capsuleRadius;
        DebugVis::drawWeaponCapsuleWire(camera, cap, color);

        // Draw attachment constraint lines
        if (part.parentIndex >= 0) {
            const auto& parent = mParts[part.parentIndex];
            glm::vec3 parentAttach = parent.position + part.parentAttachmentOffset;
            DebugVis::drawWeaponLine(camera, parentAttach, part.position,
                glm::vec4(1.0f, 1.0f, 0.0f, 0.5f));
        }

        // Draw grab indicators
        if (mLeftGrab.active && mLeftGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, mLeftGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.position, mLeftGrab.grabPoint,
                glm::vec4(0.0f, 1.0f, 0.0f, 0.9f));
        }
        if (mRightGrab.active && mRightGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, mRightGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.position, mRightGrab.grabPoint,
                glm::vec4(0.0f, 0.0f, 1.0f, 0.9f));
        }

        if (DebugConfig::DEBUG_RAGDOLL) {
            char label[128];
            snprintf(label, sizeof(label), "%s", part.name.c_str());
            DebugVis::drawWorldLabel(part.position + glm::vec3(0, 0, 0.3f),
                label, color);
        }
    }
}
