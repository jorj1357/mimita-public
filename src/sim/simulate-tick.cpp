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
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "input/input-state.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
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
#include "effects/hit-effects.h"
#include "void-death/void-death.h"
#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "network/multiplayer-context.h"
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
                sim.player->pos = seatPosition;
                sim.player->vel = glm::vec3(0.0f);
                sim.player->externalImpulse = glm::vec3(0.0f);
                sim.player->inputWishMove = glm::vec2(0.0f);
            }
            InputState seatedInput = inputStateFromFrame(tickFrame);
            if (AimBodyConfig::instance().bodyPhysicsMode() &&
                !sim.player->usesMeleeAuthoredPose()) {
                MIMITA_PERF_SCOPE("SeatedAimBodyUpdate");
                if (!ragdoll.aimActive())
                    ragdoll.activateAim(*sim.player);
                ragdoll.updateAim(TICK_DT, *sim.world, *sim.player,
                                  seatedInput.camForward,
                                  THE_CAMERA.pos,
                                  frame.extendRightMouse);
            } else if (ragdoll.aimActive()) {
                ragdoll.deactivateAim(*sim.player);
            }
        } else {
            MIMITA_PERF_SCOPE("PhysicsMainUpdate");
            InputState normalInput = inputStateFromFrame(tickFrame);
            setCollisionEntityContext("Player", 0, false);
            physicsMainUpdate(*sim.player, *sim.world, normalInput, TICK_DT);
            clearCollisionEntityContext();

            // Always-on physical aim body (aimbody.json mode == "physical" or
            // "hybrid"). Runs after normal movement/animation so it owns the
            // rendered pose and the client hitboxes for this tick. Melee exclusive
            // authored pose disables it so limb momentum/head tilt cannot alter
            // the authored Captain Falcon pose.
            if (AimBodyConfig::instance().bodyPhysicsMode() &&
                !sim.player->usesMeleeAuthoredPose()) {
                MIMITA_PERF_SCOPE("AimBodyUpdate");
                if (!ragdoll.aimActive())
                    ragdoll.activateAim(*sim.player);
                ragdoll.updateAim(TICK_DT, *sim.world, *sim.player,
                                  normalInput.camForward,
                                  THE_CAMERA.pos,
                                  frame.extendRightMouse);
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

        if (!localRagdollStatic)
        {
            const ActorPairCollisionOutput& out = collisionOutputs[0];
            sim.player->pos = out.pos;
            sim.player->vel = out.vel;
            sim.player->externalImpulse = out.externalImpulse;
            sim.player->ground.onGround = out.onGround;
            sim.player->ground.stableOnGround = out.stableOnGround;
            sim.player->collision.supportActorKey = out.supportKey;
            sim.player->collision.supportActorVelocity = out.supportVelocity;
            if (out.corrected && out.supportKey == 0)
            {
                bool grounded = false;
                doCollisions(*sim.player, *sim.world, grounded, TICK_DT);
            }
            sim.player->updateModelWorldTransforms();
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
            npc.body.collision.supportActorKey = out.supportKey;
            npc.body.collision.supportActorVelocity = out.supportVelocity;
            if (out.corrected && out.supportKey == 0)
            {
                bool grounded = false;
                doCollisions(npc.body, *sim.world, grounded, TICK_DT);
            }
            npc.body.updateModelWorldTransforms();
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
