// 09 12 2026
/* purpose
* Implements the ragdoll entity/component projection and snapshot codec.
* Does NOT own the solver math or rendering.
*/
#include "ragdoll/ragdoll-entities.h"

#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "live-code/live-behavior.h"
#include "ragdoll/ragdoll-mode.h"

namespace Ragdoll {

namespace {

constexpr std::uint32_t kRootSlot = 0xffu;
constexpr std::uint32_t kLeftGrabSlot = 0xfeu;
constexpr std::uint32_t kRightGrabSlot = 0xfdu;

std::uint32_t legacyFor(std::uint32_t owner, std::uint32_t slot)
{
    return (owner << 8) | (slot & 0xffu);
}

} // namespace

RagdollEntities& RagdollEntities::instance()
{
    static RagdollEntities entities;
    return entities;
}

void RagdollEntities::unbind(std::uint32_t ownerActorId)
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    EntityRegistry& registry = EntityRegistry::instance();
    for (EntityId id : it->second.limbs)
        registry.destroy(id);
    registry.destroy(it->second.root);
    registry.destroy(it->second.leftGrab);
    registry.destroy(it->second.rightGrab);
    bindings_.erase(it);
}

bool RagdollEntities::bound(std::uint32_t ownerActorId) const
{
    return bindings_.find(ownerActorId) != bindings_.end();
}

void RagdollEntities::bind(std::uint32_t ownerActorId, const RagdollBody& body)
{
    auto existing = bindings_.find(ownerActorId);
    if (existing != bindings_.end() &&
        existing->second.limbs.size() == body.parts.size())
        return;
    unbind(ownerActorId);

    EntityRegistry& registry = EntityRegistry::instance();
    BodyBinding binding;
    binding.limbs.reserve(body.parts.size());

    for (std::uint32_t i = 0; i < (std::uint32_t)body.parts.size(); ++i) {
        const RagdollModePart& part = body.parts[i];
        EntityId id = Ecs::ensure(EntityRealm::Server, EntityDomain::RagdollLimb,
                                  legacyFor(ownerActorId, i));
        LimbComponent limb;
        limb.limbIndex = i;
        limb.parentIndex = part.parentIndex < 0 ? kInvalidLimb : (std::uint32_t)part.parentIndex;
        limb.position = part.body.position;
        limb.orientation = part.body.orientation;
        limb.linearVelocity = part.body.linearVelocity;
        limb.angularVelocity = part.body.angularVelocity;
        limb.mass = part.body.mass;
        limb.inverseMass = part.body.invMass;
        limb.radius = part.body.capsuleRadius;
        limb.halfHeight = part.body.capsuleHalfHeight;
        registry.add<LimbComponent>(id, limb);

        JointComponent joint;
        joint.limbIndex = i;
        joint.parentLimb = limb.parentIndex;
        joint.parentLocalAnchor = part.parentLocalAnchor;
        joint.childLocalAnchor = part.childLocalAnchor;
        joint.restLength = part.restLength;
        joint.maxStretch = part.maxStretch;
        registry.add<JointComponent>(id, joint);

        binding.limbs.push_back(id);
    }

    binding.root = Ecs::ensure(EntityRealm::Server, EntityDomain::RagdollLimb,
                               legacyFor(ownerActorId, kRootSlot));
    RagdollRootComponent root;
    root.ownerActorId = ownerActorId;
    root.limbCount = (std::uint32_t)binding.limbs.size();
    registry.add<RagdollRootComponent>(binding.root, root);

    binding.leftGrab = Ecs::ensure(EntityRealm::Server, EntityDomain::RagdollLimb,
                                   legacyFor(ownerActorId, kLeftGrabSlot));
    binding.rightGrab = Ecs::ensure(EntityRealm::Server, EntityDomain::RagdollLimb,
                                    legacyFor(ownerActorId, kRightGrabSlot));
    registry.add<GrabComponent>(binding.leftGrab, GrabComponent{});
    registry.add<GrabComponent>(binding.rightGrab, GrabComponent{});

    bindings_[ownerActorId] = std::move(binding);
}

void RagdollEntities::bindLimbCount(std::uint32_t ownerActorId, std::uint32_t limbCount)
{
    auto existing = bindings_.find(ownerActorId);
    if (existing != bindings_.end() &&
        existing->second.limbs.size() == limbCount)
        return;

    // Build a minimal synthetic body so the shared bind path creates stable
    // limb entities for a remote owner. Parent structure is unknown until the
    // local simulation owns the ragdoll; joints stay unlinked (identity).
    RagdollBody body;
    body.parts.resize(limbCount);
    for (std::uint32_t i = 0; i < limbCount; ++i) {
        RagdollModePart& part = body.parts[i];
        part.name = "remote_limb_" + std::to_string(i);
        part.parentIndex = -1;
        part.body.position = glm::vec3(0.0f);
    }
    bind(ownerActorId, body);
}

EntityId RagdollEntities::limbEntity(std::uint32_t ownerActorId,
                                     std::uint32_t limbIndex) const
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end() || limbIndex >= it->second.limbs.size())
        return kInvalidEntityId;
    return it->second.limbs[limbIndex];
}

std::uint32_t RagdollEntities::limbCount(std::uint32_t ownerActorId) const
{
    auto it = bindings_.find(ownerActorId);
    return it == bindings_.end() ? 0u : (std::uint32_t)it->second.limbs.size();
}

void RagdollEntities::syncFromBody(std::uint32_t ownerActorId, const RagdollBody& body)
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    EntityRegistry& registry = EntityRegistry::instance();
    const std::size_t count = std::min(it->second.limbs.size(), body.parts.size());
    for (std::size_t i = 0; i < count; ++i) {
        auto* limb = registry.tryGet<LimbComponent>(it->second.limbs[i]);
        if (!limb)
            continue;
        const RigidBody& rb = body.parts[i].body;
        limb->position = rb.position;
        limb->orientation = rb.orientation;
        limb->linearVelocity = rb.linearVelocity;
        limb->angularVelocity = rb.angularVelocity;
    }
    if (auto* root = registry.tryGet<RagdollRootComponent>(it->second.root)) {
        root->limbCount = (std::uint32_t)it->second.limbs.size();
        root->lastSolveTick = root->lastSolveTick;
    }
}

void RagdollEntities::setGrab(std::uint32_t ownerActorId, bool left,
                              const RagdollGrabState& grab)
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    GrabComponent component;
    component.active = grab.active;
    component.wasActive = grab.wasActive;
    component.hand = left ? 0u : 1u;
    component.grabPoint = grab.grabPoint;
    component.grabNormal = grab.grabNormal;
    component.handPosition = grab.handPosition;
    component.handLocalAnchor = grab.handLocalAnchor;
    component.strength = grab.strength;
    if (grab.targetPart >= 0 && grab.targetPart < (int)it->second.limbs.size())
        component.targetEntity = (std::uint32_t)it->second.limbs[grab.targetPart];
    EntityRegistry::instance().add<GrabComponent>(
        left ? it->second.leftGrab : it->second.rightGrab, component);
}

void RagdollEntities::setGrabTarget(std::uint32_t ownerActorId, bool left,
                                    std::int32_t targetLimbIndex, float strength)
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    EntityId grabEntity = left ? it->second.leftGrab : it->second.rightGrab;
    auto* component = EntityRegistry::instance().tryGet<GrabComponent>(grabEntity);
    if (!component)
        return;
    if (targetLimbIndex >= 0 && (std::size_t)targetLimbIndex < it->second.limbs.size())
        component->targetEntity = (std::uint32_t)it->second.limbs[(std::size_t)targetLimbIndex];
    else
        component->targetEntity = kInvalidLimb;
    component->strength = strength;
}

void RagdollEntities::syncToBody(std::uint32_t ownerActorId, RagdollBody& body) const
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    EntityRegistry& registry = EntityRegistry::instance();
    const std::size_t count = std::min(it->second.limbs.size(), body.parts.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto* limb = registry.tryGet<LimbComponent>(it->second.limbs[i]);
        if (!limb)
            continue;
        RigidBody& rb = body.parts[i].body;
        rb.position = limb->position;
        rb.orientation = limb->orientation;
        rb.linearVelocity = limb->linearVelocity;
        rb.angularVelocity = limb->angularVelocity;
    }
}

RagdollEntities::SolveParams RagdollEntities::solveParams(std::uint32_t ownerActorId) const
{
    SolveParams params;
    auto it = bindings_.find(ownerActorId);
    if (it != bindings_.end()) {
        if (const auto* root = EntityRegistry::instance().tryGet<RagdollRootComponent>(
                it->second.root)) {
            params.stiffness = root->stiffness;
            params.damping = root->damping;
            params.iterations = (int)root->solverIterations;
            params.gravityScale = root->gravityScale;
        }
    }

    RagdollPolicyV1 policy{};
    policy.ownerActor = ownerActorId;
    policy.limbCount = limbCount(ownerActorId);
    policy.baseStiffness = params.stiffness;
    policy.baseDamping = params.damping;
    policy.baseIterations = (std::uint32_t)params.iterations;
    policy.baseGravityScale = params.gravityScale;
    policy.outStiffness = params.stiffness;
    policy.outDamping = params.damping;
    policy.outIterations = (std::uint32_t)params.iterations;
    policy.outGravityScale = params.gravityScale;
    if (LiveBehavior::dispatchRagdollPolicy(policy, 0)) {
        params.stiffness = policy.outStiffness;
        params.damping = policy.outDamping;
        params.iterations = (int)policy.outIterations;
        params.gravityScale = policy.outGravityScale;
    }
    return params;
}

void RagdollEntities::setSolveParams(std::uint32_t ownerActorId,
                                     const SolveParams& params)
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return;
    if (auto* root = EntityRegistry::instance().tryGet<RagdollRootComponent>(
            it->second.root)) {
        root->stiffness = params.stiffness;
        root->damping = params.damping;
        root->solverIterations = (std::uint32_t)params.iterations;
        root->gravityScale = params.gravityScale;
    }
}

std::size_t RagdollEntities::entityCount() const
{
    std::size_t count = 0;
    for (const auto& entry : bindings_)
        count += entry.second.limbs.size() + 3;
    return count;
}

bool RagdollEntities::writeSnapshot(std::uint32_t ownerActorId, Snapshot& out) const
{
    auto it = bindings_.find(ownerActorId);
    if (it == bindings_.end())
        return false;
    EntityRegistry& registry = EntityRegistry::instance();
    out = Snapshot{};
    out.ownerActorId = ownerActorId;
    const std::size_t count = std::min(it->second.limbs.size(),
                                       (std::size_t)kMaxSnapshotLimbs);
    out.limbCount = (std::uint32_t)count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* limb = registry.tryGet<LimbComponent>(it->second.limbs[i]);
        if (!limb)
            continue;
        out.limbs[i].limbIndex = (std::uint32_t)i;
        out.limbs[i].position[0] = limb->position.x;
        out.limbs[i].position[1] = limb->position.y;
        out.limbs[i].position[2] = limb->position.z;
        out.limbs[i].rotation[0] = limb->orientation.w;
        out.limbs[i].rotation[1] = limb->orientation.x;
        out.limbs[i].rotation[2] = limb->orientation.y;
        out.limbs[i].rotation[3] = limb->orientation.z;
    }

    for (int h = 0; h < 2; ++h) {
        const EntityId grabEntity = h == 0 ? it->second.leftGrab : it->second.rightGrab;
        const auto* grab = registry.tryGet<GrabComponent>(grabEntity);
        GrabSnapshot& gs = out.grabs[h];
        gs = GrabSnapshot{};
        gs.hand = (std::uint8_t)h;
        gs.targetLimb = kInvalidLimb;
        if (!grab)
            continue;
        gs.active = grab->active ? 1u : 0u;
        gs.strength = grab->strength;
        for (int k = 0; k < 3; ++k) {
            gs.anchor[k] = grab->grabPoint[k];
            gs.handLocal[k] = grab->handLocalAnchor[k];
        }
        if (grab->targetEntity != kInvalidLimb) {
            for (std::size_t i = 0; i < it->second.limbs.size(); ++i) {
                if ((std::uint32_t)it->second.limbs[i] != grab->targetEntity)
                    continue;
                gs.targetLimb = (std::uint32_t)i;
                if (const auto* target = registry.tryGet<LimbComponent>(grab->targetEntity)) {
                    gs.anchor[0] = target->position.x;
                    gs.anchor[1] = target->position.y;
                    gs.anchor[2] = target->position.z;
                }
                break;
            }
        }
    }
    return true;
}

bool RagdollEntities::applySnapshot(const Snapshot& snapshot)
{
    if (bindings_.find(snapshot.ownerActorId) == bindings_.end())
        bindLimbCount(snapshot.ownerActorId, snapshot.limbCount);
    auto it = bindings_.find(snapshot.ownerActorId);
    if (it == bindings_.end())
        return false;
    EntityRegistry& registry = EntityRegistry::instance();
    const std::size_t count = std::min((std::size_t)snapshot.limbCount,
                                       it->second.limbs.size());
    for (std::size_t i = 0; i < count; ++i) {
        auto* limb = registry.tryGet<LimbComponent>(it->second.limbs[i]);
        if (!limb)
            continue;
        limb->position = glm::vec3(snapshot.limbs[i].position[0],
                                   snapshot.limbs[i].position[1],
                                   snapshot.limbs[i].position[2]);
        limb->orientation = glm::quat(snapshot.limbs[i].rotation[0],
                                      snapshot.limbs[i].rotation[1],
                                      snapshot.limbs[i].rotation[2],
                                      snapshot.limbs[i].rotation[3]);
    }

    for (int h = 0; h < 2; ++h) {
        const EntityId grabEntity = h == 0 ? it->second.leftGrab : it->second.rightGrab;
        auto grab = registry.tryGet<GrabComponent>(grabEntity);
        if (!grab)
            continue;
        const GrabSnapshot& gs = snapshot.grabs[h];
        grab->active = gs.active != 0;
        grab->hand = (std::uint32_t)h;
        grab->grabPoint = glm::vec3(gs.anchor[0], gs.anchor[1], gs.anchor[2]);
        grab->handLocalAnchor = glm::vec3(gs.handLocal[0], gs.handLocal[1], gs.handLocal[2]);
        grab->strength = gs.strength;
        if (gs.targetLimb != kInvalidLimb && gs.targetLimb < it->second.limbs.size())
            grab->targetEntity = (std::uint32_t)it->second.limbs[gs.targetLimb];
        else
            grab->targetEntity = kInvalidLimb;
    }
    return true;
}

} // namespace Ragdoll
