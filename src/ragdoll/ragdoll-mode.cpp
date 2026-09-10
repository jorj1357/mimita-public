#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
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

static constexpr float GRAVITY = 9.81f;

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

// Build an orientation quaternion from a look direction using the ragdoll
// convention: local +Y = forward, local +Z = up, local +X = right.
static glm::quat lookRotation(glm::vec3 forward, glm::vec3 up)
{
    forward = glm::normalize(forward);
    up = glm::normalize(up);

    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f));
    right = glm::normalize(right);

    glm::vec3 trueUp = glm::normalize(glm::cross(right, forward));
    glm::mat3 basis(right, forward, trueUp);
    return glm::normalize(glm::quat_cast(basis));
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
    mCameraSmoothInit = false;

    mLeftGrab = {};
    mRightGrab = {};

    initParts(player);

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Activated — %zu parts\n", mParts.size());

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        char msg[1024];
        snprintf(msg, sizeof(msg),
            "[RAGDOLLDEBUG] RAGDOLL_ENTER player=%s pos=(%.3f,%.3f,%.3f) "
            "config=config/ragdoll.json grounded=%d yaw=%.3f tick=%u parts=%zu",
            player.username.c_str(),
            player.pos.x, player.pos.y, player.pos.z,
            (int)player.ground.onGround,
            player.yaw,
            (uint32_t)player.movementSimulationTick,
            mParts.size());
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
        const RigidBody& torso = mParts[mTorsoIndex].body;
        player.pos = torso.position;
        if (RagdollModeConfig::instance().data().exitPreserveVelocity)
            player.vel = torso.linearVelocity;
        else
            player.vel = glm::vec3(0.0f);

        const auto& cfg = RagdollModeConfig::instance().data();
        player.vel.z += cfg.exitHopVelocity;
    }
    mActive = false;
    mParts.clear();
    mLeftGrab = {};
    mRightGrab = {};
    mCameraSmoothInit = false;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Deactivated\n");
}

void RagdollModeSystem::initParts(const Player& player)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    mParts.clear();

    struct PartDef { const char* name; float defaultMass; };
    const PartDef defs[] = {
        {"torso", 8.0f}, {"head", 2.0f},
        {"leftArm", 1.5f}, {"rightArm", 1.5f},
        {"leftLeg", 2.5f}, {"rightLeg", 2.5f},
    };

    glm::quat bindRot = glm::angleAxis(glm::radians(player.yaw), glm::vec3(0, 0, 1));

    for (const auto& d : defs) {
        RagdollModePart part;
        part.name = d.name;

        auto capIt = cfg.capsules.find(d.name);
        if (capIt != cfg.capsules.end()) {
            part.body.capsuleRadius = capIt->second.radius;
            part.body.capsuleHalfHeight = capIt->second.halfHeight;
        }
        part.body.localAxis = glm::vec3(0.0f, 0.0f, 1.0f);

        float massKg = d.defaultMass;
        auto massIt = cfg.massKg.find(d.name);
        if (massIt != cfg.massKg.end()) massKg = massIt->second;
        massKg *= cfg.massGlobalMultiplier;
        part.body.staticBody = false;
        setBodyMass(part.body, massKg);

        part.body.linearDamping = cfg.bodyLinearDamping;
        part.body.angularDamping = cfg.bodyAngularDamping;
        part.body.restitution = cfg.restitution;
        part.body.friction = cfg.friction;
        part.body.maxLinearSpeed = cfg.maxFallSpeed;
        part.body.maxAngularSpeed = 25.0f;

        if (d.name == std::string("torso")) {
            part.body.position = mTorsoPosition;
        } else {
            auto capIt2 = cfg.capsules.find(d.name);
            glm::vec3 offset = (capIt2 != cfg.capsules.end()) ? capIt2->second.offset : glm::vec3(0.0f);
            part.body.position = mTorsoPosition + glm::vec3(bindRot * glm::vec4(offset, 0.0f));
        }
        part.body.orientation = bindRot;
        part.body.linearVelocity = player.vel;

        int idx = (int)mParts.size();
        mParts.push_back(std::move(part));

        if (d.name == std::string("torso")) mTorsoIndex = idx;
        else if (d.name == std::string("head")) mHeadIndex = idx;
        else if (d.name == std::string("leftArm")) mLeftArmIndex = idx;
        else if (d.name == std::string("rightArm")) mRightArmIndex = idx;
        else if (d.name == std::string("leftLeg")) mLeftLegIndex = idx;
        else if (d.name == std::string("rightLeg")) mRightLegIndex = idx;
    }

    const char* attachNames[] = {"head", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    for (const char* name : attachNames) {
        int childIdx = findPartByName(mParts, name);
        if (childIdx < 0) continue;
        auto attIt = cfg.attachments.find(name);
        if (attIt == cfg.attachments.end()) continue;
        int parentIdx = findPartByName(mParts, attIt->second.parent);
        if (parentIdx < 0) continue;

        RagdollModePart& child = mParts[childIdx];
        RagdollModePart& parent = mParts[parentIdx];

        child.parentIndex = parentIdx;
        child.parentLocalAnchor = attIt->second.offset;
        child.coneLimitDeg = attIt->second.coneLimitDeg;
        child.hasRotationLimits = attIt->second.hasRotationLimits;
        child.rotMinDeg = attIt->second.rotMinDeg;
        child.rotMaxDeg = attIt->second.rotMaxDeg;
        child.bindRelativeRotation = glm::normalize(
            glm::inverse(parent.body.orientation) * child.body.orientation);

        glm::vec3 parentWorldAnchor = parent.body.position
            + parent.body.orientation * child.parentLocalAnchor;
        child.childLocalAnchor = glm::inverse(child.body.orientation)
            * (parentWorldAnchor - child.body.position);

        glm::vec3 dir = child.body.position - parentWorldAnchor;
        child.restLength = glm::length(dir);
        if (child.restLength > 1e-5f)
            child.restDirectionLocal = glm::normalize(
                glm::inverse(parent.body.orientation) * dir);
        else
            child.restDirectionLocal = glm::vec3(0.0f, 0.0f, -1.0f);
    }
}

void RagdollModeSystem::update(float dt, const World& world, Player& player,
                                const InputState& input, const Camera& camera)
{
    if (!mActive) return;

    const auto& cfg = RagdollModeConfig::instance().data();
    mActivationTime += dt;

    // Step 1: Inputs and physical controls (head aim, arm extension, grabs).
    applyControls(dt, input, camera);
    processGrab(input, camera, world);
    processExtend(input, camera, dt);

    // Step 2: Integrate gravity and motion for every body part.
    glm::vec3 gravity(0.0f, 0.0f, -GRAVITY * cfg.gravityScale);
    for (auto& part : mParts)
        integrate(part.body, gravity, dt);

    // Step 3: Joints (velocity + position), angular limits, and cone limits.
    solveJoints(cfg.solverIterations, true);

    // Step 4: Grab constraints (velocity + position).
    solveGrabs(cfg.solverIterations);

    // Step 5: World collision (swept, substepped).
    if (cfg.worldCollision) {
        for (auto& part : mParts)
            collideWithWorld(part.body, world, dt);
    }

    // Step 6: Self collision.
    if (cfg.selfCollision)
        selfCollision();

    // Step 7: Re-converge constraints after collision so links stay rigid.
    solveJoints(cfg.solverIterations / 2, true);
    solveGrabs(cfg.solverIterations / 2);

    // Step 8: Final overlap cleanup so constraints cannot leave a part buried.
    if (cfg.worldCollision) {
        for (auto& part : mParts)
            depenetrateWorld(part.body, world, 2);
    }

    // Step 9: Write the authoritative root and skeleton transforms.
    syncToPlayer(player);

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        float totalKE = 0.0f;
        for (const auto& part : mParts) {
            float spd = glm::length(part.body.linearVelocity);
            totalKE += 0.5f * part.body.mass * spd * spd;
        }
        char msg[1024];
        snprintf(msg, sizeof(msg),
            "[RAGDOLLDEBUG] RAGDOLL_TICK player=%s time=%.3f grounded=%d "
            "yaw=%.3f tick=%u grab_left=%d grab_right=%d kinetic_energy=%.3f parts=%zu",
            player.username.c_str(), mActivationTime,
            (int)player.ground.onGround, player.yaw,
            (uint32_t)player.movementSimulationTick,
            (int)mLeftGrab.active, (int)mRightGrab.active, totalKE, mParts.size());
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

void RagdollModeSystem::applyControls(float dt, const InputState& input, const Camera& camera)
{
    (void)input;
    const auto& cfg = RagdollModeConfig::instance().data();

    // Head physically rotates toward the camera look direction.
    if (mHeadIndex >= 0 && mHeadIndex < (int)mParts.size()) {
        RigidBody& head = mParts[mHeadIndex].body;
        glm::quat target = lookRotation(camera.front, camera.up);
        glm::quat diff = glm::normalize(target * glm::inverse(head.orientation));
        float w = glm::clamp(diff.w, -1.0f, 1.0f);
        float angle = 2.0f * std::acos(std::fabs(w));
        float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
        glm::vec3 axis = (s > 1e-4f) ? glm::vec3(diff.x, diff.y, diff.z) / s
                                     : glm::vec3(0.0f, 0.0f, 1.0f);
        if (w < 0.0f) axis = -axis;

        head.angularVelocity += axis * (angle * cfg.headRotationStrength) * dt;
        float spd = glm::length(head.angularVelocity);
        if (spd > cfg.headRotationSpeed)
            head.angularVelocity *= cfg.headRotationSpeed / spd;
    }
}

static glm::vec3 quatToRotationVector(const glm::quat& q)
{
    glm::quat n = glm::normalize(q);
    float w = glm::clamp(n.w, -1.0f, 1.0f);
    float angle = 2.0f * std::acos(w);
    float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
    if (s < 1e-5f) return glm::vec3(0.0f);
    glm::vec3 axis(n.x, n.y, n.z);
    axis /= s;
    if (angle > 3.14159265f) angle -= 2.0f * 3.14159265f;
    return axis * angle;
}

void RagdollModeSystem::solveJoints(int iterations, bool positionPass)
{
    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& part : mParts) {
            if (part.parentIndex < 0) continue;
            if (part.parentIndex >= (int)mParts.size()) continue;

            RigidBody& child = part.body;
            RigidBody& parent = mParts[part.parentIndex].body;

            glm::vec3 parentAnchor = parent.position
                + parent.orientation * part.parentLocalAnchor;
            glm::vec3 childAnchor = child.position
                + child.orientation * part.childLocalAnchor;

            solvePointJointVelocity(parent, parentAnchor, child, childAnchor);

            if (positionPass)
                solvePointJointPosition(parent, parentAnchor, child, childAnchor, 0.9f);
        }

        if (positionPass) {
            solveRotationLimits();
            solveConeLimits();
        }
    }
}

void RagdollModeSystem::solveRotationLimits()
{
    constexpr float kBeta = 0.5f;

    for (auto& part : mParts) {
        if (!part.hasRotationLimits) continue;
        if (part.parentIndex < 0 || part.parentIndex >= (int)mParts.size()) continue;

        RigidBody& child = part.body;
        RigidBody& parent = mParts[part.parentIndex].body;

        glm::quat currentRelative = glm::normalize(
            glm::inverse(parent.orientation) * child.orientation);
        glm::quat delta = glm::normalize(
            currentRelative * glm::inverse(part.bindRelativeRotation));

        glm::vec3 rv = quatToRotationVector(delta);
        glm::vec3 clamped(
            glm::clamp(rv.x, part.rotMinDeg.x, part.rotMaxDeg.x),
            glm::clamp(rv.y, part.rotMinDeg.y, part.rotMaxDeg.y),
            glm::clamp(rv.z, part.rotMinDeg.z, part.rotMaxDeg.z));

        glm::vec3 rejected = rv - clamped;
        if (glm::length(rejected) < 1e-6f) continue;

        float invA = child.invInertia;
        float invB = parent.invInertia;
        float total = invA + invB;
        if (total < 1e-8f) continue;

        glm::vec3 worldCorrection = parent.orientation * (-rejected);
        rotateBody(child,  worldCorrection * (kBeta * invA / total));
        rotateBody(parent, -worldCorrection * (kBeta * invB / total));
    }
}

void RagdollModeSystem::solveConeLimits()
{
    for (auto& part : mParts) {
        if (part.parentIndex < 0 || part.parentIndex >= (int)mParts.size()) continue;
        if (part.coneLimitDeg >= 180.0f) continue;

        RigidBody& child = part.body;
        RigidBody& parent = mParts[part.parentIndex].body;

        glm::vec3 parentAnchor = parent.position
            + parent.orientation * part.parentLocalAnchor;
        glm::vec3 dir = child.position - parentAnchor;
        float len = glm::length(dir);
        if (len < 1e-5f) continue;

        glm::vec3 dirWorld = dir / len;
        glm::vec3 dirLocal = glm::inverse(parent.orientation) * dirWorld;

        float cosA = glm::clamp(glm::dot(dirLocal, part.restDirectionLocal), -1.0f, 1.0f);
        float angle = std::acos(cosA);
        float limit = glm::radians(part.coneLimitDeg);
        if (angle <= limit) continue;

        glm::vec3 axis = glm::cross(dirLocal, part.restDirectionLocal);
        float axisLen = glm::length(axis);
        axis = (axisLen > 1e-5f) ? axis / axisLen : glm::vec3(1.0f, 0.0f, 0.0f);

        float excess = angle - limit;
        glm::vec3 corrected = glm::normalize(glm::angleAxis(excess, axis) * dirLocal);
        glm::vec3 newWorld = parent.orientation * corrected;
        child.position = parentAnchor + newWorld * len;
    }
}

void RagdollModeSystem::solveGrabs(int iterations)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    float beta = glm::clamp(0.9f - cfg.grabCompliance * 20.0f, 0.1f, 0.95f);

    auto solve = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0
            || grab.partIndex >= (int)mParts.size()) return;
        RigidBody& body = mParts[grab.partIndex].body;
        for (int i = 0; i < iterations; ++i) {
            glm::vec3 hand = body.position + body.orientation * grab.handLocalAnchor;
            // Cancel the anchor velocity first so gravity cannot accumulate,
            // then project the position so the grip stays effectively rigid.
            solvePointToWorldVelocity(body, hand);
            solvePointToWorld(body, hand, grab.grabPoint, beta);
        }
    };

    solve(mLeftGrab);
    solve(mRightGrab);
}

void RagdollModeSystem::processGrab(const InputState& input, const Camera& camera, const World& world)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    auto tryGrab = [&](RagdollGrabState& grab, int armIndex) {
        if (armIndex < 0 || armIndex >= (int)mParts.size()) return;
        RagdollModePart& arm = mParts[armIndex];
        glm::vec3 handLocal = -(arm.body.localAxis) * arm.body.capsuleHalfHeight;
        glm::vec3 handWorld = arm.body.position + arm.body.orientation * handLocal;
        glm::vec3 rayDir = glm::normalize(camera.front);
        glm::vec3 hitPoint, hitNormal;
        if (raycastWorld(world, handWorld, rayDir, cfg.grabReach, hitPoint, hitNormal)) {
            float distSq = glm::dot(hitPoint - handWorld, hitPoint - handWorld);
            float grace = cfg.grabGraceDistance + arm.body.capsuleRadius;
            (void)distSq;
            (void)grace;
            grab.active = true;
            grab.grabPoint = hitPoint;
            grab.grabNormal = hitNormal;
            grab.handPosition = handWorld;
            grab.handLocalAnchor = handLocal;
            grab.partIndex = armIndex;
        }
    };

    bool leftHeld = input.grabLeftHeld;
    if (leftHeld && !mLeftGrab.wasActive)
        tryGrab(mLeftGrab, mLeftArmIndex);
    else if (!leftHeld && mLeftGrab.wasActive)
        mLeftGrab.active = false;
    mLeftGrab.wasActive = mLeftGrab.active;

    bool rightHeld = input.grabRightHeld;
    if (rightHeld && !mRightGrab.wasActive)
        tryGrab(mRightGrab, mRightArmIndex);
    else if (!rightHeld && mRightGrab.wasActive)
        mRightGrab.active = false;
    mRightGrab.wasActive = mRightGrab.active;
}

void RagdollModeSystem::processExtend(const InputState& input, const Camera& camera, float dt)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 targetHandDir = glm::normalize(camera.front);

    // Steer the arm's long axis so its hand points along camera-forward. This is
    // an internal angular motor at the shoulder: it changes only the arm's
    // angular velocity, so it cannot add net linear momentum and cannot pull the
    // player forward. The shoulder joint transmits the reaction to the torso.
    auto extend = [&](float dt, int armIndex) {
        if (armIndex < 0 || armIndex >= (int)mParts.size()) return;
        RigidBody& arm = mParts[armIndex].body;

        glm::vec3 currentHandDir = arm.orientation * (-arm.localAxis);
        float currentLen = glm::length(currentHandDir);
        if (currentLen < 1e-5f) return;
        currentHandDir /= currentLen;

        float dotp = glm::clamp(glm::dot(currentHandDir, targetHandDir), -1.0f, 1.0f);
        glm::vec3 axis = glm::cross(currentHandDir, targetHandDir);
        float axisLen = glm::length(axis);
        if (axisLen < 1e-5f) return;
        axis /= axisLen;

        float angle = std::acos(dotp);
        float desiredSpeed = glm::clamp(angle * cfg.armExtendStrength,
                                        -cfg.armExtendMaxSpeed, cfg.armExtendMaxSpeed);
        float current = glm::dot(arm.angularVelocity, axis);
        float blend = glm::clamp(dt * 10.0f, 0.0f, 1.0f);
        arm.angularVelocity += axis * ((desiredSpeed - current) * blend);
    };

    if (input.extendLeftMouse) extend(dt, mLeftArmIndex);
    if (input.extendRightMouse) extend(dt, mRightArmIndex);
}

void RagdollModeSystem::selfCollision()
{
    for (int i = 0; i < (int)mParts.size(); ++i) {
        for (int j = i + 1; j < (int)mParts.size(); ++j) {
            if (mParts[i].parentIndex == j || mParts[j].parentIndex == i)
                continue;
            collideBodies(mParts[i].body, mParts[j].body);
        }
    }
}

void RagdollModeSystem::syncToPlayer(Player& player)
{
    if (mTorsoIndex < 0 || mTorsoIndex >= (int)mParts.size()) return;

    const RigidBody& torso = mParts[mTorsoIndex].body;
    mTorsoPosition = torso.position;
    player.pos = mTorsoPosition;
    player.vel = torso.linearVelocity;

    glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), player.pos)
                        * glm::mat4_cast(torso.orientation);

    for (size_t i = 0; i < mParts.size(); ++i) {
        const auto& part = mParts[i];

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

        glm::mat4 childWorld = glm::translate(glm::mat4(1.0f), part.body.position)
                             * glm::mat4_cast(part.body.orientation);

        glm::mat4 parentWorld;
        if (part.parentIndex >= 0 && part.parentIndex < (int)mParts.size()) {
            const RigidBody& parent = mParts[part.parentIndex].body;
            parentWorld = glm::translate(glm::mat4(1.0f), parent.position)
                        * glm::mat4_cast(parent.orientation);
        } else {
            parentWorld = rootWorld;
        }

        player.perfectPoseSkeleton.nodes[nodeIndex].localTransform =
            glm::inverse(parentWorld) * childWorld;
    }
}

glm::vec3 RagdollModeSystem::getHeadPosition() const
{
    if (mHeadIndex >= 0 && mHeadIndex < (int)mParts.size())
        return mParts[mHeadIndex].body.position;
    return mTorsoPosition + glm::vec3(0, 0, 1.15f);
}

glm::mat4 RagdollModeSystem::getHeadTransform() const
{
    if (mHeadIndex >= 0 && mHeadIndex < (int)mParts.size()) {
        const RigidBody& head = mParts[mHeadIndex].body;
        return glm::translate(glm::mat4(1.0f), head.position)
             * glm::mat4_cast(head.orientation);
    }
    return glm::translate(glm::mat4(1.0f), mTorsoPosition + glm::vec3(0, 0, 1.15f));
}

glm::vec3 RagdollModeSystem::computeCameraPosition(float dt)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 headPos = getHeadPosition();

    if (!mCameraSmoothInit) {
        mCameraSmoothPos = headPos;
        mCameraSmoothInit = true;
        return mCameraSmoothPos;
    }

    if (cfg.cameraSmoothFactor <= 0.001f) {
        mCameraSmoothPos = headPos;
    } else {
        float smoothTime = cfg.cameraSmoothFactor * 0.08f;
        float alpha = dt / (dt + smoothTime);
        alpha = glm::clamp(alpha, 0.0f, 1.0f);
        mCameraSmoothPos += (headPos - mCameraSmoothPos) * alpha;
    }
    return mCameraSmoothPos;
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

        Capsule cap = capsuleOf(part.body);
        DebugVis::drawWeaponCapsuleWire(camera, cap, color);

        if (part.parentIndex >= 0) {
            const RigidBody& parent = mParts[part.parentIndex].body;
            glm::vec3 parentAttach = parent.position
                + parent.orientation * part.parentLocalAnchor;
            DebugVis::drawWeaponLine(camera, parentAttach, part.body.position,
                glm::vec4(1.0f, 1.0f, 0.0f, 0.5f));
        }

        if (mLeftGrab.active && mLeftGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, mLeftGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, mLeftGrab.grabPoint,
                glm::vec4(0.0f, 1.0f, 0.0f, 0.9f));
        }
        if (mRightGrab.active && mRightGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, mRightGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, mRightGrab.grabPoint,
                glm::vec4(0.0f, 0.0f, 1.0f, 0.9f));
        }

        if (DebugConfig::DEBUG_RAGDOLL) {
            char label[128];
            snprintf(label, sizeof(label), "%s", part.name.c_str());
            DebugVis::drawWorldLabel(part.body.position + glm::vec3(0, 0, 0.3f),
                label, color);
        }
    }
}
