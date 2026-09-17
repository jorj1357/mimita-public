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
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "input/input-state.h"
#include "physics/physics-mini.h"
#include "physics/config.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "world/world.h"
#include "config.h"
#include "debug/debug-log.h"
#include "combat/weapon-hit.h"
#include "combat/death-system.h"
#include "effects/hit-effects.h"
#include "void-death/void-death.h"
#include "ecs/actor-entities.h"
#include "hot-reload/generic-runtime.h"
#include "ragdoll/ragdoll-entities.h"
#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "editor/creation-mode.h"
#include "engine/engine-tick-creation.h"
#include "hot-reload/generic-runtime.h"
#include "live-code/live-behavior.h"
#include "live-code/live-editor.h"
#include "terminal/terminal-state.h"

#include <cmath>
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

    // The fixed-tick clock is owned here, independent of which movement path
    // (built-in kernel or a hot override) runs this tick. It advances even when
    // the player is dead so the client's reported simulation tick never freezes
    // (a frozen tick makes the server reject all later reports as stale).
    ++sim.player->movementSimulationTick;

    // Entity/component slice: the local human player is a stable entity with
    // identity, control source, live transform/health, and a movement intent.
    {
        const EntityId playerEntity = Ecs::ensureLocalPlayerEntity();
        Ecs::setTransform(playerEntity, sim.player->pos,
                          glm::vec3(1.0f, 0.0f, 0.0f), sim.player->yaw,
                          sim.player->aimBodyPitch);
        Ecs::setVelocity(playerEntity, sim.player->vel, sim.player->externalImpulse);
        Ecs::setHealth(playerEntity, sim.player->currentHp, sim.player->maxHp,
                       sim.player->dead);
        Ecs::setMovementIntent(playerEntity, frame.moveX, frame.moveY,
                               frame.movementPressed, frame.jump, frame.dashPressed,
                               frame.downDashPressed, frame.freezeHeld);
        // One source for the actor capsule so hot movement and the kernel solve
        // agree on size (fixes the model sinking when the fallback was used).
        Ecs::setBody(playerEntity, sim.player->sizeScale, PLAYER_RADIUS, PLAYER_HEIGHT);
    }

    // Publish shared editor/gameplay state and run gameplay-domain hot systems
    // BEFORE movement, so a hot movement system (free-fly) can override this
    // tick. This is the generic seam; no per-feature kernel slot.
    {
        const EntityId localEntity = Ecs::ensureLocalPlayerEntity();
        if (GameSharedStateV1* shared =
                MimitaRuntime::GenericRuntime::instance().sharedState()) {
            shared->magic = GAME_SHARED_MAGIC;
            shared->localPlayerEntity = (std::uint64_t)localEntity;
        }
        MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
        const std::uint64_t runtimeTick = (std::uint64_t)sim.tick;
        // Expose the world to movement/query capabilities for this tick.
        LiveBehavior::setDispatchWorld(sim.world);
        // Expose the current input + camera to hot code (input.read/camera.read).
        InputState hotInput = inputStateFromFrame(frame);
        LiveBehavior::setDispatchInput(&hotInput);
        LiveBehavior::setDispatchCamera(&THE_CAMERA);
        void* host = LiveBehavior::hostContext(runtimeTick);
        runtime.beginMovementTick();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, runtimeTick, TICK_DT, host);
        LiveBehavior::drainEvents(64);
    }

    if (!sim.player->dead) {
        // Hot movement override (free-fly/noclip): apply the requested transform
        // and skip the built-in physics step for this tick.
        float overridePos[3];
        float overrideVel[3];
        float overrideYaw = 0.0f;
        if (MimitaRuntime::GenericRuntime::instance().consumeMovementOverride(
                overridePos, overrideVel, overrideYaw))
        {
            sim.player->pos = glm::vec3(overridePos[0], overridePos[1], overridePos[2]);
            sim.player->vel = glm::vec3(overrideVel[0], overrideVel[1], overrideVel[2]);
            sim.player->yaw = overrideYaw;
            sim.player->externalImpulse = glm::vec3(0.0f);
            sim.player->ragdollModeActive = false;
        }
        else {
        // Handle ragdoll mode toggle
        static bool ragdollTogglePrev = false;
        bool ragdollToggleNow = frame.ragdollTogglePressed;
        if (ragdollToggleNow && !ragdollTogglePrev && RagdollModeConfig::instance().data().enabled) {
            auto& ragdoll = RagdollModeSystem::instance();
            if (ragdoll.isActive()) {
                ragdoll.deactivate(*sim.player);
                sim.player->ragdollModeActive = false;
            } else {
                ragdoll.activate(*sim.player);
                sim.player->ragdollModeActive = true;
            }
        }
        ragdollTogglePrev = ragdollToggleNow;

        if (sim.player->ragdollModeActive && RagdollModeSystem::instance().isActive()) {
            MIMITA_PERF_SCOPE("RagdollModeUpdate");
            InputState ragdollInput = inputStateFromFrame(frame);
            ragdollInput.grabLeftHeld = frame.grabLeftHeld;
            ragdollInput.grabRightHeld = frame.grabRightHeld;
            ragdollInput.extendLeftMouse = frame.extendLeftMouse;
            ragdollInput.extendRightMouse = frame.extendRightMouse;
            RagdollModeSystem::instance().update(TICK_DT, *sim.world, *sim.player,
                ragdollInput, THE_CAMERA);

            // Project the live ragdoll into persistent entities so it can be
            // inspected and replicated; owner id 1 is the local player.
            Ragdoll::RagdollEntities& ragdollEntities = Ragdoll::RagdollEntities::instance();
            const RagdollBody& aliveBody = RagdollModeSystem::instance().aliveBody();
            ragdollEntities.bind(1, aliveBody);
            ragdollEntities.syncFromBody(1, aliveBody);
            ragdollEntities.setGrab(1, true, RagdollModeSystem::instance().leftGrab());
            ragdollEntities.setGrab(1, false, RagdollModeSystem::instance().rightGrab());
        } else {
            MIMITA_PERF_SCOPE("PhysicsMainUpdate");
            setCollisionEntityContext("Player", 0, false);
            physicsMainUpdate(*sim.player, *sim.world, inputStateFromFrame(frame), TICK_DT);
            clearCollisionEntityContext();
        }
        } // end built-in movement (else of hot override)
    }

    {
        MIMITA_PERF_SCOPE("NpcUpdate");
        sim.npcSystem->update(*sim.world, *sim.player, TICK_DT, frame);
    }

    // Hot actor-movement post pass: after NPC AI has written generic intent,
    // one hot system owns movement for every non-local actor.
    {
        MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
        const std::uint64_t postTick = (std::uint64_t)sim.tick;
        void* postHost = LiveBehavior::hostContext(postTick);
        runtime.runDomain(GAME_DOMAIN_POST_MOVEMENT, postTick, TICK_DT, postHost);
        LiveBehavior::drainEvents(64);
    }

    // Resolve NPC vs Player collisions
    {
        MIMITA_PERF_SCOPE("NpcVsPlayerCollision");
        if (!sim.player->ragdollModeActive) {
            for (auto& npc : sim.npcSystem->all())
            {
                bool groundedPlayer = false;
                bool groundedNpc = false;
                if (!sim.player->dead && !npc.body.dead)
                    resolveCapsuleVsCapsule(*sim.player, npc.body, groundedPlayer, groundedNpc);
            }
        }
    }

    {
        MIMITA_PERF_SCOPE("DeathSystemUpdate");
        DeathSystem::instance().update(
            *sim.world, *sim.player, *sim.npcSystem, frame.jumpPressed, TICK_DT);
    }

    // Corpse ragdolls simulate at the fixed gameplay rate so their motion is
    // deterministic and independent of render frame rate.
    {
        MIMITA_PERF_SCOPE("RagdollCorpseUpdate");
        RagdollModeSystem::instance().updateCorpses(TICK_DT, *sim.world);
    }

    // Creation/inspection mode: continuous look-at pick at the fixed tick (not
    // per render frame). Prefer the hot editor module; fall back to the kernel
    // implementation when the module is absent or declines.
    {
        MIMITA_PERF_SCOPE("CreationModeUpdate");
        Editor::CreationMode& mode = Editor::CreationMode::instance();
        EditorStateV1 state{};
        state.enabled = mode.enabled() ? 1u : 0u;
        state.tick = (std::uint32_t)sim.tick;
        state.maxDistance = 200.0f;
        state.currentSelection = mode.selected();
        state.origin[0] = THE_CAMERA.pos.x;
        state.origin[1] = THE_CAMERA.pos.y;
        state.origin[2] = THE_CAMERA.pos.z;
        state.dir[0] = THE_CAMERA.front.x;
        state.dir[1] = THE_CAMERA.front.y;
        state.dir[2] = THE_CAMERA.front.z;

        EditorResultV1 result{};
        if (mode.enabled() && LiveEditor::tick(*sim.world, state, result)) {
            mode.setExternalResult(result.selectedEntity, result.hitKind, result.distance);
        } else {
            engineTickCreationUpdate(*sim.world, THE_CAMERA);
        }
    }

    // Generic runtime: run package-declared custom domains (gameplay.60 already
    // ran before movement).
    {
        MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
        const std::uint64_t runtimeTick = (std::uint64_t)sim.tick;
        void* host = LiveBehavior::hostContext(runtimeTick);
        runtime.runRegisteredDomains(runtimeTick, TICK_DT, host);
        // Client-only fixed 60 Hz presentation domain (effect timelines,
        // animations). Never run on a dedicated server.
        runtime.runDomain(GAME_DOMAIN_CLIENT_TICK, runtimeTick, TICK_DT, host);
        LiveBehavior::drainEvents(64);
    }

    if (sim.player->spawnFlashTimer > 0.0f)
        sim.player->spawnFlashTimer = std::max(0.0f, sim.player->spawnFlashTimer - 1.0f);

    checkVoidDeath(*sim.player, sim.player->username, "player");
    for (Npc& npc : sim.npcSystem->all())
        checkVoidDeath(npc.body, "npc_" + std::to_string(npc.id), "npc");

    sim.tick++;
}
