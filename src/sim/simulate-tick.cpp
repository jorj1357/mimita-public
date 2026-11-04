// 07 21 2026, 16 30
/* purpose
* Runs one fixed gameplay simulation tick for local Player, NPC, death, and void systems.
* Converts an InputFrame into physics input before calling the movement orchestrator.
* Keeps rendering, networking transport, and variable frame timing outside fixed simulation.
* Does NOT own movement formulas, collision internals, packet layout, or weapon authority.
* Does NOT run the main loop, load maps, serialize replay files, or allocate servers.
* Does NOT replace subsystem-owned update functions for NPC, combat, physics, or effects.
*/

#include "sim/sim-context.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "input/input-poll.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "input/input-state.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/actor-final-pose.h"
#include "physics/movement/actor-pair-collision.h"
#include "config/actor-collision-config.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "entities/player-avatar-animations.h"
#include "entities/aimbody-config.h"
#include "world/world.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "combat/weapon-hit.h"
#include "combat/death-system.h"
#include "combat/weapon-contact-local.h"
#include "combat/weapon-registry.h"
#include "combat/melee-client.h"
#include "effects/hit-effects.h"
#include "void-death/void-death.h"
#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "network/multiplayer-context.h"
#include "network/community-match-client.h"
#include "game/objective-state.h"
#include "gamemode/map-config.h"
#include "network/community-match-client.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-system.h"

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static InputState inputStateFromFrame(const InputFrame& frame)
{
    InputState state;
    state.wishMoveXY = {frame.moveX, frame.moveY};
    state.jumpHeld = frame.jump;
    state.jumpPressed = frame.jumpPressed;
    state.dashPressed = frame.dashPressed;
    state.movementPressed = frame.movementPressed;
    state.movementJustPressed = frame.movementJustPressed;
    state.groundReturnPressed = frame.groundReturnPressed;
    state.downDashPressed = frame.downDashPressed;
    state.freezeHeld = frame.freezeHeld;
    state.freezePressed = frame.freezePressed;
    state.fisterTogglePressed = frame.fisterTogglePressed;
    state.fisterFistHeld = frame.fisterFistHeld;
    state.fisterParryHeld = frame.fisterParryHeld;
    state.fisterGrabHeld = frame.fisterGrabHeld;

    float yawRad = glm::radians(frame.lookYaw);
    float pitchRad = glm::radians(frame.lookPitch);
    state.camForward = glm::vec3(
        std::cos(pitchRad) * std::cos(yawRad),
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad)
    );

    return state;
}

static constexpr float TICK_DT = 1.0f / 60.0f;

static void containAndLogPlayerFinalPose(Player& player, const World& world,
                                         const char* stage)
{
    ActorFinalPoseResult result;
    const bool clear = containActorFinalPose(
        player, world, player.finalPose, result, 0.02f, stage);
    static uint64_t lastLogTick = 0;
    const uint64_t tick = player.movementSimulationTick;
    const bool reset = result.restored || result.usedSafePoint || result.failed;
    const bool penetration = result.initialPenetration > 0.02f;
    if (!reset && !result.budgetExceeded &&
        (!penetration || tick - lastLogTick < 30))
        return;

    nlohmann::json parts = nlohmann::json::array();
    for (const ActorFinalPoseContact& contact : result.contacts)
    {
        parts.push_back({
            {"actor_part", contact.actorPart ? contact.actorPart : ""},
            {"world_triangle", contact.worldTriangle},
            {"point", {contact.point.x, contact.point.y, contact.point.z}},
            {"normal", {contact.normal.x, contact.normal.y, contact.normal.z}},
            {"surface_normal", {contact.surfaceNormal.x,
                                contact.surfaceNormal.y,
                                contact.surfaceNormal.z}},
            {"penetration_m", contact.penetration}});
    }
    if (penetration)
        lastLogTick = tick;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Collision, StructuredLevel::Important,
        reset ? "player.pose.reset" : "player.pose.contained",
        "local-player", stage, static_cast<uint32_t>(tick),
        {{"actor", player.username},
         {"stage", stage},
         {"position", {player.pos.x, player.pos.y, player.pos.z}},
         {"initial_penetration_m", result.initialPenetration},
         {"final_penetration_m", result.finalPenetration},
         {"iterations", result.iterations},
         {"correction", {result.correction.x, result.correction.y,
                          result.correction.z}},
         {"clear", clear},
         {"restored", result.restored},
         {"used_safe_point", result.usedSafePoint},
         {"failed", result.failed},
         {"budget_exceeded", result.budgetExceeded},
         {"reset_reason", result.resetReason},
         {"part_count", result.partCount},
         {"extra_part_count", result.extraPartCount},
         {"skip_reason", result.skipReason},
         {"parts", std::move(parts)}},
        __FILE__, __LINE__, __FUNCTION__);
}

static void containAndLogLocalNpcFinalPose(Npc& npc, const World& world,
                                           const char* stage)
{
    ActorFinalPoseResult result;
    const bool clear = containActorFinalPose(
        npc.body, world, npc.finalPose, result, 0.02f, stage);
    if (clear && !result.corrected && !result.budgetExceeded)
        return;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        result.failed ? "npc.pose.reset" : "npc.pose.contained",
        std::to_string(npc.id), stage,
        static_cast<uint32_t>(npc.body.movementSimulationTick),
        {{"actor", npc.id},
         {"stage", stage},
         {"position", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
         {"initial_penetration_m", result.initialPenetration},
         {"final_penetration_m", result.finalPenetration},
         {"correction", {result.correction.x, result.correction.y,
                          result.correction.z}},
         {"clear", clear},
         {"restored", result.restored},
         {"used_safe_point", result.usedSafePoint},
         {"failed", result.failed},
         {"budget_exceeded", result.budgetExceeded},
         {"reset_reason", result.resetReason},
         {"part_count", result.partCount},
         {"extra_part_count", result.extraPartCount},
         {"skip_reason", result.skipReason}},
        __FILE__, __LINE__, __FUNCTION__);
}

void simulateTick(SimContext& sim, const InputFrame& frame)
{
    MIMITA_PERF_SCOPE("Simulation::SimulateTick");
    if (!sim.player || !sim.world || !sim.npcSystem) return;

    // Recomputed below only while a physical body or ragdoll is running, so a
    // stopped body sends one inactive pose to clear the remote replicas.
    RagdollModeSystem::instance().clearReplicatedPose();

    // Local gameplay must not simulate until the authoritative spawn transform
    // is installed. Otherwise the player visibly falls from the temporary local
    // fallback position and a physical aim body binds against stale state.
    // Networking, map loading, and the spawn handshake continue in engineTickNet.
    auto& mpContext = MP_CONTEXT;
    auto& ragdoll = RagdollModeSystem::instance();
    { MIMITA_PERF_SCOPE("Simulation::Readiness");
      if (mpContext.active &&
          !MimitaNet::mpLocalGameplaySimulationReady(mpContext, *sim.player))
      {
          return;
      } }
    advanceTerminalInteractFixedTick();

    // Install changed avatar animation JSON before any player or NPC samples a
    // pose this tick. This presentation path remains live even when generic
    // config hot reload is disabled.
    { MIMITA_PERF_SCOPE("Simulation::AnimationReload");
      AvatarAnimationRegistry::instance().pollReload(
          (std::uint32_t)sim.player->movementSimulationTick); }
    { MIMITA_PERF_SCOPE("Simulation::CollisionConfigReload");
      ActorCollisionConfig::instance().pollHotReload((std::uint32_t)sim.tick); }

    // Sweep start for this fixed tick. Reconciliation/teleports have already
    // installed the authoritative position, so a discontinuity does not create
    // a false swept contact across the old pose.
    sim.player->actorCollisionPreviousPos = sim.player->pos;
    sim.player->hasActorCollisionPreviousPos = true;
    sim.player->seatedVehicleId =
        VehicleSystem::instance().localVehicleEntityId();

    // One lifecycle reset path: any new authoritative life (spawn, respawn,
    // instant respawn, teleport, duel, map change, reconnect, or transform-epoch
    // change) rebuilds the aim body at the new authoritative pose before it
    // simulates, instead of letting the old body fall and tether across.
    MIMITA_PERF_SCOPE("Simulation::Lifecycle");
    const uint64_t lifeId = MimitaNet::mpLocalLifecycleId(mpContext);
    static uint64_t s_aimLifecycleId = 0;
    const bool lifecycleChanged = lifeId != 0 && lifeId != s_aimLifecycleId;
    // A live ragdoll must not survive an authoritative lifecycle discontinuity
    // (respawn, teleport, map change, reconnect). Its body would otherwise
    // re-assert the pre-discontinuity pose over the freshly applied transform
    // on the next tick. Exit without moving the player so the new authoritative
    // position stands.
    if (lifecycleChanged && ragdoll.isActive()) {
        ragdoll.deactivate(*sim.player, /*movePlayerToBody=*/false);
        sim.player->ragdollModeActive = false;
    }
    if (ragdoll.aimActive() &&
        (lifecycleChanged || ragdoll.aimBindingNeedsRebind(*sim.player)))
        ragdoll.rebindAimToAuthoritativePlayer(*sim.player);
    s_aimLifecycleId = lifeId;

    { MIMITA_PERF_SCOPE("Simulation::LocalActor");
    if (!sim.player->dead) {
        InputFrame tickFrame = frame;
        if (InputCommandSystem::instance().consumeScheduledActionTick("walkforward")) {
            tickFrame.moveY = std::max(tickFrame.moveY, 1.0f);
            tickFrame.movementPressed = true;
            if (StructuredLogger::instance().shouldLog(
                    StructuredCategory::Collision, StructuredLevel::Trace)) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Collision, StructuredLevel::Trace,
                    "scenario.walkforward_tick", "hybrid-weapon-wall-probe",
                    "scheduled walk-forward command applied for one fixed tick",
                    static_cast<uint32_t>(sim.player->movementSimulationTick),
                    {
                        {"move_x", tickFrame.moveX},
                        {"move_y", tickFrame.moveY},
                        {"actor_position", {sim.player->pos.x, sim.player->pos.y, sim.player->pos.z}},
                        {"actor_velocity", {sim.player->vel.x, sim.player->vel.y, sim.player->vel.z}},
                        {"aimbody_mode", AimBodyConfig::instance().mode()}
                    },
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }

        // Handle ragdoll mode toggle
        static bool ragdollTogglePrev = false;
        bool ragdollToggleNow = tickFrame.ragdollTogglePressed;
        if (ragdollToggleNow && !ragdollTogglePrev && RagdollModeConfig::instance().data().enabled) {
            if (ragdoll.isActive()) {
                ragdoll.deactivate(*sim.player);
                sim.player->ragdollModeActive = false;
            } else {
                ragdoll.activate(*sim.player);
                sim.player->ragdollModeActive = true;
            }
        }
        ragdollTogglePrev = ragdollToggleNow;

        if (sim.player->ragdollModeActive && ragdoll.isActive()) {
            if (ragdoll.aimActive())
                ragdoll.deactivateAim(*sim.player);
            MIMITA_PERF_SCOPE("RagdollModeUpdate");
            InputState ragdollInput = inputStateFromFrame(tickFrame);
            ragdollInput.grabLeftHeld = tickFrame.grabLeftHeld;
            ragdollInput.grabRightHeld = tickFrame.grabRightHeld;
            ragdollInput.extendLeftMouse = tickFrame.extendLeftMouse;
            ragdollInput.extendRightMouse = tickFrame.extendRightMouse;
            ragdoll.update(TICK_DT, *sim.world, *sim.player, ragdollInput, THE_CAMERA);
        } else if (VehicleSystem::instance().hasLocalDriver() &&
                   !sim.player->dead && !sim.player->voidDeathTriggered) {
            // Seated actor: the seat owns the player origin and locomotion is
            // suppressed. The physical aim body still runs so weapons can fire.
            glm::vec3 seatPosition(0.0f);
            glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
            if (VehicleSystem::instance().localSeatWorld(seatPosition,
                                                         seatForward))
            {
                const glm::vec3 previousPos = sim.player->pos;
                if (sim.tick % 6 == 0)
                {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Physics, StructuredLevel::Important,
                        "vehicle.seat.anchor", "vehicle-seat-anchor",
                        "seated actor origin is asserted to the seat anchor",
                        static_cast<uint32_t>(sim.tick),
                        {{"actor_previous", {previousPos.x, previousPos.y,
                                             previousPos.z}},
                         {"seat", {seatPosition.x, seatPosition.y,
                                   seatPosition.z}},
                         {"snap_distance",
                          glm::length(previousPos - seatPosition)},
                         {"forward", {seatForward.x, seatForward.y,
                                      seatForward.z}},
                         {"crouching", sim.player->crouching},
                         {"has_local_driver",
                          VehicleSystem::instance().hasLocalDriver()},
                         {"seated_entity_id", sim.player->seatedVehicleId},
                         {"server_position",
                          std::vector<float>{
                              mpContext.localServerPosition.x,
                              mpContext.localServerPosition.y,
                              mpContext.localServerPosition.z}},
                         {"server_minus_previous",
                          std::vector<float>{
                              mpContext.localServerPosition.x - previousPos.x,
                              mpContext.localServerPosition.y - previousPos.y,
                              mpContext.localServerPosition.z - previousPos.z}},
                         {"vehicle_id",
                          VehicleSystem::instance().localVehicleId()}});
                }
                sim.player->pos = seatPosition;
                sim.player->vel = glm::vec3(0.0f);
                sim.player->externalImpulse = glm::vec3(0.0f);
                sim.player->inputWishMove = glm::vec2(0.0f);
                // Reuse the authored crouch clip as the held seated pose. The
                // origin remains the actor's collision root; the seat owner
                // supplies the upward riderOffset.
                sim.player->crouching = true;
                sim.player->ground.onGround = true;
                sim.player->ground.stableOnGround = true;
            }
            InputState seatedInput = inputStateFromFrame(tickFrame);
            sim.player->updateProceduralAnimation(
                TICK_DT, seatedInput.camForward, THE_CAMERA.pos, false);
            const WeaponDefinition* seatedDef = WeaponRegistry::instance().get(
                sim.player->equippedWeaponId);
            const bool seatedArmRagdoll = seatedDef &&
                seatedDef->armRagdoll.enabled &&
                (!seatedDef->armRagdoll.actionScoped ||
                 MimitaMelee::currentActionUsesPhysicalArm(seatedDef->id));
            if ((AimBodyConfig::instance().bodyPhysicsMode() ||
                 seatedArmRagdoll) &&
                (!sim.player->usesMeleeAuthoredPose() || seatedArmRagdoll)) {
                MIMITA_PERF_SCOPE("SeatedAimBodyUpdate");
                if (seatedArmRagdoll) {
                    ragdoll.updateArmRagdoll(
                        TICK_DT, *sim.world, *sim.player, seatedInput.camForward,
                        seatedDef->armRagdoll.actionScoped
                            ? MimitaMelee::currentActionPhysicalDriveActive(seatedDef->id)
                            : frame.primaryHeld,
                        seatedDef->id, seatedDef->armRagdoll);
                } else {
                    if (!ragdoll.aimActive())
                        ragdoll.activateAim(*sim.player);
                    ragdoll.updateAim(TICK_DT, *sim.world, *sim.player,
                                      seatedInput.camForward,
                                      THE_CAMERA.pos,
                                      frame.extendRightMouse);
                }
            } else if (ragdoll.armRagdollActive()) {
                ragdoll.deactivateArmRagdoll(*sim.player);
            } else if (ragdoll.aimActive()) {
                ragdoll.deactivateAim(*sim.player);
            }
        } else {
            MIMITA_PERF_SCOPE("PhysicsMainUpdate");
            InputState normalInput = inputStateFromFrame(tickFrame);
            // Mirror the authoritative hold-mode plant lock locally so the
            // predicted actor does not run ahead of the server. Momentum is
            // preserved because velocity is left untouched.
            bool plantingLock = false;
            if (tickFrame.interactHeld && mpContext.active)
            {
                const auto& objective =
                    MimitaNet::CommunityMatchClient::instance().objective();
                plantingLock =
                    objective.active &&
                    objective.state == (uint8_t)ObjectiveState::Carried &&
                    objective.interactionMode ==
                        (uint8_t)ObjectiveInteractionMode::Hold &&
                    objective.carrierId == mpContext.localPlayerId &&
                    MapConfigRegistry::instance().siteIndexAt(sim.player->pos) >= 0;
            }
            if (plantingLock)
            {
                normalInput.wishMoveXY = glm::vec2(0.0f);
                normalInput.jumpHeld = false;
                normalInput.jumpPressed = false;
                normalInput.dashPressed = false;
                normalInput.downDashPressed = false;
                normalInput.freezeHeld = false;
                normalInput.freezePressed = false;
            }
            setCollisionEntityContext("Player", 0, false);
            physicsMainUpdate(*sim.player, *sim.world, normalInput, TICK_DT);
            clearCollisionEntityContext();

            // Always-on physical aim body (aimbody.json mode == "physical" or
            // "hybrid"). Runs after normal movement/animation so it owns the
            // rendered pose and the client hitboxes for this tick. Melee exclusive
            // authored pose disables it so limb momentum/head tilt cannot alter
            // the authored Captain Falcon pose. Per-weapon arm ragdoll is the
            // exception: it replaces only the right arm and leaves the authored
            // pose on every other limb.
            const WeaponDefinition* equippedDef = WeaponRegistry::instance().get(
                sim.player->equippedWeaponId);
            const bool armRagdoll = equippedDef &&
                equippedDef->armRagdoll.enabled &&
                (!equippedDef->armRagdoll.actionScoped ||
                 MimitaMelee::currentActionUsesPhysicalArm(equippedDef->id));
            if ((AimBodyConfig::instance().bodyPhysicsMode() || armRagdoll) &&
                (!sim.player->usesMeleeAuthoredPose() || armRagdoll)) {
                MIMITA_PERF_SCOPE("AimBodyUpdate");
                if (armRagdoll) {
                    ragdoll.updateArmRagdoll(
                        TICK_DT, *sim.world, *sim.player, normalInput.camForward,
                        equippedDef->armRagdoll.actionScoped
                            ? MimitaMelee::currentActionPhysicalDriveActive(equippedDef->id)
                            : frame.primaryHeld,
                        equippedDef->id,
                        equippedDef->armRagdoll);
                } else {
                    if (!ragdoll.aimActive())
                        ragdoll.activateAim(*sim.player);
                    ragdoll.updateAim(TICK_DT, *sim.world, *sim.player,
                                      normalInput.camForward,
                                      THE_CAMERA.pos,
                                      frame.extendRightMouse);
                }
            } else if (ragdoll.armRagdollActive()) {
                ragdoll.deactivateArmRagdoll(*sim.player);
            } else if (ragdoll.aimActive()) {
                ragdoll.deactivateAim(*sim.player);
            }
        }
    }
    else {
        // Dead actor: movement/collision stay off. Keep the shared animation
        // path running so a non-ragdoll mode can present its authored death
        // clip; ragdoll death presentation remains owned by the ragdoll system.
        if (!ragdoll.isActive())
            sim.player->updateProceduralAnimation(TICK_DT, THE_CAMERA.front,
                                                  THE_CAMERA.pos, false);
    } }

    // The seated actor is contained by the vehicle seat, not by the on-foot
    // capsule. Running world containment here pushes the capsule out of the
    // occupied chassis and produces the repeating 0.7-1.1 m seat snap.
    if (!sim.player->dead && !sim.player->ragdollModeActive &&
        !VehicleSystem::instance().hasLocalDriver())
        containAndLogPlayerFinalPose(*sim.player, *sim.world, "post_animation");

    {
        MIMITA_PERF_SCOPE("NpcUpdate");
        sim.npcSystem->update(*sim.world, *sim.player, TICK_DT, frame);
    }

    // One shared actor-pair path for the local player, local NPCs/monsters, and
    // remote replicas. Remote replicas are solid prediction shapes but are not
    // mutated here; the server remains authoritative for their final state.
    {
        MIMITA_PERF_SCOPE("ActorPairCollision");
        static thread_local std::vector<ActorPairCollisionActor> collisionActors;
        static thread_local std::vector<ActorPairCollisionOutput> collisionOutputs;
        static thread_local std::vector<Npc*> collisionNpcs;
        collisionActors.clear();
        collisionNpcs.clear();

        const ActorCollisionConfig& collisionConfig =
            ActorCollisionConfig::instance();
        const ActorCollisionData& collisionData = collisionConfig.data();
        const bool localRagdollStatic =
            sim.player->ragdollModeActive || sim.player->dead;

        const std::string localPreset =
            MimitaNet::CommunityMatchClient::instance().active()
                ? MimitaNet::CommunityMatchClient::instance().actorPresetId()
                : std::string();

        ActorPairCollisionActor localActor;
        localActor.id = mpContext.active ? mpContext.localPlayerId : 0u;
        localActor.kind = ActorPairKind::Player;
        localActor.pos = sim.player->pos;
        localActor.prevPos = sim.player->hasActorCollisionPreviousPos
            ? sim.player->actorCollisionPreviousPos : sim.player->pos;
        localActor.vel = sim.player->vel;
        localActor.externalImpulse = sim.player->externalImpulse;
        localActor.yaw = sim.player->yaw;
        localActor.sizeScale = sim.player->sizeScale;
        localActor.dead = sim.player->dead;
        localActor.staticCollider = localRagdollStatic;
        localActor.onGround = sim.player->ground.onGround;
        localActor.stableOnGround = sim.player->ground.stableOnGround;
        localActor.hasWorldContact = sim.player->ground.hasWorldContact;
        localActor.profile = &collisionConfig.profileFor(localPreset);
        localActor.previousSupportKey = sim.player->collision.supportActorKey;
        localActor.previousSupportVelocity =
            sim.player->collision.supportActorVelocity;
        collisionActors.push_back(localActor);

        for (Npc& npc : sim.npcSystem->all())
        {
            const bool dead = npc.body.dead || npc.body.currentHp <= 0;
            ActorPairCollisionActor actor;
            actor.id = npc.id;
            actor.kind = ActorPairKind::Npc;
            actor.pos = npc.body.pos;
            actor.prevPos = dead ? npc.body.pos : npc.previousPosition;
            actor.vel = npc.body.vel;
            actor.externalImpulse = npc.body.externalImpulse;
            actor.yaw = npc.body.yaw;
            actor.sizeScale = npc.body.sizeScale;
            actor.dead = dead;
            actor.staticCollider = dead;
            actor.onGround = npc.body.ground.onGround;
            actor.stableOnGround = npc.body.ground.stableOnGround;
            actor.hasWorldContact = npc.body.ground.hasWorldContact;
            actor.profile = &collisionConfig.profileFor(npc.actorPresetId);
            actor.previousSupportKey = npc.body.collision.supportActorKey;
            actor.previousSupportVelocity = npc.body.collision.supportActorVelocity;
            collisionActors.push_back(actor);
            collisionNpcs.push_back(&npc);
        }

        const std::size_t localNpcActorCount = collisionNpcs.size();
        if (mpContext.active)
        {
            for (const auto& entry : mpContext.remotePlayers)
            {
                const Player& remote = entry.second;
                ActorPairCollisionActor actor;
                actor.id = entry.first;
                actor.kind = ActorPairKind::Player;
                actor.pos = remote.pos;
                actor.prevPos = remote.pos;
                actor.vel = remote.vel;
                actor.yaw = remote.yaw;
                actor.sizeScale = remote.sizeScale;
                actor.dead = remote.dead;
                actor.staticCollider = true;
                actor.onGround = remote.ground.onGround;
                actor.stableOnGround = remote.ground.stableOnGround;
                actor.hasWorldContact = remote.ground.hasWorldContact;
                actor.profile = &collisionConfig.profileFor(std::string());
                collisionActors.push_back(actor);
            }
            for (const auto& entry : mpContext.remoteNpcs)
            {
                const Player& remote = entry.second;
                ActorPairCollisionActor actor;
                actor.id = entry.first;
                actor.kind = ActorPairKind::Npc;
                actor.pos = remote.pos;
                actor.prevPos = remote.pos;
                actor.vel = remote.vel;
                actor.yaw = remote.yaw;
                actor.sizeScale = remote.sizeScale;
                actor.dead = remote.dead;
                actor.staticCollider = true;
                actor.onGround = remote.ground.onGround;
                actor.stableOnGround = remote.ground.stableOnGround;
                actor.hasWorldContact = remote.ground.hasWorldContact;
                actor.profile = &collisionConfig.profileFor(std::string());
                collisionActors.push_back(actor);
            }
        }

        collisionOutputs.assign(collisionActors.size(), ActorPairCollisionOutput{});
        ActorPairCollisionStats collisionStats;
        resolveActorPairCollisions(collisionActors.data(), collisionActors.size(),
                                   TICK_DT, collisionData,
                                   collisionOutputs.data(), &collisionStats,
                                   "client", sim.tick);

        Perf::state().current.collisionPairs =
            static_cast<int>(collisionStats.candidatePairs);
        Perf::state().current.charVsChar += collisionStats.totalMs;

        if (!localRagdollStatic &&
            !VehicleSystem::instance().hasLocalDriver())
        {
            const ActorPairCollisionOutput& out = collisionOutputs[0];
            sim.player->pos = out.pos;
            sim.player->vel = out.vel;
            sim.player->externalImpulse = out.externalImpulse;
            sim.player->ground.onGround = out.onGround;
            sim.player->ground.stableOnGround = out.stableOnGround;
            sim.player->ground.hasWorldContact = out.hasWorldContact;
            sim.player->collision.supportActorKey = out.supportKey;
            sim.player->collision.supportActorVelocity = out.supportVelocity;
            // Any actor-pair position/velocity change must be re-checked against
            // static geometry in the same tick. Actor support alone is not floor.
            if (out.corrected || out.pushed || out.supportKey != 0)
            {
                bool grounded = false;
                doCollisions(*sim.player, *sim.world, grounded, TICK_DT);
            }
            if (out.supportKey != 0)
            {
                sim.player->ground.onGround = true;
                sim.player->ground.stableOnGround = true;
            }
            sim.player->updateModelWorldTransforms();
            if (!sim.player->dead && !sim.player->ragdollModeActive)
                containAndLogPlayerFinalPose(*sim.player, *sim.world,
                                             "post_actor_pair");
        }

        for (std::size_t i = 0; i < localNpcActorCount; ++i)
        {
            Npc& npc = *collisionNpcs[i];
            const bool dead = npc.body.dead || npc.body.currentHp <= 0;
            if (dead)
                continue;
            const ActorPairCollisionOutput& out = collisionOutputs[i + 1];
            npc.body.pos = out.pos;
            npc.body.vel = out.vel;
            npc.body.externalImpulse = out.externalImpulse;
            npc.body.ground.onGround = out.onGround;
            npc.body.ground.stableOnGround = out.stableOnGround;
            npc.body.ground.hasWorldContact = out.hasWorldContact;
            npc.body.collision.supportActorKey = out.supportKey;
            npc.body.collision.supportActorVelocity = out.supportVelocity;
            if (out.corrected || out.pushed || out.supportKey != 0)
            {
                bool grounded = false;
                doCollisions(npc.body, *sim.world, grounded, TICK_DT);
            }
            if (out.supportKey != 0)
            {
                npc.body.ground.onGround = true;
                npc.body.ground.stableOnGround = true;
            }
            npc.body.updateModelWorldTransforms();
            containAndLogLocalNpcFinalPose(npc, *sim.world, "post_actor_pair");
        }

        static uint32_t lastCollisionProfileTick = 0;
        if (sim.tick - lastCollisionProfileTick >= 60)
        {
            lastCollisionProfileTick = sim.tick;
            nlohmann::json activeProfiles = nlohmann::json::object();
            for (const ActorPairCollisionActor& actor : collisionActors)
            {
                if (!actor.profile) continue;
                activeProfiles[actor.profile->id] = {
                    {"capsule_radius_m", actor.profile->capsuleRadiusM},
                    {"capsule_height_m", actor.profile->capsuleHeightM},
                    {"dead_capsule_length_m", actor.profile->deadCapsuleLengthM}};
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Important,
                "actor.collision.profile", "client",
                collisionStats.truncated ? "candidate_cap_reached" : "tick_profile",
                sim.tick,
                nlohmann::json{
                    {"source", "client"},
                    {"actors", collisionStats.actorCount},
                    {"cell_entries", collisionStats.cellEntries},
                    {"candidate_pairs", collisionStats.candidatePairs},
                    {"narrow_pairs", collisionStats.narrowPairs},
                    {"contacts", collisionStats.contacts},
                    {"supports", collisionStats.supports},
                    {"pushes", collisionStats.pushes},
                    {"player_player_candidates", collisionStats.playerPlayerCandidates},
                    {"player_npc_candidates", collisionStats.playerNpcCandidates},
                    {"npc_npc_candidates", collisionStats.npcNpcCandidates},
                    {"player_player_contacts", collisionStats.playerPlayerContacts},
                    {"player_npc_contacts", collisionStats.playerNpcContacts},
                    {"npc_npc_contacts", collisionStats.npcNpcContacts},
                    {"broadphase_ms", collisionStats.broadphaseMs},
                    {"collision_ms", collisionStats.totalMs},
                    {"frame_ms", Perf::state().lastFrameTimeMs},
                    {"active_profiles", activeProfiles},
                    {"config_revision", collisionData.revision}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    {
        MIMITA_PERF_SCOPE("DeathSystemUpdate");
        if (!mpContext.active)
            tickLocalPhysicalContactWeapons(*sim.player, *sim.npcSystem, (uint32_t)sim.tick);
        DeathSystem::instance().update(
            *sim.world, *sim.player, *sim.npcSystem, frame.jumpPressed, TICK_DT);
    }

    // Corpse ragdolls simulate at the fixed gameplay rate so their motion is
    // deterministic and independent of render frame rate.
    {
        MIMITA_PERF_SCOPE("RagdollCorpseUpdate");
        RagdollModeSystem::instance().updateCorpses(TICK_DT, *sim.world);
    }

    VehicleSystem::instance().checkVoidDeath(
        *sim.player, getVoidDeathConfig().killZ);

    checkVoidDeath(*sim.player, sim.player->username, "player");
    for (Npc& npc : sim.npcSystem->all())
        checkVoidDeath(npc.body, "npc_" + std::to_string(npc.id), "npc");

    // The seat owns the authoritative local origin while seated. Re-assert it
    // after actor/entity contact passes so a capsule push cannot eject the
    // driver from the chassis before the next tick.
    if (VehicleSystem::instance().hasLocalDriver() &&
        !sim.player->ragdollModeActive &&
        !sim.player->dead && !sim.player->voidDeathTriggered)
    {
        glm::vec3 seatPosition(0.0f);
        glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
        if (VehicleSystem::instance().localSeatWorld(seatPosition, seatForward))
        {
            sim.player->pos = seatPosition;
            sim.player->vel = glm::vec3(0.0f);
            sim.player->externalImpulse = glm::vec3(0.0f);
            sim.player->inputWishMove = glm::vec2(0.0f);
        }
    }

    sim.tick++;
}
