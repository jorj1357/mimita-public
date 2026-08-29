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

    if (!sim.player->dead) {
        MIMITA_PERF_SCOPE("PhysicsMainUpdate");
        setCollisionEntityContext("Player", 0, false);
        physicsMainUpdate(*sim.player, *sim.world, inputStateFromFrame(frame), TICK_DT);
        clearCollisionEntityContext();
    }

    {
        MIMITA_PERF_SCOPE("NpcUpdate");
        sim.npcSystem->update(*sim.world, *sim.player, TICK_DT, frame);
    }

    // Resolve NPC vs Player collisions
    {
        MIMITA_PERF_SCOPE("NpcVsPlayerCollision");
        for (auto& npc : sim.npcSystem->all())
        {
            bool groundedPlayer = false;
            bool groundedNpc = false;
            if (!sim.player->dead && !npc.body.dead)
                resolveCapsuleVsCapsule(*sim.player, npc.body, groundedPlayer, groundedNpc);
        }
    }

    {
        MIMITA_PERF_SCOPE("DeathSystemUpdate");
        DeathSystem::instance().update(
            *sim.world, *sim.player, *sim.npcSystem, frame.jumpPressed, TICK_DT);
    }

    if (sim.player->spawnFlashTimer > 0.0f)
        sim.player->spawnFlashTimer = std::max(0.0f, sim.player->spawnFlashTimer - 1.0f);

    checkVoidDeath(*sim.player, sim.player->username, "player");
    for (Npc& npc : sim.npcSystem->all())
        checkVoidDeath(npc.body, "npc_" + std::to_string(npc.id), "npc");

    sim.tick++;
}
