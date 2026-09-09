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

extern Renderer* gRenderer;
extern TextureStore gTextures;

static constexpr int CONSTRAINT_ITERATIONS = 5;
static constexpr float GRAB_SPRING = 5000.0f;
static constexpr float GRAB_DAMPING = 200.0f;
static constexpr float MAX_PART_SPEED = 30.0f;

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
}

void RagdollModeSystem::deactivate(Player& player)
{
    mActive = false;
    mParts.clear();
    mLeftGrab = {};
    mRightGrab = {};
    (void)player;

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

    // Set up attachment constraints
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

    // Step 0: Gravity + integration
    for (auto& part : mParts) {
        if (part.name == "torso") {
            // Torso follows player position (player movement still applies)
            part.position = player.pos;
            part.velocity = player.vel;
            continue;
        }

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

    // Step 3: Apply grab spring forces
    auto applyGrabForce = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0) return;
        RagdollModePart& part = mParts[grab.partIndex];
        glm::vec3 toGrab = grab.grabPoint - part.position;
        float dist = glm::length(toGrab);
        if (dist < 0.001f) return;
        glm::vec3 dir = toGrab / dist;
        glm::vec3 relVel = part.velocity;
        float springForce = dist * GRAB_SPRING;
        float dampingForce = glm::dot(relVel, dir) * GRAB_DAMPING;
        part.velocity += (dir * (springForce - dampingForce) / part.mass) * dt;
    };

    applyGrabForce(mLeftGrab);
    applyGrabForce(mRightGrab);

    // Step 4: Joint constraint solving
    solveConstraints(dt);

    // Step 5: World collision
    if (cfg.worldCollision)
        worldCollision(world);

    // Step 6: Sync torso position back to player
    syncToPlayer(player);
}

void RagdollModeSystem::processGrab(const InputState& input, const Camera& camera, const World& world)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    // Left grab (A key)
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
        if (mLeftGrab.partIndex >= 0 && mLeftGrab.partIndex < (int)mParts.size()) {
            mParts[mLeftGrab.partIndex].velocity *= 0.5f;
        }
    }
    mLeftGrab.wasActive = mLeftGrab.active;

    // Right grab (D key)
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
        if (mRightGrab.partIndex >= 0 && mRightGrab.partIndex < (int)mParts.size()) {
            mParts[mRightGrab.partIndex].velocity *= 0.5f;
        }
    }
    mRightGrab.wasActive = mRightGrab.active;
}

void RagdollModeSystem::processExtend(const InputState& input, const Camera& camera)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 forceDir = glm::normalize(camera.front);

    if (input.extendLeftMouse && mLeftArmIndex >= 0) {
        RagdollModePart& arm = mParts[mLeftArmIndex];
        arm.velocity += forceDir * cfg.extendForce / arm.mass;
    }

    if (input.extendRightMouse && mRightArmIndex >= 0) {
        RagdollModePart& arm = mParts[mRightArmIndex];
        arm.velocity += forceDir * cfg.extendForce / arm.mass;
    }
}

void RagdollModeSystem::solveConstraints(float dt)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    for (int iter = 0; iter < CONSTRAINT_ITERATIONS; ++iter) {
        for (auto& part : mParts) {
            if (part.name == "torso") continue;
            if (part.parentIndex < 0) continue;

            RagdollModePart& parent = mParts[part.parentIndex];
            glm::vec3 parentAttachPoint = parent.position + part.parentAttachmentOffset;
            glm::vec3 delta = part.position - parentAttachPoint;
            float dist = glm::length(delta);
            if (dist < 0.0001f) continue;
            glm::vec3 dir = delta / dist;

            // Distance constraint (spring)
            float displacement = dist - part.restLength;
            float springForce = displacement * cfg.jointStiffness;
            glm::vec3 relVel = part.velocity - parent.velocity;
            float dampingForce = glm::dot(relVel, dir) * cfg.jointDamping;
            float totalForce = springForce + dampingForce;

            // Cone limit: project child back into parent's cone
            if (part.coneLimitDeg < 180.0f && dist > 0.001f) {
                glm::vec3 localDir = glm::inverse(parent.rotation) * dir;
                float coneRad = glm::radians(part.coneLimitDeg);
                float angleFromY = std::acos(glm::clamp(localDir.y, -1.0f, 1.0f));
                if (angleFromY > coneRad) {
                    // Project back to cone surface
                    float correction = angleFromY - coneRad;
                    glm::vec3 axis = glm::normalize(glm::cross(localDir, glm::vec3(0, 1, 0)));
                    if (glm::length(axis) < 0.001f) axis = glm::vec3(1, 0, 0);
                    glm::vec3 corrected = glm::mat3_cast(glm::angleAxis(-correction, axis)) * localDir;
                    part.position = parentAttachPoint + parent.rotation * corrected * dist;
                    delta = part.position - parentAttachPoint;
                    dir = glm::normalize(delta);
                }
            }

            float invMassA = 1.0f / parent.mass;
            float invMassB = 1.0f / part.mass;
            float totalInvMass = invMassA + invMassB;

            glm::vec3 impulse = dir * totalForce * dt / totalInvMass;

            if (part.name != "torso") {
                part.velocity += impulse * invMassB;
                parent.velocity -= impulse * invMassA;
            }
        }
    }

    // Clamp velocities after constraints
    for (auto& part : mParts) {
        float speed = glm::length(part.velocity);
        if (speed > MAX_PART_SPEED)
            part.velocity *= MAX_PART_SPEED / speed;
    }
}

void RagdollModeSystem::worldCollision(const World& world)
{
    std::vector<int> candidates;
    for (auto& part : mParts) {
        if (part.name == "torso") continue;

        float r = part.capsuleRadius;
        glm::vec3 move = part.velocity * (1.0f / 60.0f);
        float moveLen = glm::length(move);

        Capsule cap;
        cap.a = part.position - glm::vec3(0, 0, part.capsuleHalfHeight);
        cap.b = part.position + glm::vec3(0, 0, part.capsuleHalfHeight);
        cap.r = r;

        gatherGLBTriangles(candidates, world, cap, move, "ragdollModePartCollision");

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
                        glm::vec3 tangent = part.velocity - contact.normal * velDot;
                        part.velocity += tangent * -0.15f;
                        part.angularVelocity *= 0.5f;
                    }
                }
            }
        }
    }
}

void RagdollModeSystem::syncToPlayer(Player& player)
{
    if (mTorsoIndex >= 0 && mTorsoIndex < (int)mParts.size()) {
        mTorsoPosition = mParts[mTorsoIndex].position;
        player.pos = mTorsoPosition;
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
    if (!mActive || !gRenderer) return;

    const auto& cfg = RagdollModeConfig::instance().data();

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    GLuint shader = gRenderer->shaderProgram;

    static GLint uViewLoc = -1, uProjLoc = -1, uModelLoc = -1;
    static GLint uUseColorLoc = -1, uColorLoc = -1, uTexLoc = -1;
    if (uViewLoc < 0) uViewLoc = glGetUniformLocation(shader, "view");
    if (uProjLoc < 0) uProjLoc = glGetUniformLocation(shader, "projection");
    if (uModelLoc < 0) uModelLoc = glGetUniformLocation(shader, "model");
    if (uUseColorLoc < 0) uUseColorLoc = glGetUniformLocation(shader, "uUseColor");
    if (uColorLoc < 0) uColorLoc = glGetUniformLocation(shader, "uColor");
    if (uTexLoc < 0) uTexLoc = glGetUniformLocation(shader, "uTex");

    MIMITA_GL_CALL(glUseProgram(shader));
    glUniformMatrix4fv(uViewLoc, 1, 0, &view[0][0]);
    glUniformMatrix4fv(uProjLoc, 1, 0, &proj[0][0]);
    glUniform1i(uUseColorLoc, 1);
    glUniform1i(uTexLoc, 0);

    for (size_t i = 0; i < mParts.size(); ++i) {
        const auto& part = mParts[i];

        // Draw capsule wireframe debug
        if (DebugConfig::DEBUG_RAGDOLL) {
            glm::vec4 color(0.2f, 0.8f, 1.0f, 0.6f);
            if (part.name == "head") color = glm::vec4(1.0f, 0.3f, 0.3f, 0.6f);
            else if (part.name == "leftArm") color = glm::vec4(0.3f, 1.0f, 0.3f, 0.6f);
            else if (part.name == "rightArm") color = glm::vec4(0.3f, 0.3f, 1.0f, 0.6f);
            else if (part.name == "leftLeg") color = glm::vec4(1.0f, 1.0f, 0.3f, 0.6f);
            else if (part.name == "rightLeg") color = glm::vec4(1.0f, 0.3f, 1.0f, 0.6f);

            DebugVis::drawWireSphere(camera, part.position, part.capsuleRadius, color);

            // Draw line from parent attachment
            if (part.parentIndex >= 0) {
                const auto& parent = mParts[part.parentIndex];
                glm::vec3 parentAttach = parent.position + part.parentAttachmentOffset;
                DebugVis::drawLine(camera, parentAttach, part.position,
                    glm::vec4(1.0f, 1.0f, 0.0f, 0.4f));
            }

            // Draw grab points
            if (mLeftGrab.active && mLeftGrab.partIndex == (int)i) {
                DebugVis::drawWireSphere(camera, mLeftGrab.grabPoint, 0.1f,
                    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                DebugVis::drawLine(camera, part.position, mLeftGrab.grabPoint,
                    glm::vec4(0.0f, 1.0f, 0.0f, 0.8f));
            }
            if (mRightGrab.active && mRightGrab.partIndex == (int)i) {
                DebugVis::drawWireSphere(camera, mRightGrab.grabPoint, 0.1f,
                    glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
                DebugVis::drawLine(camera, part.position, mRightGrab.grabPoint,
                    glm::vec4(0.0f, 0.0f, 1.0f, 0.8f));
            }

            // Label
            char label[128];
            snprintf(label, sizeof(label), "%s", part.name.c_str());
            DebugVis::drawWorldLabel(part.position + glm::vec3(0, 0, 0.3f),
                label, color);
        }
    }
}
