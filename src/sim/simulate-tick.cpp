#include "sim/sim-context.h"
#include "input/input-frame.h"
#include "input/input-state.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "world/world.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static InputState inputStateFromFrame(const InputFrame& frame)
{
    InputState state;
    state.wishMoveXY = {frame.moveX, frame.moveY};
    state.jumpHeld = frame.jump;
    state.dashPressed = frame.dashPressed;
    state.groundReturnPressed = frame.groundReturnPressed;
    state.freezeHeld = frame.freezeHeld;

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
    if (!sim.player || !sim.world || !sim.npcSystem) return;

    InputState input = inputStateFromFrame(frame);
    physicsMainUpdate(*sim.player, *sim.world, input, TICK_DT);
    sim.npcSystem->update(*sim.world, *sim.player, TICK_DT);

    // Resolve NPC vs Player collisions
    for (auto& npc : sim.npcSystem->all())
    {
        bool groundedPlayer = false;
        bool groundedNpc = false;
        resolveCapsuleVsCapsule(*sim.player, npc.body, groundedPlayer, groundedNpc);
    }

    sim.tick++;
}
