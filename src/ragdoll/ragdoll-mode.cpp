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

static constexpr int PBD_ITERATIONS = 3;
static constexpr float MAX_PART_SPEED = 30.0f;
static constexpr float POSITION_CORRECTION = 0.5f;

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

static float wrapDegrees(float a)
{
    a = std::fmod(a + 180.0f, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a - 180.0f;
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
                "capsule_c=(%.3f,%.3f,%.3f) r=%.3f hh=%.3f ",
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

    struct PartDef { const char* name; float mass; };
    const PartDef defs[] = {
        {"torso", 5.0f}, {"head", 2.0f},
        {"leftArm", 1.5f}, {"rightArm", 1.5f},
        {"leftLeg", 2.0f}, {"rightLeg", 2.0f},
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

    // Step 0: Gravity + integration (ALL parts)
    for (auto& part : mParts) {
        part.previousPosition = part.position;
        part.velocity.z -= 9.81f * cfg.gravityScale * dt;
        part.velocity *= (1.0f - cfg.linearDamping * dt);
        part.angularVelocity *= (1.0f - cfg.angularDamping * dt);

        float speed = glm::length(part.velocity);
        if (speed > MAX_PART_SPEED) part.velocity *= MAX_PART_SPEED / speed;

        if (glm::length(part.angularVelocity) > 0.0001f) {
            float angSpeed = glm::length(part.angularVelocity);
            glm::quat delta = glm::angleAxis(angSpeed * dt, glm::normalize(part.angularVelocity));
            part.rotation = glm::normalize(delta * part.rotation);
        }

        part.position += part.velocity * dt;
    }

    // Step 0b: Torso follows look direction (spring-smoothed yaw)
    {
        float targetYaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
        glm::vec3 torsoFwd = mTorsoRotation * glm::vec3(0, 1, 0);
        float currentYaw = glm::degrees(std::atan2(torsoFwd.y, torsoFwd.x));
        float yawError = wrapDegrees(targetYaw - currentYaw);
        float maxStep = cfg.torsoMaxAngularStep;
        float step = glm::clamp(yawError * cfg.torsoLookSpring * dt, -maxStep, maxStep);
        mTorsoRotation = glm::angleAxis(glm::radians(step), glm::vec3(0, 0, 1)) * mTorsoRotation;
        if (mTorsoIndex >= 0 && mTorsoIndex < (int)mParts.size())
            mParts[mTorsoIndex].rotation = mTorsoRotation;
    }

    // Step 1: Process grab inputs
    processGrab(input, camera, world);

    // Step 2: Process extend inputs
    processExtend(input, camera);

    // Step 3: Pin grabbed hands to grab point
    auto pinGrab = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0) return;
        mParts[grab.partIndex].position = grab.grabPoint;
        mParts[grab.partIndex].velocity = glm::vec3(0.0f);
    };
    pinGrab(mLeftGrab);
    pinGrab(mRightGrab);

    // Step 4: World collision (BEFORE PBD — no movement application, only depenetration)
    if (cfg.worldCollision)
        worldCollision(world);

    // Step 5: PBD constraint solving
    solveConstraints(dt);

    // Step 6: Sync torso back to player
    syncToPlayer(player);

    // Structured log: RAGDOLL_TICK
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
            "yaw=%.3f tick=%u grab_left=%d grab_right=%d kinetic_energy=%.3f ",
            player.username.c_str(), mActivationTime,
            (int)player.ground.onGround, player.yaw,
            (uint32_t)player.movementSimulationTick,
            (int)mLeftGrab.active, (int)mRightGrab.active, totalKE);
        for (const auto& part : mParts) {
            off += snprintf(msg + off, sizeof(msg) - off,
                "part=%s pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) r=%.3f hh=%.3f ",
                part.name.c_str(),
                part.position.x, part.position.y, part.position.z,
                part.velocity.x, part.velocity.y, part.velocity.z,
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
        mParts[mLeftArmIndex].velocity += forceDir * cfg.extendForce / mParts[mLeftArmIndex].mass;
    }
    if (input.extendRightMouse && mRightArmIndex >= 0) {
        mParts[mRightArmIndex].velocity += forceDir * cfg.extendForce / mParts[mRightArmIndex].mass;
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

            float error = dist - part.restLength;
            float totalInvMass = (1.0f / part.mass);
            if (part.name != "torso")
                totalInvMass += (1.0f / parent.mass);
            if (totalInvMass < 0.0001f) continue;

            glm::vec3 correction = dir * (error / totalInvMass) * POSITION_CORRECTION;
            part.position -= correction * (1.0f / part.mass);

            // Cone limit
            if (part.coneLimitDeg < 180.0f) {
                glm::vec3 newDelta = part.position - parentAttachPoint;
                float newDist = glm::length(newDelta);
                if (newDist > 0.001f) {
                    glm::vec3 newDir = newDelta / newDist;
                    glm::vec3 localDir = glm::inverse(parent.rotation) * newDir;
                    float coneRad = glm::radians(part.coneLimitDeg);
                    float angleFromY = std::acos(glm::clamp(localDir.y, -1.0f, 1.0f));
                    if (angleFromY > coneRad) {
                        float corr = angleFromY - coneRad;
                        glm::vec3 axis = glm::normalize(glm::cross(localDir, glm::vec3(0, 1, 0)));
                        if (glm::length(axis) < 0.001f) axis = glm::vec3(1, 0, 0);
                        glm::vec3 corrected = glm::mat3_cast(glm::angleAxis(-corr, axis)) * localDir;
                        part.position = parentAttachPoint + parent.rotation * corrected * newDist;
                    }
                }
            }
        }
    }

    // Phase 5: If a hand is pinned (grabbed) and its distance from the torso
    // attachment exceeds rest length, pull the torso toward the grab point.
    if (mTorsoIndex >= 0 && mTorsoIndex < (int)mParts.size()) {
        auto pullTorsoForGrab = [&](const RagdollGrabState& grab) {
            if (!grab.active || grab.partIndex < 0) return;
            if (grab.partIndex == mTorsoIndex) return;

            const auto& handPart = mParts[grab.partIndex];
            // Find the attachment offset for this hand on the torso
            glm::vec3 attachOffset{0.0f};
            for (const auto& p : mParts) {
                if (p.parentIndex == mTorsoIndex && p.name == handPart.name) {
                    attachOffset = p.parentAttachmentOffset;
                    break;
                }
            }

            glm::vec3 attachWorld = mParts[mTorsoIndex].position
                + mParts[mTorsoIndex].rotation * attachOffset;
            glm::vec3 toGrab = grab.grabPoint - attachWorld;
            float dist = glm::length(toGrab);
            if (dist > 0.5f) {
                glm::vec3 correction = toGrab * (dist - 0.5f) / dist * 0.3f;
                mParts[mTorsoIndex].position += correction;
            }
        };
        pullTorsoForGrab(mLeftGrab);
        pullTorsoForGrab(mRightGrab);
    }

    // Derive velocities from position changes (PBD standard)
    float invDt = 1.0f / dt;
    for (auto& part : mParts) {
        part.velocity = (part.position - part.previousPosition) * invDt;
        float speed = glm::length(part.velocity);
        if (speed > MAX_PART_SPEED) part.velocity *= MAX_PART_SPEED / speed;
    }
}

void RagdollModeSystem::worldCollision(const World& world)
{
    std::vector<int> candidates;

    for (auto& part : mParts) {
        float r = part.capsuleRadius;
        float halfH = part.capsuleHalfHeight;

        // 3 sphere samples along the capsule's rotated local axis
        glm::vec3 localUp = part.rotation * glm::vec3(0, 0, halfH);
        glm::vec3 samples[3] = {
            part.position,
            part.position + localUp,
            part.position - localUp
        };

        for (int s = 0; s < 3; ++s) {
            glm::vec3& sample = samples[s];

            AABB queryBounds;
            queryBounds.min = sample - glm::vec3(r + 2.0f);
            queryBounds.max = sample + glm::vec3(r + 2.0f);
            appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates, "ragdollModeCollision");

            for (int ti : candidates) {
                if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
                    continue;
                const CollisionTriangle& tri = world.collisionMesh.triangles[ti];

                Contact contact;
                if (sphereTriangleContact(sample, r, tri, contact)) {
                    if (contact.penetration > 0.0f) {
                        // Depenetrate the whole part from this sample's contact
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
    if (mTorsoIndex < 0 || mTorsoIndex >= (int)mParts.size()) return;

    mTorsoPosition = mParts[mTorsoIndex].position;
    player.pos = mTorsoPosition;
    player.vel = mParts[mTorsoIndex].velocity;

    // Bridge ragdoll transforms → skeleton → renderer
    // The renderer reads physicalBody.parts[i].worldTransform, which comes from
    // perfectPoseSkeleton.nodes[i].localTransform via updateModelWorldTransforms().
    // We write local transforms here so the player model visually follows the ragdoll.

    glm::mat4 rootWorld = transformMatrix(player.pos, mTorsoRotation);

    for (size_t i = 0; i < mParts.size(); ++i) {
        const auto& part = mParts[i];

        // Find matching physicalBody part by name
        int bodyPartIdx = -1;
        for (int j = 0; j < (int)player.physicalBody.parts.size(); ++j) {
            if (player.physicalBody.parts[j].name == part.name) {
                bodyPartIdx = j;
                break;
            }
        }
        if (bodyPartIdx < 0) continue;

        int nodeIndex = player.physicalBody.parts[bodyPartIdx].nodeIndex;
        if (nodeIndex < 0 || nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size())
            continue;

        // Compute child world matrix from ragdoll position/rotation
        glm::mat4 childWorld = glm::translate(glm::mat4(1.0f), part.position)
                             * glm::mat4_cast(part.rotation);

        // Compute parent world matrix
        glm::mat4 parentWorld;
        if (part.parentIndex >= 0 && part.parentIndex < (int)mParts.size()) {
            const auto& parent = mParts[part.parentIndex];
            parentWorld = glm::translate(glm::mat4(1.0f), parent.position)
                        * glm::mat4_cast(parent.rotation);
        } else {
            // Root (torso) — parent is the player root
            parentWorld = rootWorld;
        }

        // Write local transform: parent⁻¹ * child
        player.perfectPoseSkeleton.nodes[nodeIndex].localTransform =
            glm::inverse(parentWorld) * childWorld;
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

        // Capsule aligned to limb rotation
        glm::vec3 localA = part.rotation * glm::vec3(0, 0, -part.capsuleHalfHeight);
        glm::vec3 localB = part.rotation * glm::vec3(0, 0,  part.capsuleHalfHeight);
        Capsule cap;
        cap.a = part.position + localA;
        cap.b = part.position + localB;
        cap.r = part.capsuleRadius;
        DebugVis::drawWeaponCapsuleWire(camera, cap, color);

        // Attachment constraint lines
        if (part.parentIndex >= 0) {
            const auto& parent = mParts[part.parentIndex];
            glm::vec3 parentAttach = parent.position + part.parentAttachmentOffset;
            DebugVis::drawWeaponLine(camera, parentAttach, part.position,
                glm::vec4(1.0f, 1.0f, 0.0f, 0.5f));
        }

        // Grab indicators
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
