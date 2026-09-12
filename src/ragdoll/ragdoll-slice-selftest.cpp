// 09 12 2026
/* purpose
* Implements the ragdoll slice self-test.
* Does NOT run the solver or touch the running world.
*/
#include "ragdoll/ragdoll-slice-selftest.h"

#include "ecs/entity-registry.h"
#include "hot-reload/hot-reload-system.h"
#include "ragdoll/ragdoll-entities.h"
#include "ragdoll/ragdoll-mode.h"

#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

RagdollBody makeBody()
{
    RagdollBody body;
    body.parts.resize(3);
    const char* names[3] = {"torso", "left_arm", "right_arm"};
    for (int i = 0; i < 3; ++i) {
        RagdollModePart& part = body.parts[i];
        part.name = names[i];
        part.parentIndex = i == 0 ? -1 : 0;
        part.restLength = 0.4f;
        part.maxStretch = 0.1f;
        part.body.position = glm::vec3((float)i, 0.0f, 0.0f);
        part.body.mass = 1.0f;
        part.body.invMass = 1.0f;
        part.body.capsuleRadius = 0.15f;
        part.body.capsuleHalfHeight = 0.2f;
    }
    body.torsoIndex = 0;
    return body;
}

} // namespace

bool runRagdollSliceSelfTest(std::string& report)
{
    bool ok = true;
    EntityRegistry::instance().destroyAll();
    Ragdoll::RagdollEntities& entities = Ragdoll::RagdollEntities::instance();
    entities.unbind(7);

    const RagdollBody body = makeBody();
    entities.bind(7, body);

    ok &= check(entities.limbCount(7) == 3, "limbs become entities", report);
    const EntityId leftArm = entities.limbEntity(7, 1);
    const EntityId torso = entities.limbEntity(7, 0);
    ok &= check(leftArm != kInvalidEntityId && torso != kInvalidEntityId &&
                    leftArm != torso,
                "limb entity ids are distinct", report);
    ok &= check(entities.limbEntity(7, 1) == leftArm, "limb entity id is stable", report);

    ok &= check(EntityRegistry::instance().has<Ragdoll::LimbComponent>(leftArm) &&
                    EntityRegistry::instance().has<Ragdoll::JointComponent>(leftArm),
                "limb and joint components present", report);
    const auto* joint = EntityRegistry::instance().tryGet<Ragdoll::JointComponent>(leftArm);
    ok &= check(joint && joint->parentLimb == 0, "joint links to its parent limb", report);

    RagdollGrabState grab;
    grab.active = true;
    grab.handPosition = glm::vec3(1.0f, 1.0f, 1.0f);
    entities.setGrab(7, true, grab);
    const EntityId leftGrab = EntityRegistry::instance().find(
        EntityRealm::Server, EntityDomain::RagdollLimb, (7u << 8) | 0xfeu);
    const auto* grabComponent = EntityRegistry::instance().tryGet<Ragdoll::GrabComponent>(
        leftGrab);
    ok &= check(grabComponent && grabComponent->active && grabComponent->hand == 0,
                "grab state stored as a component", report);

    Ragdoll::RagdollEntities::SolveParams params;
    params.stiffness = 0.75f;
    params.damping = 0.35f;
    params.iterations = 18;
    params.gravityScale = 1.2f;
    entities.setSolveParams(7, params);
    const Ragdoll::RagdollEntities::SolveParams read = entities.solveParams(7);
    ok &= check(read.stiffness == params.stiffness && read.iterations == params.iterations,
                "solver policy read back (hot dispatch)", report);

    // Snapshot round trip.
    Ragdoll::Snapshot snapshot;
    ok &= check(entities.writeSnapshot(7, snapshot) && snapshot.limbCount == 3,
                "snapshot written with all limbs", report);
    snapshot.limbs[1].position[0] = 42.0f;
    ok &= check(entities.applySnapshot(snapshot), "snapshot applied", report);
    const auto* limb = EntityRegistry::instance().tryGet<Ragdoll::LimbComponent>(leftArm);
    ok &= check(limb && limb->position.x == 42.0f, "applied transform reached the component",
                report);

    // syncToBody must load the canonical component state into a solve workspace.
    RagdollBody workspace = makeBody();
    entities.syncToBody(7, workspace);
    ok &= check(workspace.parts.size() == 3 && workspace.parts[1].body.position.x == 42.0f,
                "syncToBody loads canonical limb state", report);

    // Entity-to-entity grab target resolves to a limb entity and survives the
    // snapshot codec.
    ok &= check(entities.limbEntity(7, 1) == leftArm, "limb id stable before grab target", report);
    entities.setGrabTarget(7, true, 2, 0.5f);
    const auto* targeted = EntityRegistry::instance().tryGet<Ragdoll::GrabComponent>(
        EntityRegistry::instance().find(
            EntityRealm::Server, EntityDomain::RagdollLimb, (7u << 8) | 0xfeu));
    const EntityId rightArm = entities.limbEntity(7, 2);
    ok &= check(targeted && targeted->targetEntity == (std::uint32_t)rightArm &&
                    targeted->strength == 0.5f,
                "entity-to-entity grab resolves target limb", report);
    Ragdoll::Snapshot grabSnapshot;
    entities.writeSnapshot(7, grabSnapshot);
    ok &= check(grabSnapshot.grabs[0].active && grabSnapshot.grabs[0].targetLimb == 2u &&
                    grabSnapshot.grabs[0].strength == 0.5f,
                "grab constraint serialized in snapshot", report);

    // Remote reconstruction: an unknown owner gains a limb set from the first
    // snapshot, and the entity identities stay stable across later snapshots.
    Ragdoll::Snapshot remote;
    remote.ownerActorId = 99;
    remote.limbCount = 4;
    for (int i = 0; i < 4; ++i)
        remote.limbs[i].position[0] = 100.0f + i;
    ok &= check(entities.applySnapshot(remote) && entities.bound(99) &&
                    entities.limbCount(99) == 4,
                "first remote snapshot reconstructs the limb set", report);
    const EntityId remote0 = entities.limbEntity(99, 0);
    remote.limbs[0].position[0] = 123.0f;
    entities.reconstructFromSnapshot(remote);
    const auto* remoteLimb = EntityRegistry::instance().tryGet<Ragdoll::LimbComponent>(remote0);
    ok &= check(entities.limbEntity(99, 0) == remote0 && remoteLimb &&
                    remoteLimb->position.x == 123.0f,
                "remote reconstruction reuses stable entity ids", report);

    // Replicated death identity round trip (corpse seed input).
    std::uint32_t rTick = 0, rEvent = 0;
    ok &= check(RagdollModeSystem::instance().noteNetworkDeath(55, 7, 9) &&
                    RagdollModeSystem::instance().consumeNetworkDeath(55, rTick, rEvent) &&
                    rTick == 7 && rEvent == 9,
                "network death identity round trips", report);
    ok &= check(!RagdollModeSystem::instance().consumeNetworkDeath(55, rTick, rEvent),
                "network death identity consumed once", report);

    // Identity survives a hot-code reload cycle.
    HotReloadSystem::instance().startup();
    HotReloadSystem::instance().unloadGameDLL();
    HotReloadSystem::instance().startup();
    ok &= check(entities.limbEntity(7, 1) == leftArm &&
                    EntityRegistry::instance().alive(leftArm),
                "ragdoll entity ids survive DLL reload", report);
    HotReloadSystem::instance().unloadGameDLL();

    entities.unbind(7);
    EntityRegistry::instance().destroyAll();
    return ok;
}
