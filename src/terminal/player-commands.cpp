#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "input/input-commands.h"
#include "network/net_mode.h"
#include "combat/death-system.h"
#include "input/input-poll.h"

// TODO(main-cleanup): move to devtools/dev-teleport.cpp
static bool parseTeleportPosition(
    const std::vector<std::string>& args,
    glm::vec3& position)
{
    if (args.size() == 1)
        return std::sscanf(
            args[0].c_str(), "%f,%f,%f",
            &position.x, &position.y, &position.z) == 3;
    if (args.size() == 3)
    {
        try
        {
            position = {
                std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    return false;
}

void registerPlayerCommands()
{
    auto registerActionCommand = [](const char* name, const char* description) {
        Terminal::instance().registerCommand({
            name, description, name,
            [name](const std::vector<std::string>&) {
                InputCommandSystem::instance().pulseAction(name);
                Terminal::instance().addLog(std::string("[GAMEPLAY] ") + name);
            }
        });
    };
    registerActionCommand("walkforward", "Move forward for one simulation tick");
    registerActionCommand("walkback", "Move backward for one simulation tick");
    registerActionCommand("walkleft", "Move left for one simulation tick");
    registerActionCommand("walkright", "Move right for one simulation tick");
    registerActionCommand("jump", "Execute a jump action");
    registerActionCommand("dash", "Execute a dash action");

    Terminal::instance().registerCommand({
        "teleport", "Teleport the local player", "teleport x,y,z",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;
            glm::vec3 destination(0.0f);
            if (!parseTeleportPosition(args, destination) ||
                !std::isfinite(destination.x) ||
                !std::isfinite(destination.y) ||
                !std::isfinite(destination.z))
            {
                Terminal::instance().addLog("[ERROR] Usage: teleport x,y,z");
                return;
            }

            player.pos = destination;
            player.vel = glm::vec3(0.0f);
            player.externalImpulse = glm::vec3(0.0f);
            player.inputWishMove = glm::vec2(0.0f);
            player.onGround = false;
            player.jumpHeldPrev = false;
            player.moveHeldPrev = false;
            player.dashHeldPrev = false;
            player.freezeHeldPrev = false;
            player.syncLegacyStateToLayers();
            player.updateModelWorldTransforms();

            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (mpContext.active)
                MimitaNet::mpRequestTeleport(mpContext, destination);

            char line[128];
            snprintf(line, sizeof(line),
                     "[GAMEPLAY] teleported to %.2f,%.2f,%.2f",
                     destination.x, destination.y, destination.z);
            Terminal::instance().addLog(line);
        }
    });

    Terminal::instance().registerCommand({
        "explode", "Instantly kill the local player", "explode",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            if (player.dead)
            {
                Terminal::instance().addLog("[GAMEPLAY] already dead");
                return;
            }

            DeathSystem::instance().kill(
                player,
                player.username,
                "player",
                "explode",
                glm::vec3(0.0f, 0.0f, 1.0f),
                24.0f);
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (mpContext.active)
                MimitaNet::mpRequestExplode(mpContext);
            Terminal::instance().addLog("[GAMEPLAY] explode");
        }
    });

    Terminal::instance().registerCommand({
        "freeze", "Toggle freeze", "freeze",
        [](const std::vector<std::string>&) {
            gTerminalInputOverride.freezeHeld = !gTerminalInputOverride.freezeHeld;
            Terminal::instance().addLog(
                gTerminalInputOverride.freezeHeld ? "[GAMEPLAY] freeze ON" : "[GAMEPLAY] freeze OFF");
        }
    });

    Terminal::instance().registerCommand({
        "ground_return", "Execute a ground return", "ground_return",
        [](const std::vector<std::string>&) {
            gTerminalInputOverride.groundReturnPressed = true;
            Terminal::instance().addLog("[GAMEPLAY] ground_return");
        }
    });
}
