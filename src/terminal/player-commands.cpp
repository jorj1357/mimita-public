#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "input/input-commands.h"
#include "network/net_mode.h"
#include "combat/death-system.h"
#include "input/input-poll.h"
#include "config/player-settings.h"
#include "network/packets.h"
#include "outfit/outfit-system.h"

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
            player.ground.onGround = false;
            player.jump.jumpHeldPrev = false;
            player.dash.moveHeldPrev = false;
            player.dash.dashHeldPrev = false;
            player.freeze.freezeHeldPrev = false;
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

    Terminal::instance().registerCommand({
        "chat", "Send a chat message visible above your character", "chat <message>",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;
            if (args.empty())
            {
                Terminal::instance().addLog("[CHAT] usage: chat <message>");
                return;
            }
            std::string message;
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0) message += " ";
                message += args[i];
            }
            if (message.size() > 240)
            {
                message.resize(240);
                Terminal::instance().addLog("[CHAT] message truncated to 240 characters");
            }

            printf("[CHAT] %s: %s\n", player.username.c_str(), message.c_str());
            Terminal::instance().addLog("[CHAT] " + player.username + ": " + message);
            Terminal::instance().addLog("[CHAT] bubble added");

            addChatMessage(player.chatState, message, player.username);
            playChatSound((int)message.size());

            {
                ReplayEffectEvent chatEvent;
                chatEvent.type = "chat";
                chatEvent.sourceActorId = player.username;
                chatEvent.assetId = message;
                chatEvent.lifetime = computeChatDuration((int)message.size());
                captureReplayEffect(chatEvent);
                Terminal::instance().addLog("[CHAT] replay event recorded");
            }

            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (mpContext.active && mpContext.localPlayerId != 0)
            {
                MimitaNet::ChatPacket chatPacket{};
                chatPacket.header.type = MimitaNet::PACKET_CHAT_MESSAGE;
                chatPacket.header.tick = mpContext.tick;
                chatPacket.header.playerId = mpContext.localPlayerId;
                std::memset(chatPacket.senderName, 0, sizeof(chatPacket.senderName));
                std::strncpy(chatPacket.senderName, player.username.c_str(),
                             sizeof(chatPacket.senderName) - 1);
                std::memset(chatPacket.text, 0, sizeof(chatPacket.text));
                std::strncpy(chatPacket.text, message.c_str(), sizeof(chatPacket.text) - 1);
                MimitaNet::mpSendPacket(mpContext, &chatPacket, sizeof(chatPacket));
                Terminal::instance().addLog("[CHAT] replicated");
            }
        }
    });

    Terminal::instance().registerCommand({
        "setoutfit", "Set and save the player outfit PNG", "setoutfit <path>",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: setoutfit <path>");
                return;
            }
            if (OutfitSystem::applySingleTexture(player, args[0])) {
                GetPlayerSettings().outfitPath = args[0];
                SavePlayerSettings();
            }
        }
    });
    Terminal::instance().registerCommand({
        "reloadoutfit", "Reload the current outfit PNG from disk", "reloadoutfit",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            OutfitSystem::applySingleTexture(player, GetPlayerSettings().outfitPath, true);
        }
    });
    Terminal::instance().registerCommand({
        "outfitdebug", "Print outfit atlas region mapping", "outfitdebug",
        [](const std::vector<std::string>&) {  }
    });
    Terminal::instance().registerCommand({
        "killfeed", "Show recent kills", "killfeed",
        [](const std::vector<std::string>&) {
            WeaponSystem& weapons = THE_WEAPONS;
            if (weapons.killfeed().empty()) {
                Terminal::instance().addLog("[KILLFEED] no kills");
                return;
            }
            for (const std::string& line : weapons.killfeed())
                Terminal::instance().addLog("[KILLFEED] " + line);
        }
    });

    Terminal::instance().registerCommand({
        "pos", "Print local player position", "pos",
        [](const std::vector<std::string>&) {
            if (!gpPlayer) {
                Terminal::instance().addLog("No local player.");
                return;
            }
            Player& player = *gpPlayer;

            char buf[256];
            snprintf(buf, sizeof(buf), "Player Position: (%.3f, %.3f, %.3f)",
                     player.pos.x, player.pos.y, player.pos.z);
            Terminal::instance().addLog(buf);

            snprintf(buf, sizeof(buf), "Velocity: (%.3f, %.3f, %.3f)",
                     player.vel.x, player.vel.y, player.vel.z);
            Terminal::instance().addLog(buf);

            Terminal::instance().addLog(
                player.ground.onGround ? "Grounded: true" : "Grounded: false");

            if (gpActiveMapPath && !gpActiveMapPath->empty())
                Terminal::instance().addLog("Current Map: " + *gpActiveMapPath);

            snprintf(buf, sizeof(buf), "Facing: (%.3f, %.3f, %.3f)",
                     player.aimDirection.x, player.aimDirection.y, player.aimDirection.z);
            Terminal::instance().addLog(buf);
        }
    });
}

