#include "sim/sim-context.h"
#include "input/input-frame.h"
#include "input/input-state.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "world/world.h"
#include "config.h"
#include "debug/debug-log.h"
#include "combat/weapon-hit.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static InputState inputStateFromFrame(const InputFrame& frame)
{
    InputState state;
    state.wishMoveXY = {frame.moveX, frame.moveY};
    state.jumpHeld = frame.jump;
    state.dashPressed = frame.dashPressed;
    state.movementPressed = frame.movementPressed;
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

    if (DebugConfig::DEBUG_TICKS)
        Debug::log(Debug::Category::General, "[TICK] number=%llu\n", (unsigned long long)sim.tick);
    if (DebugConfig::DEBUG_INPUT)
        Debug::log(Debug::Category::General,
                   "[INPUT] tick=%llu move=(%.2f %.2f) jump=%d dash=%d\n",
                   (unsigned long long)sim.tick, frame.moveX, frame.moveY,
                   (int)frame.jump, (int)frame.dashPressed);
    if (DebugConfig::DEBUG_COMMANDS) {
        if (frame.moveX != 0.0f || frame.moveY != 0.0f)
            Debug::log(Debug::Category::General, "[COMMAND] tick=%llu walk vector=(%.2f %.2f)\n",
                       (unsigned long long)sim.tick, frame.moveX, frame.moveY);
        if (frame.jump) Debug::log(Debug::Category::General, "[COMMAND] tick=%llu jump\n", (unsigned long long)sim.tick);
        if (frame.dashPressed) Debug::log(Debug::Category::General, "[COMMAND] tick=%llu dash\n", (unsigned long long)sim.tick);
    }

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

    // Resolve NPC melee attacks on player
    for (auto& npc : sim.npcSystem->all())
    {
        if (npc.chosenAction.attackPressed && npc.body.currentHp > 0)
        {
            weaponHit(npc.body, *sim.player);
        }
    }

    if (DebugConfig::DEBUG_TICKS)
        Debug::log(Debug::Category::General,
                   "[PLAYER] tick=%llu pos=(%.3f %.3f %.3f) grounded=%d vel=(%.3f %.3f %.3f)\n",
                   (unsigned long long)sim.tick,
                   sim.player->pos.x, sim.player->pos.y, sim.player->pos.z,
                   (int)sim.player->onGround,
                   sim.player->vel.x, sim.player->vel.y, sim.player->vel.z);

    sim.tick++;
}
