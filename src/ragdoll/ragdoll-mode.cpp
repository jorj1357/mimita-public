#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <utility>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "config.h"
#include "config/ragdoll-death-config.h"
#include "camera.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "entities/aimbody-config.h"
#include "render/render-player.h"
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

static glm::mat4 rigidWorld(const RigidBody& body)
{
    return glm::translate(glm::mat4(1.0f), body.position)
         * glm::mat4_cast(body.orientation);
}

// The hand is the capsule endpoint farther from the shoulder joint anchor.
static glm::vec3 partHandWorld(const RagdollModePart& part)
{
    Capsule cap = capsuleOf(part.body);
    glm::vec3 shoulder = part.body.position;
    if (part.parentIndex >= 0)
        shoulder = part.body.position + part.body.orientation * part.childLocalAnchor;
    return (glm::length(cap.a - shoulder) > glm::length(cap.b - shoulder)) ? cap.a : cap.b;
}

static bool raycastWorld(const World& world, const glm::vec3& origin,
                          const glm::vec3& dir, float maxDist, float radius,
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
        if (sweptSphereTriangle(origin, dir, radius, tri, maxDist, dist, normal, point)) {
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

// glm::quat_cast can return q or -q for the same rotation. Near the 90-degree
// X rest pose this alternates hemispheres and flips a part (historically the
// left leg). Canonicalize the sign so every part is consistently oriented.
static glm::quat quatFromMatrixCanonical(const glm::mat4& m)
{
    glm::quat q = glm::normalize(glm::quat_cast(glm::mat3(m)));
    if (q.w < 0.0f) q = -q;
    return q;
}

static glm::vec3 quatToRotationVector(const glm::quat& q);

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
    mAlive = RagdollBody{};
    mCameraSmoothInit = false;

    // Bind from the model's rest pose, not the currently animated pose.
    // Otherwise a limb's current animation rotation (for example an arm swung
    // about the shoulder mid-walk) is baked into meshLocal, and the visible
    // limb stays stuck at that angle relative to its ragdoll capsule. Resetting
    // the skeleton first snaps every limb to the capsule's canonical bind
    // frame; physics then owns it.
    {
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
    }
    player.updateModelWorldTransforms();
    mAlive.torsoPosition = player.pos;
    mAlive.rootWorldPosition = player.pos;
    initParts(player, mAlive);
    mAppliedConfigGeneration = cfg.generation;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Activated — %zu parts\n", mAlive.parts.size());

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
            mAlive.parts.size());
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
    RagdollBody& b = mAlive;
    if (b.torsoIndex >= 0 && b.torsoIndex < (int)b.parts.size()) {
        const RigidBody& torso = mAlive.parts[mAlive.torsoIndex].body;
        glm::mat4 torsoWorld = rigidWorld(torso);
        // Return the authoritative root to the same body-relative point it was
        // anchored at, so repeated toggles do not drift.
        player.pos = glm::vec3(torsoWorld * glm::vec4(mAlive.rootOffsetLocal, 1.0f));
        const auto& cfg = RagdollModeConfig::instance().data();
        player.vel = cfg.exitPreserveVelocity ? torso.linearVelocity : glm::vec3(0.0f);
        player.vel.z += cfg.exitHopVelocity;
    }

    // Restore any skeleton nodes the ragdoll neutralized, so the normal
    // animation root is valid again on the next frame.
    for (int anc : mAlive.rootAncestorNodes) {
        if (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size())
            player.perfectPoseSkeleton.nodes[anc].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[anc];
    }

    player.modelRootRotationActive = false;
    player.modelRootRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    mActive = false;
    mAlive = RagdollBody{};
    mCameraSmoothInit = false;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Deactivated\n");
}

// ── Normal-play physical aim body ───────────────────────────────────

void RagdollModeSystem::buildAimBody(Player& player)
{
    // Bind from the model's rest pose so meshLocal captures the true body-to-
    // mesh offset, exactly like ragdoll activation.
    {
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
    }
    player.updateModelWorldTransforms();

    mAim = RagdollBody{};
    mAim.torsoPosition = player.pos;
    mAim.rootWorldPosition = player.pos;
    initParts(player, mAim);
    mAimActive = !mAim.parts.empty();
    mAimAvatarName = player.avatarName();

    // Seed limb momentum from the current movement velocity so physical mode
    // does not appear to freeze on entry (RAG-003 analog). On a lifecycle
    // rebind this plants the rebuilt body at the authoritative position with
    // the authoritative velocity instead of the stale pre-teleport state.
    const float inherit =
        glm::clamp(RagdollModeConfig::instance().data().physicalAim.limbInheritance,
                   0.0f, 1.0f);
    for (auto& part : mAim.parts)
        part.body.linearVelocity = glm::mix(part.body.linearVelocity, player.vel, inherit);
}

void RagdollModeSystem::activateAim(Player& player)
{
    buildAimBody(player);
    Debug::log(Debug::Category::Ragdoll,
        "[AIMBODY] activated parts=%zu stiffness=%.1f\n",
        mAim.parts.size(),
        RagdollModeConfig::instance().data().physicalAim.torsoTetherStiffness);
}

void RagdollModeSystem::rebindAimToAuthoritativePlayer(Player& player)
{
    if (!mAimActive && mAim.parts.empty())
        return;
    buildAimBody(player);
    Debug::log(Debug::Category::Ragdoll,
        "[AIMBODY] rebound to authoritative lifecycle parts=%zu pos=(%.2f,%.2f,%.2f)\n",
        mAim.parts.size(), player.pos.x, player.pos.y, player.pos.z);
}

void RagdollModeSystem::deactivateAim(Player& player)
{
    for (int anc : mAim.rootAncestorNodes) {
        if (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size())
            player.perfectPoseSkeleton.nodes[anc].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[anc];
    }
    mAimActive = false;
    mAim = RagdollBody{};
    mAimAvatarName.clear();
    Debug::log(Debug::Category::Ragdoll, "[AIMBODY] deactivated\n");
}

bool RagdollModeSystem::aimBindingNeedsRebind(const Player& player) const
{
    return mAimActive && mAimAvatarName != player.avatarName();
}

// Pull the dynamic torso toward the authoritative movement root. The player
// root is never moved by the body; this is a one-way kinematic tether whose
// stiffness is editable in ragdoll.json (physical.torso_tether_stiffness).
void RagdollModeSystem::tetherAimRoot(const Player& player, RagdollBody& b, float dt)
{
    if (b.torsoIndex < 0 || b.torsoIndex >= (int)b.parts.size()) return;
    const auto& cfg = RagdollModeConfig::instance().data();
    RigidBody& torso = b.parts[b.torsoIndex].body;

    const glm::quat rootRot =
        glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f));
    // rootOffsetLocal stores the authoritative root in the torso bind frame, so
    // the bind torso position is player.pos - rootRot * rootOffsetLocal.
    const glm::vec3 target = player.pos - rootRot * b.rootOffsetLocal;

    const float k = glm::clamp(cfg.physicalAim.torsoTetherStiffness * dt, 0.0f, 1.0f);
    torso.position += (target - torso.position) * k;
    // Feed the movement velocity so the joint solver sees consistent relative
    // motion instead of reacting to an artificial teleport each tick.
    torso.linearVelocity = player.vel;
}

void RagdollModeSystem::applyAimMotor(RagdollBody& b, const glm::vec3& camForward, float dt)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    auto motor = [&](int idx, float weight) {
        if (idx < 0 || idx >= (int)b.parts.size()) return;
        RagdollModePart& part = b.parts[idx];
        RigidBody& body = part.body;

        glm::vec3 fwd = camForward;
        if (glm::length(fwd) < 1e-5f) return;
        glm::quat target = aimLookRotation(fwd, glm::vec3(0.0f, 0.0f, 1.0f))
                         * part.aimOffset;

        if (cfg.physicalAim.damping == PhysicalAimDamping::Physical) {
            const glm::vec3 torque = computeAimTorque(
                body.orientation, target, body.angularVelocity, cfg.physicalAim, weight);
            body.angularVelocity += torque * dt;
        } else {
            const glm::vec3 desiredVel = aimDesiredAngularVelocity(
                body.orientation, target, cfg.physicalAim, weight);
            const float blend = glm::clamp(dt * cfg.physicalAim.lookDamping, 0.0f, 1.0f);
            body.angularVelocity += (desiredVel - body.angularVelocity) * blend;
        }

        const float cap = std::min(body.maxAngularSpeed, cfg.physicalAim.maxAngularSpeed);
        const float spd = glm::length(body.angularVelocity);
        if (cap > 0.0f && spd > cap)
            body.angularVelocity *= cap / spd;
    };

    motor(b.headIndex, cfg.physicalAim.headWeight);
    motor(b.torsoIndex, cfg.physicalAim.torsoWeight);
}

void RagdollModeSystem::applyRightArmPointMotor(RagdollBody& b,
                                                const Player& player,
                                                const glm::vec3& camForward,
                                                const glm::vec3& camPosition,
                                                float dt)
{
    if (b.rightArmPointingBlend <= 1e-4f || b.rightArmIndex < 0 ||
        b.rightArmIndex >= (int)b.parts.size())
        return;

    const auto& cfg = RagdollModeConfig::instance().data();
    const auto& aimConfig = AimBodyConfig::instance();
    const float pointingBlend = glm::clamp(b.rightArmPointingBlend, 0.0f, 1.0f);
    const float aimStrength = aimConfig.rightArmPointingAimStrength();
    RagdollModePart& part = b.parts[b.rightArmIndex];
    RigidBody& body = part.body;
    if (glm::length(camForward) < 1e-5f) return;

    // Move the arm's physical center toward the configured player-local
    // centerline. This is a spring/force response, not a position assignment,
    // so movement momentum and collisions remain part of the result.
    const glm::quat rootRotation = glm::angleAxis(
        glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 targetArmPosition = player.pos + rootRotation *
        aimConfig.rightArmPointingCenterOffset();
    const glm::vec3 positionError = targetArmPosition - body.position;
    const glm::vec3 relativeVelocity = body.linearVelocity - player.vel;
    const glm::vec3 acceleration =
        positionError * cfg.physicalAim.rightArmPointingPositionForce *
            pointingBlend -
        relativeVelocity * cfg.physicalAim.rightArmPointingPositionDamping *
            1.0f;
    body.linearVelocity += acceleration * dt;

    glm::vec3 targetForward = glm::normalize(camForward);
    glm::vec3 targetUp(0.0f, 0.0f, 1.0f);

    // The weapon's actual barrel is the authoritative aim axis. Convert its
    // grip-to-muzzle direction into the arm frame, then aim the muzzle at a
    // distant point on the camera ray. This makes a right-offset hand/gun
    // converge on the camera center instead of merely running parallel to it.
    const glm::vec3 weaponBarrelLocal =
        player.weaponMuzzleLocal - player.weaponGripLocal;
    const glm::mat3 weaponToArm = glm::mat3(player.weaponLocalToArm);
    glm::vec3 armBarrelLocal = weaponToArm * weaponBarrelLocal;
    glm::vec3 armUpLocal = weaponToArm * glm::vec3(0.0f, 0.0f, 1.0f);
    bool hasWeaponAimAxis = glm::length(weaponBarrelLocal) > 1e-4f &&
                            glm::length(armBarrelLocal) > 1e-4f &&
                            glm::length(armUpLocal) > 1e-4f;
    if (hasWeaponAimAxis) {
        armBarrelLocal = glm::normalize(armBarrelLocal);
        armUpLocal = glm::normalize(armUpLocal);
        if (std::fabs(glm::dot(armBarrelLocal, armUpLocal)) > 0.98f)
            hasWeaponAimAxis = false;
    }

    if (hasWeaponAimAxis) {
        glm::vec3 muzzleWorld = player.weaponModelTransform[3];
        const glm::vec3 localMuzzle = player.weaponMuzzleLocal;
        if (glm::length(localMuzzle) > 1e-4f)
            muzzleWorld = glm::vec3(player.weaponModelTransform *
                                    glm::vec4(localMuzzle, 1.0f));
        const glm::vec3 cameraRayPoint = camPosition + targetForward * 100.0f;
        const glm::vec3 toTarget = cameraRayPoint - muzzleWorld;
        if (glm::length(toTarget) > 1e-4f)
            targetForward = glm::normalize(toTarget);
    }

    if (glm::length(glm::cross(targetForward, targetUp)) < 0.05f)
        targetUp = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::quat target;
    if (hasWeaponAimAxis) {
        const glm::quat desiredWorld = aimLookRotation(targetForward, targetUp);
        const glm::quat localWeaponInArm =
            aimLookRotation(armBarrelLocal, armUpLocal);
        target = glm::normalize(desiredWorld * glm::inverse(localWeaponInArm));
    } else {
        target = aimLookRotation(targetForward, targetUp) * part.aimOffset;
    }
    // This is an arm-local goal rotation, intentionally separate from the
    // weapon attachment transform. It is the hot tuning hook for correcting
    // pitch/roll/yaw if an avatar's bind orientation makes the arm point down.
    target = glm::normalize(target * glm::quat(glm::radians(
        aimConfig.rightArmPointingRotationDegrees())));

    // Reuse the same physical aim controller as ragdoll/head aiming. Rotation
    // and position are both physical; no transform is snapped. Aim strength
    // changes how quickly the arm follows the target, never the target itself,
    // its pitch range, or the positional spring that keeps the hand centered.
    PhysicalAimConfig pointingAim = cfg.physicalAim;
    if (cfg.physicalAim.damping == PhysicalAimDamping::Physical) {
        // Keep the PD target/caps unchanged so a larger strength cannot create
        // a different effective range of motion. It only increases the rate
        // at which the physical controller corrects its existing error.
        const float pdStrength = glm::clamp(aimStrength, 0.1f, 100.0f);
        pointingAim.torqueGain *= pdStrength;
        pointingAim.angularDamping *= pdStrength;
        body.angularVelocity += computeAimTorque(
            body.orientation, target, body.angularVelocity,
            pointingAim, pointingBlend) * dt;
    } else {
        const glm::vec3 desired = aimDesiredAngularVelocity(
            body.orientation, target, pointingAim, pointingBlend);
        const float blend = glm::clamp(
            dt * cfg.physicalAim.lookDamping * glm::max(0.1f, aimStrength),
            0.0f, 1.0f);
        body.angularVelocity += (desired - body.angularVelocity) * blend;
    }

    const float cap = std::min(body.maxAngularSpeed, cfg.physicalAim.maxAngularSpeed);
    const float speed = glm::length(body.angularVelocity);
    if (cap > 0.0f && speed > cap)
        body.angularVelocity *= cap / speed;
}

// Capture the procedural animation pose (world transform per part) before
// physics overwrites the skeleton. Hybrid springs pull the body toward it.
void RagdollModeSystem::captureAimTargets(const Player& player, RagdollBody& b)
{
    for (auto& part : b.parts) {
        if (part.nodeIndex < 0
            || part.nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size())
            continue;
        const glm::mat4& nodeWorld =
            player.perfectPoseSkeleton.nodes[part.nodeIndex].worldTransform;
        glm::mat4 bodyWorld = nodeWorld * glm::inverse(part.meshLocal);
        part.aimTargetPosition = glm::vec3(bodyWorld[3]);
        part.aimTargetOrientation = glm::normalize(glm::quat_cast(glm::mat3(bodyWorld)));
    }
}

// Pull the body toward the procedural animation pose with a stable exponential
// blend, so any follow force is safe (no explicit-spring blow-up). At high
// follow force the limbs look like the default animation; at low force they lag
// and carry momentum from movement, look, and impacts.
void RagdollModeSystem::applyHybridSprings(RagdollBody& b, float dt)
{
    const auto& pc = RagdollModeConfig::instance().data().physicalAim;
    const float baseRate = std::max(0.0f, pc.hybridBaseRate * pc.hybridFollowForce);
    if (baseRate <= 1e-4f) return;

    for (auto& part : b.parts) {
        const float pointingBlend = (part.name == "rightArm")
            ? glm::clamp(b.rightArmPointingBlend, 0.0f, 1.0f) : 0.0f;
        // Arm sway correction runs after physics below. Keeping this pre-physics
        // rate shared prevents the old arm multiplier from saturating here.
        float rate = baseRate;
        if (rate <= 1e-4f) continue;

        const float alpha = glm::clamp(1.0f - std::exp(-rate * dt), 0.0f, 1.0f);
        const float normalPoseAlpha = alpha * (1.0f - pointingBlend);
        if (normalPoseAlpha <= 1e-4f) continue;
        const float retain = 1.0f - normalPoseAlpha;
        RigidBody& body = part.body;

        body.orientation = glm::normalize(
            glm::slerp(body.orientation, part.aimTargetOrientation, normalPoseAlpha));
        body.angularVelocity *= retain;

        // The torso position is owned by the movement tether, not the animation.
        if (part.parentIndex >= 0) {
            const glm::vec3 delta = part.aimTargetPosition - body.position;
            body.position += delta * (normalPoseAlpha * pc.hybridPositionFollow);
            body.linearVelocity *= retain;
        }
    }
}

// Reduce movement-induced arm sway after joints and collision have finished.
// The target preserves the intended arm pose relative to the torso's current
// orientation, rather than pulling toward a stale world-space angle.
void RagdollModeSystem::stabilizeHybridArms(RagdollBody& b, float dt)
{
    const auto& pc = RagdollModeConfig::instance().data().physicalAim;
    if (b.torsoIndex < 0 || b.torsoIndex >= (int)b.parts.size()) return;

    const float force = std::max(0.0f, pc.hybridArmsFollowForce);
    const float cameraFollow = std::max(1.0f, pc.hybridArmsCameraFollow);
    const bool armsDefaultMode = AimBodyConfig::instance().armsDefaultMode();

    const glm::quat targetTorso =
        b.parts[b.torsoIndex].aimTargetOrientation;
    const glm::quat currentTorso =
        b.parts[b.torsoIndex].body.orientation;

    for (auto& part : b.parts) {
        if (part.name != "leftArm" && part.name != "rightArm") continue;
        const float pointingBlend = (part.name == "rightArm")
            ? glm::clamp(b.rightArmPointingBlend, 0.0f, 1.0f) : 0.0f;

        const glm::quat armRelativeToTargetTorso = glm::normalize(
            glm::inverse(targetTorso) * part.aimTargetOrientation);
        const glm::quat centeredTarget = glm::normalize(
            currentTorso * armRelativeToTargetTorso);

        // At 1.0 this extra branch is inert. Above 1.0, blend toward the
        // exact per-arm procedural/default target captured before physics.
        // This keeps weapon-specific attachment rotations out of the arm
        // target; the weapon renderer already applies those rotations.
        glm::quat target = centeredTarget;
        float alpha = 0.0f;
        if (armsDefaultMode || cameraFollow > 1.0f) {
            const float defaultBlend =
                armsDefaultMode ? 1.0f : glm::clamp(
                    (cameraFollow - 1.0f) / cameraFollow, 0.0f, 1.0f);
            target = glm::normalize(glm::slerp(
                part.body.orientation, centeredTarget, defaultBlend));
            const float extraRate = std::max(1.0f, pc.hybridBaseRate)
                                  * (armsDefaultMode
                                      ? std::max(1.0f, cameraFollow)
                                      : cameraFollow - 1.0f);
            alpha = glm::clamp(
                1.0f - std::exp(-extraRate * dt), 0.0f, 1.0f);
        } else if (force > 1e-4f) {
            alpha = glm::clamp(
                1.0f - std::exp(-force * dt), 0.0f, 1.0f);
        }

        alpha *= (1.0f - pointingBlend);
        if (alpha <= 1e-4f) continue;

        part.body.orientation = glm::normalize(
            glm::slerp(part.body.orientation, target, alpha));
        const float velocityRetention = cameraFollow > 1.0f
            ? (1.0f / cameraFollow) : (1.0f - alpha);
        part.body.angularVelocity *= velocityRetention;
    }
}

// Keep each limb inside its configured range of motion so the body cannot fold
// or throw limbs far from their attachments. The torso is clamped relative to
// the movement yaw frame; child limbs are clamped by swing magnitude.
void RagdollModeSystem::clampAimRanges(const Player& player, RagdollBody& b, float beta)
{
    const auto& pc = RagdollModeConfig::instance().data().physicalAim;
    beta = glm::clamp(beta, 0.0f, 1.0f);

    if (b.torsoIndex >= 0 && b.torsoIndex < (int)b.parts.size()) {
        RigidBody& torso = b.parts[b.torsoIndex].body;
        const glm::quat rootRot =
            glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat rel = glm::normalize(glm::inverse(rootRot) * torso.orientation);
        const glm::vec3 rv = quatToRotationVector(rel);
        const float pitch = glm::radians(pc.torsoMaxPitchDeg);
        const float roll = glm::radians(pc.torsoMaxRollDeg);
        glm::vec3 clamped(
            glm::clamp(rv.x, -pitch, pitch),
            rv.y,
            glm::clamp(rv.z, -roll, roll));
        glm::vec3 rejected = rv - clamped;
        if (glm::length(rejected) > 1e-6f)
            rotateBody(torso, rootRot * (-rejected * beta));
    }

    for (int pi = 0; pi < (int)b.parts.size(); ++pi) {
        RagdollModePart& part = b.parts[pi];
        if (pi == b.rightArmIndex && b.rightArmPointingBlend > 1e-3f) continue;
        if (part.parentIndex < 0 || part.parentIndex >= (int)b.parts.size()) continue;

        float maxDeg = pc.headMaxSwingDeg;
        if (part.name == "leftArm" || part.name == "rightArm")
            maxDeg = pc.armMaxSwingDeg;
        else if (part.name == "leftLeg" || part.name == "rightLeg")
            maxDeg = pc.legMaxSwingDeg;
        const float maxRad = glm::radians(maxDeg);
        if (maxRad <= 0.0f) continue;

        RigidBody& child = part.body;
        RigidBody& parent = b.parts[part.parentIndex].body;
        const glm::quat rel = glm::normalize(
            glm::inverse(parent.orientation) * child.orientation);
        const glm::quat delta = glm::normalize(
            rel * glm::inverse(part.bindRelativeRotation));
        const glm::vec3 rv = quatToRotationVector(delta);
        const float ang = glm::length(rv);
        if (ang <= maxRad + 1e-6f) continue;

        const glm::vec3 rejected = rv - rv * (maxRad / ang);
        const float invA = child.invInertia;
        const float invB = parent.invInertia;
        const float total = invA + invB;
        if (total < 1e-8f) continue;
        const glm::vec3 worldCorrection = parent.orientation * (-rejected);
        rotateBody(child, worldCorrection * (beta * invA / total));
        rotateBody(parent, -worldCorrection * (beta * invB / total));
    }
}

void RagdollModeSystem::syncAimToPlayer(Player& player, RagdollBody& b)
{
    if (b.torsoIndex < 0 || b.torsoIndex >= (int)b.parts.size()) return;
    b.torsoPosition = b.parts[b.torsoIndex].body.position;

    // Authoritative movement root. Unlike ragdoll sync, player.pos/vel are owned
    // by the movement controller and are never written here.
    glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), player.pos)
                        * glm::mat4_cast(glm::angleAxis(
                              glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f)));

    for (int anc : b.rootAncestorNodes)
        player.perfectPoseSkeleton.nodes[anc].localTransform = glm::mat4(1.0f);

    // No render smoothing: the visible transform must equal the physics
    // transform so damage hitboxes line up with what is drawn.
    for (int i = 0; i < (int)b.parts.size(); ++i) {
        RagdollModePart& part = b.parts[i];
        if (part.nodeIndex < 0
            || part.nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size())
            continue;

        glm::mat4 childWorld = glm::translate(glm::mat4(1.0f), part.body.position)
                             * glm::mat4_cast(part.body.orientation)
                             * part.meshLocal;
        glm::mat4 parentWorld = rootWorld;
        if (part.skeletonParentPart >= 0
            && part.skeletonParentPart < (int)b.parts.size()) {
            const RagdollModePart& parent = b.parts[part.skeletonParentPart];
            parentWorld = glm::translate(glm::mat4(1.0f), parent.body.position)
                        * glm::mat4_cast(parent.body.orientation)
                        * parent.meshLocal;
        }

        player.perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
            glm::inverse(parentWorld) * childWorld;
    }
}

void RagdollModeSystem::updateAim(float dt, const World& world, Player& player,
                                  const glm::vec3& camForward,
                                  const glm::vec3& camPosition,
                                  bool rightMouseHeld)
{
    if (!mAimActive) return;
    const auto& cfg = RagdollModeConfig::instance().data();
    RagdollBody& b = mAim;
    b.activationTime += dt;

    // The aim body is intentionally NOT rebuilt when ragdoll.json changes.
    // Its tuning is read live each tick, and rebuilding mid-play reset the
    // skeleton and could shove the player when the physical pose no longer
    // matched the movement root. Capsule/attachment geometry changes require
    // toggling the mode off and on.

    // Hybrid follows the procedural animation pose; capture it before physics
    // overwrites the skeleton this tick.
    const AimBodyConfig& aimConfig = AimBodyConfig::instance();
    const bool hybrid = aimConfig.hybridMode();
    const bool pointingWanted = aimConfig.rightArmPointingRmb()
                             && rightMouseHeld && b.rightArmIndex >= 0;
    const float blendTarget = pointingWanted ? 1.0f : 0.0f;
    const float blendAlpha = glm::clamp(
        1.0f - std::exp(-aimConfig.rightArmPointingBlendRate() * dt),
        0.0f, 1.0f);
    b.rightArmPointingBlend +=
        (blendTarget - b.rightArmPointingBlend) * blendAlpha;
    b.rightArmPointing = pointingWanted || b.rightArmPointingBlend > 1e-3f;
    if (hybrid)
        captureAimTargets(player, b);

    tetherAimRoot(player, b, dt);
    applyAimMotor(b, camForward, dt);
    if (hybrid)
        applyHybridSprings(b, dt);
    applyRightArmPointMotor(b, player, camForward, camPosition, dt);

    glm::vec3 gravity(0.0f, 0.0f, -GRAVITY * cfg.gravityScale);
    for (auto& part : b.parts)
        integrate(part.body, gravity, dt);

    solveJoints(cfg.solverIterations, true, b);

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            collideWithWorld(part.body, world, dt);
    }

    if (cfg.selfCollision)
        selfCollision(b);

    solveJoints(cfg.solverIterations / 2, true, b);

    if (cfg.selfCollision) {
        for (int i = 0; i < cfg.selfCollisionIterations; ++i) selfCollision(b);
    }

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            depenetrateWorld(part.body, world, 2);
    }

    if (hybrid)
        stabilizeHybridArms(b, dt);

    // Range limits keep limbs near their attachments; then the attachment's own
    // per-axis limits are enforced as a final pass.
    clampAimRanges(player, b, 1.0f);
    solveRotationLimits(1.0f, b);

    // Re-assert the tether so gravity/collision cannot let the body drift from
    // the player across the tick.
    tetherAimRoot(player, b, dt);

    syncAimToPlayer(player, b);
    player.updateModelWorldTransforms();

    cacheReplicatedPose(player, b,
        AimBodyConfig::instance().hybridMode()
            ? RAGDOLL_NET_HYBRID : RAGDOLL_NET_PHYSICAL,
        (uint32_t)player.movementSimulationTick);
}

void RagdollModeSystem::cacheReplicatedPose(const Player& player,
                                            const RagdollBody& b, uint8_t mode,
                                            uint32_t sourceTick)
{
    mReplicated.active = true;
    mReplicated.mode = mode;
    mReplicated.sourceTick = sourceTick;
    mReplicated.count = 0;
    // The owning client's root is authoritative while the body is physical.
    mReplicated.rootPosition = player.pos;
    mReplicated.rootYaw = player.yaw;
    for (int i = 0; i < kRagdollLimbCount; ++i) {
        const char* name = ragdollReplicatedPartName(i);
        const int pi = findPartByName(b.parts, name);
        if (pi < 0) continue;
        RagdollLimbState& s = mReplicated.limbs[mReplicated.count];
        s.position = b.parts[pi].body.position;
        s.orientation = b.parts[pi].body.orientation;

        // Damage hitbox using the exact formula the client's hit detection uses
        // for a rendered part: world center of the collider box, node-local
        // half extents (clamped), from the refreshed world transforms.
        for (const PhysicalBodyPart& part : player.physicalBody.parts) {
            if (part.name != name) continue;
            const glm::vec3 localCenter =
                (part.collider.localMin + part.collider.localMax) * 0.5f;
            s.hitCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            s.hitHalf = glm::max(
                (part.collider.localMax - part.collider.localMin) * 0.5f,
                glm::vec3(0.12f));
            break;
        }
        ++mReplicated.count;
    }
    if (mReplicated.count == 0)
        mReplicated.active = false;
}

void RagdollModeSystem::clearReplicatedPose()
{
    mReplicated.active = false;
    mReplicated.mode = RAGDOLL_NET_OFF;
    mReplicated.count = 0;
}

void RagdollModeSystem::clearReplicatedBody(uint32_t ownerId)
{
    mReplicatedBodies.erase(ownerId);
}

void RagdollModeSystem::applyReplicatedPose(Player& player, uint32_t ownerId,
                                            const RagdollReplicationPose& pose)
{
    if (!pose.active || pose.count == 0) {
        clearReplicatedBody(ownerId);
        return;
    }

    auto it = mReplicatedBodies.find(ownerId);
    if (it == mReplicatedBodies.end())
        it = mReplicatedBodies.emplace(ownerId, RagdollBody{}).first;
    RagdollBody& b = it->second;

    if (b.parts.empty()) {
        // Bind from the model rest pose so meshLocal is the true body-to-mesh
        // offset, exactly like local activation.
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
        player.updateModelWorldTransforms();
        initParts(player, b);
        if (b.parts.empty()) {
            mReplicatedBodies.erase(it);
            return;
        }
    }

    for (int i = 0; i < pose.count && i < kRagdollLimbCount; ++i) {
        const int pi = findPartByName(b.parts, ragdollReplicatedPartName(i));
        if (pi < 0) continue;
        RagdollModePart& part = b.parts[pi];
        part.body.position = pose.limbs[i].position;
        part.body.orientation = glm::normalize(pose.limbs[i].orientation);
    }

    // Authoritative movement root stays the replica's interpolated root; only
    // the limb transforms come from the wire.
    syncAimToPlayer(player, b);
    player.updateModelWorldTransforms();
}

void RagdollModeSystem::initParts(const Player& player, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    b.parts.clear();
    b.rootAncestorNodes.clear();
    b.torsoIndex = b.headIndex = b.leftArmIndex = b.rightArmIndex = -1;
    b.leftLegIndex = b.rightLegIndex = -1;

    struct PartDef { const char* name; float defaultMass; };
    const PartDef defs[] = {
        {"torso", 8.0f}, {"head", 2.0f},
        {"leftArm", 1.5f}, {"rightArm", 1.5f},
        {"leftLeg", 2.5f}, {"rightLeg", 2.5f},
    };

    auto findBodyPart = [&](const std::string& name) -> const PhysicalBodyPart* {
        for (const auto& bp : player.physicalBody.parts)
            if (bp.name == name) return &bp;
        return nullptr;
    };

    // Canonical physics frame: the character frame (yaw only), shared by all
    // parts. The model's baked Z-up rotation stays in meshLocal, so the body
    // frame has no surprise 90-degree rotation.
    const glm::quat bindCanonical = glm::angleAxis(glm::radians(player.yaw), glm::vec3(0, 0, 1));

    for (const auto& d : defs) {
        const PhysicalBodyPart* bp = findBodyPart(d.name);
        if (!bp || bp->nodeIndex < 0
            || bp->nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size())
            continue;

        RagdollModePart part;
        part.name = d.name;
        part.nodeIndex = bp->nodeIndex;

        const glm::mat4& nodeWorld = player.perfectPoseSkeleton.nodes[bp->nodeIndex].worldTransform;
        const glm::vec3 nodePos = glm::vec3(nodeWorld[3]);
        const glm::quat canonicalRot = bindCanonical;

        // Mesh rotation relative to the canonical body frame.
        glm::quat meshRot = glm::normalize(
            glm::inverse(canonicalRot) * quatFromMatrixCanonical(nodeWorld));

        // Derive the capsule from the mesh collider bounds (mesh space), then
        // express it in the canonical body frame.
        glm::vec3 capsuleCenter(0.0f);
        glm::vec3 axis(0.0f, 0.0f, 1.0f);
        float radius = 0.1f;
        float halfHeight = 0.2f;
        const Collider& col = bp->collider;
        if (col.localMax.x >= col.localMin.x || col.localMax.y >= col.localMin.y ||
            col.localMax.z >= col.localMin.z) {
            glm::vec3 e = (col.localMax - col.localMin) * 0.5f;
            capsuleCenter = (col.localMax + col.localMin) * 0.5f;
            int ai = 0;
            if (e.y > e[ai]) ai = 1;
            if (e.z > e[ai]) ai = 2;
            axis = glm::vec3(0.0f);
            axis[ai] = 1.0f;
            float other0 = e[(ai + 1) % 3];
            float other1 = e[(ai + 2) % 3];
            radius = std::max(0.02f, std::min(other0, other1));
            halfHeight = std::max(0.0f, e[ai] - radius);
        }
        capsuleCenter = meshRot * capsuleCenter;
        axis = meshRot * axis;

        glm::vec3 comOffset(0.0f);
        auto capIt = cfg.capsules.find(d.name);
        if (capIt != cfg.capsules.end()) {
            if (capIt->second.radius > 0.0f) radius = capIt->second.radius;
            if (capIt->second.halfHeight >= 0.0f) halfHeight = capIt->second.halfHeight;
            capsuleCenter += capIt->second.offset;
            if (capIt->second.hasAxis) axis = capIt->second.axis;
            comOffset = capIt->second.centerOfMass;
        }

        // The physics center of mass sits at the capsule center plus the
        // editable per-part COM offset. This makes a limb hang from its
        // attachment instead of behaving like an inverted pendulum.
        glm::vec3 comLocal = capsuleCenter + comOffset;
        part.body.position = nodePos + canonicalRot * comLocal;
        part.body.orientation = canonicalRot;
        part.body.capsuleRadius = radius;
        part.body.capsuleHalfHeight = halfHeight;
        part.body.localAxis = axis;
        // Capsule center relative to the body COM frame.
        part.body.capsuleCenter = -comOffset;

        glm::mat4 bodyBindWorld = glm::translate(glm::mat4(1.0f), part.body.position)
                                * glm::mat4_cast(canonicalRot);
        part.meshLocal = glm::inverse(bodyBindWorld) * nodeWorld;

        float massKg = d.defaultMass;
        auto massIt = cfg.massKg.find(d.name);
        if (massIt != cfg.massKg.end()) massKg = massIt->second;
        massKg *= cfg.massGlobalMultiplier;
        part.body.staticBody = false;
        setBodyMass(part.body, massKg);

        part.body.linearDamping = cfg.bodyLinearDamping;
        part.body.angularDamping = cfg.bodyAngularDamping;
        part.body.stopLinearSpeed = cfg.stopLinearSpeed;
        part.body.stopAngularSpeed = cfg.stopAngularSpeed;
        part.body.restitution = cfg.restitution;
        part.body.friction = cfg.friction;
        part.body.maxLinearSpeed = cfg.maxFallSpeed;
        part.body.maxAngularSpeed = cfg.maxAngularSpeed;

        // Aim offset: map the configured local front/up axes onto the look
        // convention (local +Y forward, +Z up).
        {
            glm::quat aimOffset(1.0f, 0.0f, 0.0f, 0.0f);
            auto aimIt = cfg.aim.find(d.name);
            if (aimIt != cfg.aim.end()) {
                glm::vec3 f = aimIt->second.frontAxis;
                glm::vec3 u = aimIt->second.upAxis;
                if (glm::length(f) > 1e-5f) {
                    f = glm::normalize(f);
                    if (glm::length(u) < 1e-5f) u = glm::vec3(0.0f, 0.0f, 1.0f);
                    u = glm::normalize(u);
                    glm::vec3 r = glm::cross(f, u);
                    if (glm::length(r) < 1e-5f) r = glm::cross(f, glm::vec3(0.0f, 0.0f, 1.0f));
                    if (glm::length(r) < 1e-5f) r = glm::cross(f, glm::vec3(1.0f, 0.0f, 0.0f));
                    r = glm::normalize(r);
                    u = glm::normalize(glm::cross(r, f));
                    glm::mat3 basis(r, f, u);
                    aimOffset = glm::inverse(glm::normalize(glm::quat_cast(basis)));
                }
            }
            part.aimOffset = aimOffset;
        }

        part.renderPosition = part.body.position;
        part.renderOrientation = part.body.orientation;
        part.renderSmoothed = false;

        // RAG-003: entering ragdoll preserves current velocity.
        part.body.linearVelocity = player.vel;

        int idx = (int)b.parts.size();
        b.parts.push_back(std::move(part));

        const std::string& n = b.parts[idx].name;
        if (n == "torso") b.torsoIndex = idx;
        else if (n == "head") b.headIndex = idx;
        else if (n == "leftArm") b.leftArmIndex = idx;
        else if (n == "rightArm") b.rightArmIndex = idx;
        else if (n == "leftLeg") b.leftLegIndex = idx;
        else if (n == "rightLeg") b.rightLegIndex = idx;
    }

    // Map skeleton node -> part index so each part can find its nearest
    // skeleton-ancestor part (flat export = root; nested export = parent part).
    std::unordered_map<int, int> nodeToPart;
    for (int i = 0; i < (int)b.parts.size(); ++i)
        nodeToPart[b.parts[i].nodeIndex] = i;

    for (int i = 0; i < (int)b.parts.size(); ++i) {
        RagdollModePart& part = b.parts[i];
        int anc = player.perfectPoseSkeleton.nodes[part.nodeIndex].parent;
        int guard = 0;
        while (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size() && guard++ < 64) {
            auto it = nodeToPart.find(anc);
            if (it != nodeToPart.end()) {
                part.skeletonParentPart = it->second;
                break;
            }
            anc = player.perfectPoseSkeleton.nodes[anc].parent;
        }
    }

    // Attachments: connect the capsule end nearest the parent body to a point
    // on the parent. The joint then pivots about that attachment (shoulder/hip),
    // not the player origin.
    const char* attachNames[] = {"head", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    for (const char* name : attachNames) {
        int childIdx = findPartByName(b.parts, name);
        if (childIdx < 0) continue;
        auto attIt = cfg.attachments.find(name);
        if (attIt == cfg.attachments.end()) continue;
        int parentIdx = findPartByName(b.parts, attIt->second.parent);
        if (parentIdx < 0) continue;

        RagdollModePart& child = b.parts[childIdx];
        RagdollModePart& parent = b.parts[parentIdx];

        child.parentIndex = parentIdx;
        // Arms may stretch a little away from the shoulder while extending.
        {
            const std::string partName(name);
            if (partName == "leftArm" || partName == "rightArm")
                child.maxStretch = cfg.armMaxStretch;
        }
        child.hasRotationLimits = attIt->second.hasRotationLimits;
        child.rotMinDeg = attIt->second.rotMinDeg;
        child.rotMaxDeg = attIt->second.rotMaxDeg;

        // Default anchor: the capsule end nearest the parent body. Can be
        // overridden independently on the parent side and the child side, so
        // an arm can attach and pivot at the top of the limb rather than the
        // capsule center.
        Capsule cap = capsuleOf(child.body);
        glm::vec3 anchor = (glm::length(cap.a - parent.body.position)
                            < glm::length(cap.b - parent.body.position)) ? cap.a : cap.b;

        if (attIt->second.hasChildOffset)
            child.childLocalAnchor = attIt->second.childOffset;
        else
            child.childLocalAnchor = glm::inverse(child.body.orientation)
                * (anchor - child.body.position);

        if (attIt->second.hasParentOffset)
            child.parentLocalAnchor = attIt->second.offset;
        else
            child.parentLocalAnchor = glm::inverse(parent.body.orientation)
                * (anchor - parent.body.position);

        glm::vec3 parentAnchorWorld = parent.body.position
            + parent.body.orientation * child.parentLocalAnchor;
        child.restLength = glm::length(child.body.position - parentAnchorWorld);

        glm::quat bindRel = glm::normalize(
            glm::inverse(parent.body.orientation) * child.body.orientation);
        if (bindRel.w < 0.0f) bindRel = -bindRel;
        child.bindRelativeRotation = bindRel;
    }

    // Collect non-part skeleton ancestors above the torso (e.g. plrOrigin) so
    // they can be neutralized while ragdoll owns the model root.
    if (b.torsoIndex >= 0) {
        int anc = player.perfectPoseSkeleton.nodes[b.parts[b.torsoIndex].nodeIndex].parent;
        int guard = 0;
        while (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size() && guard++ < 64) {
            if (nodeToPart.find(anc) != nodeToPart.end()) break;
            b.rootAncestorNodes.push_back(anc);
            anc = player.perfectPoseSkeleton.nodes[anc].parent;
        }
    }

    // Anchor the authoritative root in the torso bind frame.
    if (b.torsoIndex >= 0) {
        glm::mat4 torsoWorld = rigidWorld(b.parts[b.torsoIndex].body);
        b.rootOffsetLocal = glm::vec3(glm::inverse(torsoWorld) * glm::vec4(player.pos, 1.0f));
    }
}

// Re-derives capsule and attachment geometry from config while keeping the
// current body poses and velocities. Used for live ragdoll.json tuning.
void RagdollModeSystem::reinitPreservingState(Player& player, RagdollBody& b)
{
    struct Saved {
        glm::vec3 position;
        glm::quat orientation;
        glm::vec3 linearVelocity;
        glm::vec3 angularVelocity;
    };
    std::vector<std::pair<std::string, Saved>> saved;
    saved.reserve(b.parts.size());
    for (const auto& part : b.parts) {
        saved.push_back({part.name,
            {part.body.position, part.body.orientation,
             part.body.linearVelocity, part.body.angularVelocity}});
    }

    // Re-bind from the rest pose (not the live ragdoll pose) so meshLocal stays
    // the true body-to-mesh bind offset; the saved body poses below restore the
    // current ragdoll configuration on top of it.
    {
        const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                                  player.perfectPoseSkeleton.restLocalTransforms.size());
        for (size_t i = 0; i < n; ++i)
            player.perfectPoseSkeleton.nodes[i].localTransform =
                player.perfectPoseSkeleton.restLocalTransforms[i];
    }
    player.updateModelWorldTransforms();
    initParts(player, b);

    for (auto& part : b.parts) {
        for (const auto& s : saved) {
            if (s.first != part.name) continue;
            part.body.position = s.second.position;
            part.body.orientation = s.second.orientation;
            part.body.linearVelocity = s.second.linearVelocity;
            part.body.angularVelocity = s.second.angularVelocity;
            break;
        }
    }

    if (b.torsoIndex >= 0) {
        glm::mat4 torsoWorld = rigidWorld(b.parts[b.torsoIndex].body);
        b.rootOffsetLocal = glm::vec3(glm::inverse(torsoWorld) * glm::vec4(b.rootWorldPosition, 1.0f));
    }

    Debug::log(Debug::Category::Ragdoll,
        "[RAGDOLL MODE] Geometry re-initialized (config generation %llu, parts=%zu)\n",
        (unsigned long long)mAppliedConfigGeneration, b.parts.size());
}

void RagdollModeSystem::update(float dt, const World& world, Player& player,
                                const InputState& input, const Camera& camera)
{
    if (!mActive) return;
    RagdollBody& b = mAlive;

    const auto& cfg = RagdollModeConfig::instance().data();

    // Live tuning: apply capsule/attachment geometry changes immediately while
    // preserving the current pose and momentum.
    if (cfg.generation != mAppliedConfigGeneration) {
        mAppliedConfigGeneration = cfg.generation;
        if (!b.parts.empty())
            reinitPreservingState(player, b);
    }

    b.activationTime += dt;

    // Step 1: Inputs and physical controls (head aim, arm extension, grabs).
    applyControls(dt, input, camera, b);
    processGrab(input, camera, world, b);
    processExtend(input, camera, dt, b);

    // Step 2: Integrate gravity and motion for every body part.
    glm::vec3 gravity(0.0f, 0.0f, -GRAVITY * cfg.gravityScale);
    for (auto& part : b.parts)
        integrate(part.body, gravity, dt);

    // Step 3: Joints (velocity + position), angular limits, and cone limits.
    solveJoints(cfg.solverIterations, true, b);

    // Step 4: Grab constraints (velocity + position).
    solveGrabs(cfg.solverIterations, b);

    // Step 5: World collision (swept, substepped).
    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            collideWithWorld(part.body, world, dt);
    }

    // Step 6: One self-collision pre-pass before the joints.
    if (cfg.selfCollision)
        selfCollision(b);

    // Step 7: Re-converge constraints after collision so links stay rigid.
    solveJoints(cfg.solverIterations / 2, true, b);
    solveGrabs(cfg.solverIterations / 2, b);

    // Step 7b: Self collision again so the joint pass cannot re-penetrate parts.
    if (cfg.selfCollision) {
        for (int i = 0; i < cfg.selfCollisionIterations; ++i) selfCollision(b);
    }

    // Step 8: Final overlap cleanup so constraints cannot leave a part buried.
    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            depenetrateWorld(part.body, world, 2);
    }

    // Step 8b: Natural stop. A part below the configured speed is treated as
    // at rest for this tick so limbs settle instead of jittering.
    for (auto& part : b.parts) {
        if (part.body.stopLinearSpeed > 0.0f &&
            glm::length(part.body.linearVelocity) < part.body.stopLinearSpeed)
            part.body.linearVelocity = glm::vec3(0.0f);
        if (part.body.stopAngularSpeed > 0.0f &&
            glm::length(part.body.angularVelocity) < part.body.stopAngularSpeed)
            part.body.angularVelocity = glm::vec3(0.0f);
    }

    // Step 8c: Final strict angular limits so no later pass overrides them.
    solveRotationLimits(1.0f, b);

    // Step 9: Write the authoritative root and skeleton transforms.
    syncToPlayer(player, b);
    player.updateModelWorldTransforms();

    cacheReplicatedPose(player, b, RAGDOLL_NET_RAGDOLL,
                        (uint32_t)player.movementSimulationTick);

    if (StructuredLogger::instance().shouldLog(StructuredCategory::Ragdoll, StructuredLevel::Trace)) {
        float totalKE = 0.0f;
        for (const auto& part : b.parts) {
            float spd = glm::length(part.body.linearVelocity);
            totalKE += 0.5f * part.body.mass * spd * spd;
        }
        char msg[1024];
        snprintf(msg, sizeof(msg),
            "[RAGDOLLDEBUG] RAGDOLL_TICK player=%s time=%.3f grounded=%d "
            "yaw=%.3f tick=%u grab_left=%d grab_right=%d kinetic_energy=%.3f parts=%zu",
            player.username.c_str(), b.activationTime,
            (int)player.ground.onGround, player.yaw,
            (uint32_t)player.movementSimulationTick,
            (int)b.leftGrab.active, (int)b.rightGrab.active, totalKE, b.parts.size());
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

void RagdollModeSystem::applyControls(float dt, const InputState& input, const Camera& camera, RagdollBody& b)
{
    (void)input;
    const auto& cfg = RagdollModeConfig::instance().data();

    // Physically rotate a body toward the camera look direction. No orientation
    // is assigned directly; the controller sets angular velocity only. The
    // damping model is selected in config: "look" is the historical velocity
    // blend, "physical" is the PD torque model.
    auto aimAtCamera = [&](RagdollModePart& part, float strength, float maxSpeed,
                           float weight) {
        RigidBody& body = part.body;

        glm::vec3 upHint(0.0f, 0.0f, 1.0f);
        if (glm::length(glm::cross(camera.front, upHint)) < 0.05f)
            upHint = camera.up;

        glm::quat target = aimLookRotation(camera.front, upHint) * part.aimOffset;

        if (cfg.physicalAim.damping == PhysicalAimDamping::Physical) {
            const glm::vec3 torque = computeAimTorque(
                body.orientation, target, body.angularVelocity, cfg.physicalAim, weight);
            body.angularVelocity += torque * dt;
            const float spd = glm::length(body.angularVelocity);
            if (spd > maxSpeed && spd > 0.0f)
                body.angularVelocity *= maxSpeed / spd;
            return;
        }

        glm::quat diff = glm::normalize(target * glm::inverse(body.orientation));
        float w = glm::clamp(diff.w, -1.0f, 1.0f);
        float angle = 2.0f * std::acos(std::fabs(w));
        float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
        glm::vec3 axis = (s > 1e-4f) ? glm::vec3(diff.x, diff.y, diff.z) / s
                                     : glm::vec3(0.0f, 0.0f, 1.0f);
        if (w < 0.0f) axis = -axis;

        float desiredSpeed = glm::clamp(angle * strength, -maxSpeed, maxSpeed);
        glm::vec3 desiredVel = axis * desiredSpeed;
        float blend = glm::clamp(dt * cfg.lookDamping, 0.0f, 1.0f);
        body.angularVelocity += (desiredVel - body.angularVelocity) * blend;

        float spd = glm::length(body.angularVelocity);
        if (spd > maxSpeed)
            body.angularVelocity *= maxSpeed / spd;
    };

    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size())
        aimAtCamera(b.parts[b.headIndex], cfg.headRotationStrength,
                    cfg.headRotationSpeed, cfg.physicalAim.headWeight);

    // The torso strongly wishes to face where the camera looks, so the body
    // reads clearly in third person. Strength and max speed are tunable.
    if (b.torsoIndex >= 0 && b.torsoIndex < (int)b.parts.size())
        aimAtCamera(b.parts[b.torsoIndex], cfg.torsoLookSpring,
                    cfg.torsoMaxAngularStep, cfg.physicalAim.torsoWeight);
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

void RagdollModeSystem::solveJoints(int iterations, bool positionPass, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    for (int iter = 0; iter < iterations; ++iter) {
        for (int pi = 0; pi < (int)b.parts.size(); ++pi) {
            RagdollModePart& part = b.parts[pi];
            if (part.parentIndex < 0) continue;
            if (part.parentIndex >= (int)b.parts.size()) continue;

            RigidBody& child = part.body;
            RigidBody& parent = b.parts[part.parentIndex].body;

            glm::vec3 parentAnchor = parent.position
                + parent.orientation * part.parentLocalAnchor;
            glm::vec3 childAnchor = child.position
                + child.orientation * part.childLocalAnchor;

            const float beta = glm::clamp(cfg.jointPositionBeta, 0.0f, 1.0f);

            // Arms may stretch only while reaching (extending) and not holding a
            // grab. While grabbing the shoulder stays rigid so the pinned hand
            // pivots the arm at the shoulder instead of letting it float/spin
            // about its center. Otherwise the joint is rigid too.
            float stretch = 0.0f;
            if (part.maxStretch > 0.0f) {
                const bool isLeft = (pi == b.leftArmIndex);
                const bool isRight = (pi == b.rightArmIndex);
                const bool grabbing = (isLeft && b.leftGrab.active) || (isRight && b.rightGrab.active);
                const bool extending = (isLeft && b.leftArmExtending) ||
                    (isRight && (b.rightArmExtending ||
                                 b.rightArmPointingBlend > 1e-3f));
                if (extending && !grabbing)
                    stretch = (isRight && b.rightArmPointingBlend > 1e-3f)
                        ? glm::mix(part.maxStretch,
                                   std::max(part.maxStretch,
                                            cfg.physicalAim.rightArmPointingMaxStretch),
                                   glm::clamp(b.rightArmPointingBlend, 0.0f, 1.0f))
                        : part.maxStretch;
            }

            if (stretch > 0.0f) {
                // Anchors coincide at bind, so the allowed separation is the
                // stretch amount itself (base 0), not restLength.
                solvePointJointMaxDistanceVelocity(parent, parentAnchor, child, childAnchor, stretch);
                if (positionPass)
                    solvePointJointMaxDistance(parent, parentAnchor, child, childAnchor, stretch, beta);
            } else {
                solvePointJointVelocity(parent, parentAnchor, child, childAnchor);
                if (positionPass)
                    solvePointJointPosition(parent, parentAnchor, child, childAnchor, beta);
            }
        }

        if (positionPass)
            solveRotationLimits(-1.0f, b);
    }
}

void RagdollModeSystem::solveRotationLimits(float betaOverride, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    const float kBeta = (betaOverride >= 0.0f)
        ? glm::clamp(betaOverride, 0.0f, 1.0f)
        : glm::clamp(cfg.limitPositionBeta, 0.0f, 1.0f);

    for (int pi = 0; pi < (int)b.parts.size(); ++pi) {
        RagdollModePart& part = b.parts[pi];
        if (!part.hasRotationLimits) continue;
        if (part.parentIndex < 0 || part.parentIndex >= (int)b.parts.size()) continue;

        // An actively extending arm must be free to point along camera-forward.
        if ((pi == b.leftArmIndex && b.leftArmExtending) ||
            (pi == b.rightArmIndex && (b.rightArmExtending ||
                                       b.rightArmPointingBlend > 1e-3f)))
            continue;

        RigidBody& child = part.body;
        RigidBody& parent = b.parts[part.parentIndex].body;

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

void RagdollModeSystem::solveGrabs(int iterations, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    float beta = glm::clamp(0.9f - cfg.grabCompliance * 20.0f, 0.1f, 0.95f);

    auto solve = [&](RagdollGrabState& grab) {
        if (!grab.active || grab.partIndex < 0
            || grab.partIndex >= (int)b.parts.size()) return;
        RigidBody& body = b.parts[grab.partIndex].body;
        for (int i = 0; i < iterations; ++i) {
            glm::vec3 hand = body.position + body.orientation * grab.handLocalAnchor;
            // Cancel the anchor velocity first so gravity cannot accumulate,
            // then project the position so the grip stays effectively rigid.
            solvePointToWorldVelocity(body, hand);
            solvePointToWorld(body, hand, grab.grabPoint, beta);
        }
    };

    solve(b.leftGrab);
    solve(b.rightGrab);
}

void RagdollModeSystem::processGrab(const InputState& input, const Camera& camera, const World& world, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    auto tryGrab = [&](RagdollGrabState& grab, int armIndex) {
        if (armIndex < 0 || armIndex >= (int)b.parts.size()) return;
        RagdollModePart& arm = b.parts[armIndex];
        glm::vec3 handWorld = partHandWorld(arm);
        glm::vec3 rayDir = glm::normalize(camera.front);
        glm::vec3 hitPoint, hitNormal;
        if (!raycastWorld(world, handWorld, rayDir, cfg.grabReach, cfg.grabRadius,
                          hitPoint, hitNormal))
            return;

        // Only grab within the grace radius of the hand, otherwise the hand
        // would teleport to a distant point and yank the body into geometry.
        float dist = glm::length(hitPoint - handWorld);
        if (dist > cfg.grabGraceDistance + arm.body.capsuleRadius)
            return;

        grab.active = true;
        grab.grabPoint = hitPoint;
        grab.grabNormal = hitNormal;
        grab.handPosition = handWorld;
        grab.handLocalAnchor = glm::inverse(arm.body.orientation)
            * (handWorld - arm.body.position);
        grab.partIndex = armIndex;
    };

    bool leftHeld = input.grabLeftHeld;
    if (leftHeld && !b.leftGrab.active)
        tryGrab(b.leftGrab, b.leftArmIndex);
    else if (!leftHeld)
        b.leftGrab.active = false;
    b.leftGrab.wasActive = b.leftGrab.active;

    bool rightHeld = input.grabRightHeld;
    if (rightHeld && !b.rightGrab.active)
        tryGrab(b.rightGrab, b.rightArmIndex);
    else if (!rightHeld)
        b.rightGrab.active = false;
    b.rightGrab.wasActive = b.rightGrab.active;
}

void RagdollModeSystem::processExtend(const InputState& input, const Camera& camera, float dt, RagdollBody& b)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    glm::vec3 targetDir = glm::normalize(camera.front);

    b.leftArmExtending = input.extendLeftMouse && b.leftArmIndex >= 0;
    b.rightArmExtending = input.extendRightMouse && b.rightArmIndex >= 0;

    // Steer the arm so the hand points along camera-forward. This changes only
    // the arm's angular velocity (an internal motor at the shoulder), so it adds
    // no net linear momentum and cannot pull the player forward. The rigid
    // shoulder attachment keeps the hand within arm's length of the body.
    auto extend = [&](int armIndex) {
        if (armIndex < 0 || armIndex >= (int)b.parts.size()) return;
        RagdollModePart& part = b.parts[armIndex];
        RigidBody& arm = part.body;

        glm::vec3 shoulder = arm.position + arm.orientation * part.childLocalAnchor;
        glm::vec3 hand = partHandWorld(part);
        glm::vec3 current = hand - shoulder;
        float currentLen = glm::length(current);
        if (currentLen < 1e-5f) return;
        current /= currentLen;

        float dotp = glm::clamp(glm::dot(current, targetDir), -1.0f, 1.0f);
        glm::vec3 axis = glm::cross(current, targetDir);
        float axisLen = glm::length(axis);
        if (axisLen < 1e-5f) return;
        axis /= axisLen;

        float angle = std::acos(dotp);
        float desiredSpeed = glm::clamp(angle * cfg.armExtendStrength,
                                        -cfg.armExtendMaxSpeed, cfg.armExtendMaxSpeed);
        float currentSpeed = glm::dot(arm.angularVelocity, axis);
        float blend = glm::clamp(dt * 12.0f, 0.0f, 1.0f);
        arm.angularVelocity += axis * ((desiredSpeed - currentSpeed) * blend);

        // Reach: push the arm away from the shoulder until it hits max stretch.
        // The one-sided shoulder joint lets it telescope, so this does not drag
        // the body until the arm is fully extended (and only then, if body_pull
        // is enabled, or when the hand is grabbing the world).
        if (part.parentIndex >= 0 && part.parentIndex < (int)b.parts.size()) {
            const RigidBody& parent = b.parts[part.parentIndex].body;
            glm::vec3 parentAnchor = parent.position
                + parent.orientation * part.parentLocalAnchor;
            float sep = glm::length(shoulder - parentAnchor);
            float maxSep = part.restLength + part.maxStretch;
            if (sep < maxSep) {
                arm.linearVelocity += targetDir * (cfg.armStretchForce * dt
                                                   / std::max(arm.mass, 0.001f));
            } else if (cfg.armBodyPull > 0.0f && b.torsoIndex >= 0) {
                RigidBody& torso = b.parts[b.torsoIndex].body;
                torso.linearVelocity += targetDir * (cfg.armBodyPull * dt
                                                     / std::max(torso.mass, 0.001f));
            }
        }
    };

    if (b.leftArmExtending) extend(b.leftArmIndex);
    if (b.rightArmExtending) extend(b.rightArmIndex);
}

void RagdollModeSystem::selfCollision(RagdollBody& b)
{
    for (int i = 0; i < (int)b.parts.size(); ++i) {
        for (int j = i + 1; j < (int)b.parts.size(); ++j) {
            // Directly-jointed parts overlap at their shared joint by design, so
            // they collide everywhere except a small sphere around that joint.
            const bool connected = (b.parts[i].parentIndex == j || b.parts[j].parentIndex == i);
            glm::vec3 excludePoint(0.0f);
            float excludeRadius = 0.0f;
            if (connected) {
                int childIdx = (b.parts[i].parentIndex == j) ? i : j;
                int parentIdx = b.parts[childIdx].parentIndex;
                if (parentIdx < 0 || parentIdx >= (int)b.parts.size()) continue;
                const RagdollModePart& c = b.parts[childIdx];
                const RigidBody& p = b.parts[parentIdx].body;
                excludePoint = p.position + p.orientation * c.parentLocalAnchor;
                excludeRadius = c.body.capsuleRadius + p.capsuleRadius;
            }
            const auto& cfg = RagdollModeConfig::instance().data();
            collideBodies(b.parts[i].body, b.parts[j].body, excludePoint, excludeRadius,
                          cfg.selfCollisionBeta, cfg.selfCollisionSkin,
                          cfg.selfCollisionMaxCorrection);
        }
    }
}

void RagdollModeSystem::syncToPlayer(Player& player, RagdollBody& b)
{
    if (b.torsoIndex < 0 || b.torsoIndex >= (int)b.parts.size()) return;

    const RigidBody& torso = b.parts[b.torsoIndex].body;
    glm::mat4 torsoWorld = rigidWorld(torso);

    // The authoritative player root stays anchored to a fixed point in the
    // torso frame, so it rotates with the body and does not drift on toggles.
    b.torsoPosition = torso.position;
    player.pos = glm::vec3(torsoWorld * glm::vec4(b.rootOffsetLocal, 1.0f));
    b.rootWorldPosition = player.pos;
    player.vel = torso.linearVelocity;
    player.modelRootRotationActive = true;
    player.modelRootRotation = torso.orientation;

    glm::mat4 rootWorld = glm::translate(glm::mat4(1.0f), player.pos)
                        * glm::mat4_cast(torso.orientation);

    // Neutralize non-part ancestors so the torso frame is the model root.
    for (int anc : b.rootAncestorNodes)
        player.perfectPoseSkeleton.nodes[anc].localTransform = glm::mat4(1.0f);

    const float smoothing = RagdollModeConfig::instance().data().bodySmoothing;
    const float smoothAlpha = (smoothing <= 0.0f)
        ? 1.0f
        : glm::clamp(1.0f - smoothing, 0.02f, 1.0f);

    for (int i = 0; i < (int)b.parts.size(); ++i) {
        RagdollModePart& part = b.parts[i];
        if (part.nodeIndex < 0
            || part.nodeIndex >= (int)player.perfectPoseSkeleton.nodes.size())
            continue;

        // Render-only smoothing: blend the transform used for the mesh toward
        // the physics transform. Physics and gameplay keep the raw body state.
        if (!part.renderSmoothed) {
            part.renderPosition = part.body.position;
            part.renderOrientation = part.body.orientation;
            part.renderSmoothed = true;
        } else {
            part.renderPosition += (part.body.position - part.renderPosition) * smoothAlpha;
            part.renderOrientation = glm::normalize(
                glm::slerp(part.renderOrientation, part.body.orientation, smoothAlpha));
        }

        glm::mat4 childWorld = glm::translate(glm::mat4(1.0f), part.renderPosition)
                             * glm::mat4_cast(part.renderOrientation)
                             * part.meshLocal;
        glm::mat4 parentWorld = rootWorld;
        if (part.skeletonParentPart >= 0 && part.skeletonParentPart < (int)b.parts.size()) {
            const RagdollModePart& parent = b.parts[part.skeletonParentPart];
            parentWorld = glm::translate(glm::mat4(1.0f), parent.renderPosition)
                        * glm::mat4_cast(parent.renderOrientation)
                        * parent.meshLocal;
        }

        player.perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
            glm::inverse(parentWorld) * childWorld;
    }

    // Symmetry check: the model's left/right legs are identical, so any
    // left-only axis problem shows up here as a body-vs-node mismatch or a
    // non-mirrored frame. Low volume, one line per second.
    if (b.leftLegIndex >= 0 && b.rightLegIndex >= 0) {
        const RagdollModePart& L = b.parts[b.leftLegIndex];
        const RagdollModePart& R = b.parts[b.rightLegIndex];
        glm::vec3 nodeL(0.0f), nodeR(0.0f);
        if (L.nodeIndex >= 0 && L.nodeIndex < (int)player.perfectPoseSkeleton.nodes.size())
            nodeL = glm::vec3(player.perfectPoseSkeleton.nodes[L.nodeIndex].worldTransform[3]);
        if (R.nodeIndex >= 0 && R.nodeIndex < (int)player.perfectPoseSkeleton.nodes.size())
            nodeR = glm::vec3(player.perfectPoseSkeleton.nodes[R.nodeIndex].worldTransform[3]);
        Debug::logThrottled(Debug::Category::Ragdoll, "ragdoll_leg_sym", 1.0f,
            "[RAGDOLL SYM] L body=(%.3f %.3f %.3f) node=(%.3f %.3f %.3f) q=(%.3f %.3f %.3f %.3f) | "
            "R body=(%.3f %.3f %.3f) node=(%.3f %.3f %.3f) q=(%.3f %.3f %.3f %.3f)\n",
            L.body.position.x, L.body.position.y, L.body.position.z,
            nodeL.x, nodeL.y, nodeL.z,
            L.body.orientation.w, L.body.orientation.x, L.body.orientation.y, L.body.orientation.z,
            R.body.position.x, R.body.position.y, R.body.position.z,
            nodeR.x, nodeR.y, nodeR.z,
            R.body.orientation.w, R.body.orientation.x, R.body.orientation.y, R.body.orientation.z);
    }
}

glm::vec3 RagdollModeSystem::getHeadPosition() const
{
    const RagdollBody& b = mAlive;
    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size())
        return b.parts[b.headIndex].body.position;
    return b.torsoPosition + glm::vec3(0, 0, 1.15f);
}

glm::mat4 RagdollModeSystem::getHeadTransform() const
{
    const RagdollBody& b = mAlive;
    if (b.headIndex >= 0 && b.headIndex < (int)b.parts.size()) {
        const RigidBody& head = b.parts[b.headIndex].body;
        return glm::translate(glm::mat4(1.0f), head.position)
             * glm::mat4_cast(head.orientation);
    }
    return glm::translate(glm::mat4(1.0f), b.torsoPosition + glm::vec3(0, 0, 1.15f));
}

glm::vec3 RagdollModeSystem::computeCameraPosition(float dt)
{
    RagdollBody& b = mAlive;
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
    const RagdollBody& b = mAlive;

    const auto& rcfg = RagdollModeConfig::instance().data();
    if (!rcfg.debugHitboxesVisible) return;

    const bool showAttach = rcfg.attachmentsVisible || DebugConfig::DEBUG_RAGDOLL;

    if (showAttach) {
        DebugVis::drawWeaponWireSphere(camera, b.rootWorldPosition, 0.07f, glm::vec4(1, 1, 1, 1));
        DebugVis::drawWorldLabel(b.rootWorldPosition + glm::vec3(0, 0, 0.12f), "plrOrigin", glm::vec4(1, 1, 1, 1));
        DebugVis::drawWeaponLine(camera, b.torsoPosition, b.rootWorldPosition, glm::vec4(1, 1, 1, 0.5f));
    }

    for (size_t i = 0; i < b.parts.size(); ++i) {
        const auto& part = b.parts[i];

        float alpha = 0.9f;
        const auto& capsules = RagdollModeConfig::instance().data().capsules;
        auto capCfg = capsules.find(part.name);
        if (capCfg != capsules.end()) alpha = glm::clamp(capCfg->second.alpha, 0.0f, 1.0f);

        glm::vec4 color(0.2f, 0.8f, 1.0f, alpha);
        if (part.name == "head") color = glm::vec4(1.0f, 0.3f, 0.3f, alpha);
        else if (part.name == "torso") color = glm::vec4(0.2f, 0.8f, 1.0f, alpha);
        else if (part.name == "leftArm") color = glm::vec4(0.3f, 1.0f, 0.3f, alpha);
        else if (part.name == "rightArm") color = glm::vec4(0.3f, 0.3f, 1.0f, alpha);
        else if (part.name == "leftLeg") color = glm::vec4(1.0f, 1.0f, 0.3f, alpha);
        else if (part.name == "rightLeg") color = glm::vec4(1.0f, 0.3f, 1.0f, alpha);

        Capsule cap = capsuleOf(part.body);
        DebugVis::drawWeaponCapsuleWire(camera, cap, color);

        if (part.parentIndex >= 0) {
            const RigidBody& parent = b.parts[part.parentIndex].body;
            glm::vec3 parentAttach = parent.position
                + parent.orientation * part.parentLocalAnchor;
            glm::vec3 childAttach = part.body.position
                + part.body.orientation * part.childLocalAnchor;

            DebugVis::drawWeaponLine(camera, parentAttach, childAttach,
                glm::vec4(1.0f, 1.0f, 0.0f, 0.85f));

            if (showAttach) {
                DebugVis::drawWeaponWireSphere(camera, parentAttach, 0.05f, glm::vec4(0, 1, 1, 1));
                DebugVis::drawWeaponWireSphere(camera, childAttach, 0.05f, glm::vec4(1, 0, 1, 1));
                DebugVis::drawWeaponLine(camera, parentAttach, b.rootWorldPosition,
                    glm::vec4(1.0f, 0.5f, 0.0f, 0.55f));
                DebugVis::drawWeaponLine(camera, childAttach, part.body.position,
                    glm::vec4(0.6f, 0.6f, 0.6f, 0.6f));

                // Body frame axes (X red, Y green, Z blue).
                glm::mat4 bodyM = rigidWorld(part.body);
                const float ax = 0.18f;
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[0]) * ax, glm::vec4(1, 0.1f, 0.1f, 0.9f));
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[1]) * ax, glm::vec4(0.1f, 1, 0.1f, 0.9f));
                DebugVis::drawWeaponLine(camera, part.body.position,
                    part.body.position + glm::vec3(bodyM[2]) * ax, glm::vec4(0.1f, 0.4f, 1, 0.9f));

                char lbl[96];
                snprintf(lbl, sizeof(lbl), "%s attach", part.name.c_str());
                DebugVis::drawWorldLabel(childAttach + glm::vec3(0, 0, 0.08f), lbl,
                    glm::vec4(1, 0, 1, 1));
            }
        }

        if (b.leftGrab.active && b.leftGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, b.leftGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, b.leftGrab.grabPoint,
                glm::vec4(0.0f, 1.0f, 0.0f, 0.9f));
        }
        if (b.rightGrab.active && b.rightGrab.partIndex == (int)i) {
            DebugVis::drawWeaponWireSphere(camera, b.rightGrab.grabPoint, 0.1f,
                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
            DebugVis::drawWeaponLine(camera, part.body.position, b.rightGrab.grabPoint,
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

// ============================================================================
// Corpse ragdolls
//
// A corpse is a cloned Player body driven by the same RigidBody solver as the
// alive ragdoll, but with no input, grabs, or look motors. This is the death
// presentation for players and NPCs (they share the Player body type). It is
// simulated client-side now; the spawn parameters are shaped so the server can
// own and broadcast the same event later.
// ============================================================================

void RagdollModeSystem::spawnCorpse(const Player& victim,
                                    const glm::vec3& deathImpulse,
                                    const std::string& actorId,
                                    uint32_t ownerId)
{
    const auto& cfg = RagdollModeConfig::instance().data();
    if (!cfg.enabled) return;
    if (!RagdollDeathConfig::instance().enabled()) return;
    if (victim.physicalBody.parts.empty() || victim.physicalBody.partMeshes.empty())
        return;

    RagdollCorpse corpse;
    corpse.actorId = actorId.empty() ? victim.username : actorId;
    corpse.ownerId = ownerId;
    corpse.actor = victim;
    corpse.actor.dead = true;
    corpse.actor.netPredictedDead = false;
    corpse.actor.currentHp = 0;
    // Mark the clone as ragdoll-driven so the render path keeps the physics
    // pose instead of hiding the dead body or re-running procedural animation.
    corpse.actor.ragdollModeActive = true;
    corpse.actor.deathAnim = Player::DeathAnimState{};
    corpse.lifetime = std::max(1.0f, cfg.corpseLifetimeSeconds);
    corpse.age = 0.0f;
    corpse.fade = 0.0f;
    corpse.bloodTimer = 0.0f;
    corpse.bloodInit = false;

    // Build the physical parts from the victim's current (frozen) pose.
    initParts(victim, corpse.body);

    // Inherit the dying actor's momentum and add the killing-blow impulse.
    const glm::vec3 playerVel = victim.vel + victim.externalImpulse;
    const glm::vec3 impulse = deathImpulse * cfg.corpseDeathImpulseMultiplier;
    for (auto& part : corpse.body.parts) {
        part.body.linearVelocity =
            playerVel * cfg.corpseSpawnVelocityMultiplier +
            impulse / std::max(part.body.mass, 0.001f);
        part.body.angularVelocity += glm::vec3(
            ((float)(std::rand() % 2001) / 1000.0f - 1.0f) * 2.0f,
            ((float)(std::rand() % 2001) / 1000.0f - 1.0f) * 2.0f,
            ((float)(std::rand() % 2001) / 1000.0f - 1.0f) * 2.0f);
    }

    // Write the initial skeleton so the clone renders at its physics pose.
    syncToPlayer(corpse.actor, corpse.body);

    // Death mist at the point of death (was the DeathGhost ellipsoid).
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    if (deCfg.enabled) {
        glm::vec3 dir = glm::length(deathImpulse) > 0.001f
            ? glm::normalize(deathImpulse) : glm::vec3(0.0f, 0.0f, -1.0f);
        EffectPartSystem::instance().spawnDeathEllipsoid(
            victim.pos, dir, deCfg.length, deCfg.radius, deCfg.lifetime,
            victim.sizeScale);
    }

    // Bounded corpse count keeps the solver cost predictable.
    constexpr size_t kMaxCorpses = 12;
    while (mCorpses.size() >= kMaxCorpses)
        mCorpses.erase(mCorpses.begin());

    mCorpses.push_back(std::move(corpse));

    Debug::log(Debug::Category::Ragdoll,
        "[RAGDOLL CORPSE] spawned actor=%s id=%u parts=%zu lifetime=%.1fs\n",
        corpse.actorId.c_str(), corpse.ownerId,
        mCorpses.back().body.parts.size(), cfg.corpseLifetimeSeconds);
}

void RagdollModeSystem::stepBody(RagdollBody& b, const World& world, float dt)
{
    const auto& cfg = RagdollModeConfig::instance().data();

    glm::vec3 gravity(0.0f, 0.0f, -GRAVITY * cfg.gravityScale);
    for (auto& part : b.parts)
        integrate(part.body, gravity, dt);

    solveJoints(cfg.solverIterations, true, b);

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            collideWithWorld(part.body, world, dt);
    }

    if (cfg.selfCollision)
        selfCollision(b);

    solveJoints(cfg.solverIterations / 2, true, b);

    if (cfg.selfCollision) {
        for (int i = 0; i < cfg.selfCollisionIterations; ++i) selfCollision(b);
    }

    if (cfg.worldCollision) {
        for (auto& part : b.parts)
            depenetrateWorld(part.body, world, 2);
    }

    for (auto& part : b.parts) {
        if (part.body.stopLinearSpeed > 0.0f &&
            glm::length(part.body.linearVelocity) < part.body.stopLinearSpeed)
            part.body.linearVelocity = glm::vec3(0.0f);
        if (part.body.stopAngularSpeed > 0.0f &&
            glm::length(part.body.angularVelocity) < part.body.stopAngularSpeed)
            part.body.angularVelocity = glm::vec3(0.0f);
    }

    solveRotationLimits(1.0f, b);
}

void RagdollModeSystem::sprayCorpseBlood(RagdollCorpse& corpse, float dt)
{
    RagdollBody& b = corpse.body;
    if (b.parts.empty()) return;

    corpse.bloodTimer -= dt;
    if (corpse.bloodTimer > 0.0f) return;
    corpse.bloodTimer = std::max(0.02f,
        RagdollModeConfig::instance().data().corpseBloodIntervalSeconds);

    // Spray from the fastest-moving part (the body pouring blood as it flies
    // and tumbles). If everything has settled, bleed from the torso root once.
    int fastest = b.torsoIndex >= 0 ? b.torsoIndex : 0;
    float bestSpeed = 0.0f;
    for (int i = 0; i < (int)b.parts.size(); ++i) {
        const float s = glm::length(b.parts[i].body.linearVelocity);
        if (s > bestSpeed) { bestSpeed = s; fastest = i; }
    }

    const RagdollModePart& part = b.parts[fastest];
    const glm::vec3 pos = part.body.position;
    glm::vec3 dir = glm::length(part.body.linearVelocity) > 0.05f
        ? glm::normalize(part.body.linearVelocity)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    const float damage = glm::clamp(bestSpeed * 6.0f, 5.0f, 80.0f);
    EffectPartSystem::instance().spawnBloodEffect(
        pos, dir, damage, corpse.actorId, corpse.actorId, 0.8f, -1.0f);
}

void RagdollModeSystem::updateCorpses(float dt, const World& world)
{
    if (mCorpses.empty()) return;
    const auto& cfg = RagdollModeConfig::instance().data();

    for (auto it = mCorpses.begin(); it != mCorpses.end();) {
        RagdollCorpse& corpse = *it;
        corpse.age += dt;
        if (corpse.age >= corpse.lifetime) {
            it = mCorpses.erase(it);
            continue;
        }

        const float fadeStart = std::max(0.0f, corpse.lifetime - cfg.corpseFadeSeconds);
        if (corpse.age > fadeStart && cfg.corpseFadeSeconds > 0.0f) {
            corpse.fade = glm::clamp(
                (corpse.age - fadeStart) / cfg.corpseFadeSeconds, 0.0f, 1.0f);
        }

        stepBody(corpse.body, world, dt);
        syncToPlayer(corpse.actor, corpse.body);

        if (cfg.corpseBloodEnabled)
            sprayCorpseBlood(corpse, dt);

        ++it;
    }
}

void RagdollModeSystem::renderCorpses(const Camera& camera) const
{
    for (const RagdollCorpse& corpse : mCorpses) {
        if (corpse.fade >= 1.0f) continue;
        renderNetworkPlayer(corpse.actor, camera, 0, false);
    }
}

void RagdollModeSystem::removeCorpsesForOwner(uint32_t ownerId)
{
    if (ownerId == 0) return;
    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(),
            [ownerId](const RagdollCorpse& corpse) {
                return corpse.ownerId == ownerId;
            }),
        mCorpses.end());
}

void RagdollModeSystem::clearCorpses()
{
    mCorpses.clear();
}
