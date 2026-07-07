#include "sim/sim-context.h"
#include "input/input-frame.h"
#include "perf/perf.h"
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

    Perf::ScopedTimer _t("Simulation");

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
    if (!sim.player->dead) {
        setCollisionEntityContext("Player", 0, false);
        physicsMainUpdate(*sim.player, *sim.world, input, TICK_DT);
        clearCollisionEntityContext();

        // Spawn movement burst on input keydown transition (not velocity-based)
        if (frame.movementJustPressed && sim.player->ground.stableOnGround) {
            glm::vec2 move2d(frame.moveX, frame.moveY);
            float moveLen = glm::length(move2d);
            if (moveLen > 0.001f) {
                glm::vec3 dir = glm::normalize(glm::vec3(move2d.x, move2d.y, 0.0f));
                float speed = glm::length(glm::vec2(sim.player->vel.x, sim.player->vel.y));
                HitEffects::spawnMovementDashBurst(sim.player->pos, dir, speed);
            }
        }
    }
    {
    Perf::ScopedTimer _npcTick("NpcTick");
    sim.npcSystem->update(*sim.world, *sim.player, TICK_DT);
    }

    // Resolve NPC vs Player collisions
    for (auto& npc : sim.npcSystem->all())
    {
        bool groundedPlayer = false;
        bool groundedNpc = false;
        if (!sim.player->dead && !npc.body.dead)
            resolveCapsuleVsCapsule(*sim.player, npc.body, groundedPlayer, groundedNpc);
    }

    DeathSystem::instance().update(
        *sim.world, *sim.player, *sim.npcSystem, frame.jumpPressed, TICK_DT);

    if (sim.player->spawnFlashTimer > 0.0f)
        sim.player->spawnFlashTimer = std::max(0.0f, sim.player->spawnFlashTimer - 1.0f);

    checkVoidDeath(*sim.player, sim.player->username, "player");
    for (Npc& npc : sim.npcSystem->all())
        checkVoidDeath(npc.body, "npc_" + std::to_string(npc.id), "npc");

    if (DebugConfig::DEBUG_TICKS)
        Debug::log(Debug::Category::General,
                   "[PLAYER] tick=%llu pos=(%.3f %.3f %.3f) grounded=%d vel=(%.3f %.3f %.3f)\n",
                   (unsigned long long)sim.tick,
                   sim.player->pos.x, sim.player->pos.y, sim.player->pos.z,
                   (int)sim.player->ground.onGround,
                   sim.player->vel.x, sim.player->vel.y, sim.player->vel.z);

    sim.tick++;
}
