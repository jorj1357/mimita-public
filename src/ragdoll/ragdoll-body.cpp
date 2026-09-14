// 09 12 2026
/* purpose
* Implements shared ragdoll body construction and skeleton write-back.
* Does NOT own the solver, input, networking, or the domain clock.
*/
#include "ragdoll/ragdoll-body.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "debug/debug-log.h"
#include "entities/player.h"
#include "live-code/live-behavior.h"
#include "physics/physical-body.h"
#include "physics/physics-types.h"
#include "ragdoll/ragdoll-mode-config.h"

namespace Ragdoll {

namespace {

int findPartByName(const std::vector<RagdollModePart>& parts, const std::string& name)
{
    for (int i = 0; i < (int)parts.size(); ++i)
        if (parts[i].name == name) return i;
    return -1;
}

std::uint32_t ragdollBindIdForName(const std::string& name)
{
    if (name == "torso") return RAGDOLL_BIND_TORSO;
    if (name == "head") return RAGDOLL_BIND_HEAD;
    if (name == "leftArm") return RAGDOLL_BIND_LEFT_ARM;
    if (name == "rightArm") return RAGDOLL_BIND_RIGHT_ARM;
    if (name == "leftLeg") return RAGDOLL_BIND_LEFT_LEG;
    if (name == "rightLeg") return RAGDOLL_BIND_RIGHT_LEG;
    return 0xffffffffu;
}

} // namespace

glm::mat4 rigidWorld(const RigidBody& body)
{
    return glm::translate(glm::mat4(1.0f), body.position)
         * glm::mat4_cast(body.orientation);
}

void buildBody(const Player& player, const RagdollModeConfigData& cfg, RagdollBody& b)
{
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

        // Node-local -> canonical body frame linear map. Uses the full linear
        // part (scale and reflection included) so a mirrored avatar node
        // (det < 0, e.g. a right leg authored with scale.x = -1) expresses its
        // capsule on the correct side. Quat extraction from an improper matrix
        // is undefined; the matrix is the correct primitive here.
        const glm::mat3 canonicalBasis = glm::mat3_cast(canonicalRot);
        const glm::mat3 nodeToBody = glm::transpose(canonicalBasis) * glm::mat3(nodeWorld);

        // Derive the capsule from the mesh collider bounds (node-local space),
        // then express it in the canonical body frame.
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
        capsuleCenter = nodeToBody * capsuleCenter;
        axis = nodeToBody * axis;
        if (glm::length(axis) > 1e-5f)
            axis = glm::normalize(axis);
        else
            axis = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 comOffset(0.0f);
        const RagdollModeCapsuleConfig* capCfg = nullptr;
        auto capIt = cfg.capsules.find(d.name);
        if (capIt != cfg.capsules.end()) {
            capCfg = &capIt->second;
            if (capCfg->radius > 0.0f) radius = capCfg->radius;
            if (capCfg->halfHeight >= 0.0f) halfHeight = capCfg->halfHeight;
            capsuleCenter += capCfg->offset;
            if (capCfg->hasAxis) axis = capCfg->axis;
            comOffset = capCfg->centerOfMass;
        }

        // The physics center of mass sits at the capsule center plus the
        // editable per-part COM offset. This makes a limb hang from its
        // attachment instead of behaving like an inverted pendulum.
        glm::vec3 comLocal = capsuleCenter + comOffset;

        // Hot bind policy seam: the kernel fills the corrected base frame and
        // the behavior may override the outputs. Mirroring/scaling policy is
        // therefore editable live without relinking the EXE.
        {
            RagdollBindPartV1 bind{};
            bind.partId = ragdollBindIdForName(d.name);
            bind.hasCollider = (col.localMax.x >= col.localMin.x ||
                                col.localMax.y >= col.localMin.y ||
                                col.localMax.z >= col.localMin.z) ? 1u : 0u;
            for (int k = 0; k < 3; ++k) {
                bind.nodePosition[k] = nodePos[k];
                bind.outComLocal[k] = comLocal[k];
                bind.outCapsuleCenter[k] = -comOffset[k];
                bind.outCapsuleAxis[k] = axis[k];
            }
            for (int c = 0; c < 3; ++c)
                for (int r = 0; r < 3; ++r)
                    bind.nodeLinear[c * 3 + r] = nodeWorld[c][r];
            for (int k = 0; k < 3; ++k) {
                bind.colliderMin[k] = col.localMin[k];
                bind.colliderMax[k] = col.localMax[k];
                bind.cfgOffset[k] = capCfg ? capCfg->offset[k] : 0.0f;
                bind.cfgCenterOfMass[k] = capCfg ? capCfg->centerOfMass[k] : 0.0f;
                bind.cfgAxis[k] = capCfg ? capCfg->axis[k] : 0.0f;
            }
            bind.cfgRadius = capCfg ? capCfg->radius : -1.0f;
            bind.cfgHalfHeight = capCfg ? capCfg->halfHeight : -1.0f;
            bind.hasCfgAxis = capCfg && capCfg->hasAxis ? 1u : 0u;
            bind.outRadius = radius;
            bind.outHalfHeight = halfHeight;
            LiveBehavior::dispatchPayload(GAME_EVENT_RAGDOLL_BIND, &bind, sizeof(bind),
                                          player.movementSimulationTick, 0, 0, 0);
            for (int k = 0; k < 3; ++k) {
                comLocal[k] = bind.outComLocal[k];
                axis[k] = bind.outCapsuleAxis[k];
            }
            radius = bind.outRadius;
            halfHeight = bind.outHalfHeight;
            part.body.capsuleCenter = glm::vec3(bind.outCapsuleCenter[0],
                                                 bind.outCapsuleCenter[1],
                                                 bind.outCapsuleCenter[2]);
        }

        part.body.position = nodePos + canonicalRot * comLocal;
        part.body.orientation = canonicalRot;
        part.body.capsuleRadius = radius;
        part.body.capsuleHalfHeight = halfHeight;
        part.body.localAxis = axis;

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

void applyBodyToPlayer(Player& player, RagdollBody& b, const RagdollModeConfigData& cfg)
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
        if (anc >= 0 && anc < (int)player.perfectPoseSkeleton.nodes.size())
            player.perfectPoseSkeleton.nodes[anc].localTransform = glm::mat4(1.0f);

    const float smoothing = cfg.bodySmoothing;
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
        if (!part.renderSmoothed || smoothing <= 0.0f) {
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

} // namespace Ragdoll
