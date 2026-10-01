#include "fister/fister-port.h"

#include "fister/sim/FisterSim.h"
#include "entities/player.h"
#include "avatar/avatar.h"
#include "input/input-state.h"
#include "terminal/terminal-state.h"
#include "world/world.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace
{
    fister::Sim gSim;
    bool gEnabled = false;
    bool gToggleWasDown = false;
    bool gLoadedBody = false;
    double gAccumulator = 0.0;
    std::uint32_t gTapSequence = 0;

    fister::Vec3 toFister(const glm::vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    glm::vec3 toGlm(const fister::Vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    void loadBody(Player& player)
    {
        if (gLoadedBody)
            return;

        if (player.loadModel("assets/fister/model/john_fister.glb")) {
            gLoadedBody = true;
            Debug::log(Debug::Category::General,
                       "[FISTER] body loaded path=assets/fister/model/john_fister.glb");
        } else {
            Debug::log(Debug::Category::General,
                       "[FISTER] body load failed path=assets/fister/model/john_fister.glb");
        }
    }

    fister::Input buildInput(const Player& player, const InputState& input)
    {
        fister::Input out;
        const glm::vec3 forward = glm::normalize(glm::vec3(input.camForward.x,
                                                            input.camForward.y, 0.0f));
        const glm::vec3 right = glm::vec3(forward.y, -forward.x, 0.0f);
        const float yaw = std::atan2(forward.x, forward.y);
        out.moveY = glm::dot(input.wishMoveXY, glm::vec2(forward.x, forward.y));
        out.moveX = glm::dot(input.wishMoveXY, glm::vec2(right.x, right.y));
        out.yaw = yaw;
        out.pitch = std::asin(std::clamp(input.camForward.z, -1.0f, 1.0f));
        out.charging = input.fisterFistHeld;
        out.parrying = input.fisterParryHeld;
        out.grabbing = input.fisterGrabHeld;
        out.tapPunchSeq = gTapSequence;
        (void)player;
        return out;
    }

    fister::Environment buildEnvironment(const Player& player)
    {
        fister::Environment env;
        env.position = toFister(player.pos);
        env.groundSupport = player.ground.onGround || player.ground.stableOnGround;
        env.groundClearance = std::max(0.0f, player.pos.z - 0.8f);
        env.hostLimp = player.ragdollModeActive;
        env.dead = player.dead;
        return env;
    }

    void logEvents()
    {
        for (const fister::Event& event : gSim.Events()) {
            Debug::log(Debug::Category::General,
                       "[FISTER] event=%u strength=%.3f value=%.3f",
                       static_cast<unsigned>(event.type), event.strength, event.value);
        }
        gSim.Events().clear();
    }
}

namespace FisterPort
{
    bool enabled()
    {
        return gEnabled;
    }

    void reset()
    {
        gSim.Reset();
        gAccumulator = 0.0;
        gTapSequence = 0;
        gLoadedBody = false;
    }

    bool tick(Player& player, const World& world, const InputState& input, float dt)
    {
        (void)world;
        if (&player != gpPlayer)
            return false;

        const bool toggleDown = input.fisterTogglePressed;
        if (toggleDown && !gToggleWasDown) {
            gEnabled = !gEnabled;
            reset();
            if (!gEnabled)
                AvatarSystem::instance().applyToPlayer(player, true);
            Debug::log(Debug::Category::General, "[FISTER] mode=%s terrain=disabled",
                       gEnabled ? "enabled" : "disabled");
        }
        gToggleWasDown = toggleDown;

        if (!gEnabled)
            return false;

        loadBody(player);
        gAccumulator = std::min(gAccumulator + static_cast<double>(std::max(0.0f, dt)), 0.25);
        constexpr double fixedDt = 1.0 / 60.0;
        const fister::Input fisterInput = buildInput(player, input);
        while (gAccumulator >= fixedDt) {
            fister::Environment env = buildEnvironment(player);
            gSim.Tick(fisterInput, env);
            player.vel = toGlm(gSim.Velocity());
            player.pos += player.vel * static_cast<float>(fixedDt);
            player.yaw = glm::degrees(fisterInput.yaw);
            player.updateModelWorldTransforms();
            gAccumulator -= fixedDt;
            logEvents();
        }
        return true;
    }
}
