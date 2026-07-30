#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "input/input-commands.h"
#include "network/net_mode.h"
#include "combat/death-system.h"
#include "debug/debug-log.h"
#include "input/input-poll.h"
#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "network/packets.h"
#include "network/multiplayer-context.h"
#include "gui/hud/chat-bubble.h"
#include "gui/hud/chat-history.h"
#include "replay/replay-scene.h"
#include "avatar/avatar.h"

// TODO(main-cleanup): move to devtools/dev-teleport.cpp
// Shared function: both the console "chat" command and GUI text input call this.
void requestSendChatMessage(const std::string& message)
{
    Player& player = THE_PLAYER;
    if (message.empty())
        return;

    // Trim whitespace
    std::string trimmed = message;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty())
    {
        Terminal::instance().addLog("[server]: u cant send nothing, silly!");
        return;
    }

    // Limit to 256 Unicode code points (byte-safe: truncate at 256 bytes for now,
    // full Unicode-codepoint validation is a future enhancement)
    if (trimmed.size() > 256)
        trimmed.resize(256);

    Debug::log(Debug::Category::Chat, "[CHAT SEND] player=%s len=%zu\n",
               player.username.c_str(), trimmed.size());

    addChatMessage(player.chatState, trimmed, player.username);
    playChatSound((int)trimmed.size());

    {
        ReplayEffectEvent chatEvent;
        chatEvent.type = "chat";
        chatEvent.sourceActorId = player.username;
        chatEvent.assetId = trimmed;
        chatEvent.lifetime = computeChatDuration((int)trimmed.size());
        captureReplayEffect(chatEvent);
    }

    // Add to local chat history
    ChatHistoryEntry entry;
    entry.messageId = 0; // assigned by server when online
    entry.serverTick = 0;
    entry.utcUnixMilliseconds = 0;
    entry.senderEntityId = 0;
    entry.senderAccountId = 0;
    entry.senderType = ChatSenderType::Player;
    entry.senderName = player.username;
    entry.text = trimmed;
    gChatHistory.append(entry);

    MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
    if (mpContext.active && mpContext.localPlayerId != 0)
    {
        // Use new v2 chat request packet
        MimitaNet::ChatRequestPacket req{};
        req.header.type = MimitaNet::PACKET_CHAT_REQUEST;
        req.header.tick = mpContext.tick;
        req.header.playerId = mpContext.localPlayerId;
        req.requestId = mpContext.nextActionRequestId++;
        req.clientSimulationTick = mpContext.tick;
        std::strncpy(req.utf8Message, trimmed.c_str(), sizeof(req.utf8Message) - 1);
        MimitaNet::mpSendPacket(mpContext, &req, sizeof(req));
    }
}

// Preferences for chat features
namespace {
    bool gChatWindowEnabled = true;
    bool gChatTipsEnabled = true;
    bool gFloatingTipsEnabled = true;
    bool gNotificationsEnabled = true;
    uint64_t gNotificationMuteUntilTick = 0;
}

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
            requestSendChatMessage(message);
        }
    });

    Terminal::instance().registerCommand({
        "chatwindow", "Show or hide the chat window (0=hide, 1=show)", "chatwindow <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gChatWindowEnabled ? "[CHAT] chatwindow 1" : "[CHAT] chatwindow 0");
                return;
            }
            gChatWindowEnabled = args[0] != "0";
            Terminal::instance().addLog(gChatWindowEnabled ? "[CHAT] chat window enabled" : "[CHAT] chat window disabled");
        }
    });

    Terminal::instance().registerCommand({
        "chattips", "Show or hide automated chat tips (0=hide, 1=show)", "chattips <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gChatTipsEnabled ? "[CHAT] chattips 1" : "[CHAT] chattips 0");
                return;
            }
            gChatTipsEnabled = args[0] != "0";
            Terminal::instance().addLog(gChatTipsEnabled ? "[CHAT] chat tips enabled" : "[CHAT] chat tips disabled");
        }
    });

    Terminal::instance().registerCommand({
        "tips", "Show or hide the floating tip box (0=hide, 1=show)", "tips <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gFloatingTipsEnabled ? "[CHAT] tips 1" : "[CHAT] tips 0");
                return;
            }
            gFloatingTipsEnabled = args[0] != "0";
            Terminal::instance().addLog(gFloatingTipsEnabled ? "[CHAT] floating tips enabled" : "[CHAT] floating tips disabled");
        }
    });

    Terminal::instance().registerCommand({
        "listusers", "List all connected users with temporary numeric indices", "listusers",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            int idx = 1;
            Terminal::instance().addLog("[CHAT] Connected users:");
            Terminal::instance().addLog(std::to_string(idx++) + ". " + player.username + " (you)");
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            for (auto& kv : mpContext.remotePlayers)
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%d. %s", idx++, kv.second.username.c_str());
                Terminal::instance().addLog(buf);
            }
        }
    });

    Terminal::instance().registerCommand({
        "chatmute", "Mute or unmute a player by username or list index", "chatmute <username|index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[CHAT] usage: chatmute <username or listusers index>");
                return;
            }
            // TODO: Implement local mute map and rendering
            Terminal::instance().addLog("[CHAT] chatmute not yet fully implemented");
        }
    });

    Terminal::instance().registerCommand({
        "reportuser", "Report a player to the moderation team (interactive)",
        "reportuser",
        [](const std::vector<std::string>&) {
            // TODO: Implement interactive report flow
            Terminal::instance().addLog("[CHAT] reportuser not yet fully implemented");
        }
    });

    Terminal::instance().registerCommand({
        "servermessage", "Send a server-wide message (host/operator only)", "servermessage <message>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[CHAT] usage: servermessage <message>");
                return;
            }
            // TODO: Implement permission check and server message broadcast
            Terminal::instance().addLog("[CHAT] servermessage not yet fully implemented");
        }
    });

    Terminal::instance().registerCommand({
        "notifs", "Show or hide notifications (0=hide, 1=show)", "notifs <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gNotificationsEnabled ? "[CHAT] notifs 1" : "[CHAT] notifs 0");
                return;
            }
            gNotificationsEnabled = args[0] != "0";
            Terminal::instance().addLog(gNotificationsEnabled ? "[CHAT] notifications enabled" : "[CHAT] notifications disabled");
        }
    });

    Terminal::instance().registerCommand({
        "notifstempmute", "Temporarily mute notifications for N hours", "notifstempmute <hours>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[CHAT] usage: notifstempmute <hours>");
                return;
            }
            double hours = std::atof(args[0].c_str());
            if (hours <= 0.0) {
                gNotificationMuteUntilTick = 0;
                Terminal::instance().addLog("[CHAT] notification mute cleared");
                return;
            }
            uint64_t muteTicks = static_cast<uint64_t>(hours * 3600.0 * 60.0);
            gNotificationMuteUntilTick = 0 + muteTicks; // UI tick clock integration TBD
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[CHAT] notifications muted for %.1f hours (%llu ticks)", hours, (unsigned long long)muteTicks);
            Terminal::instance().addLog(buf);
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
            if (AvatarSystem::applySingleTexture(player, args[0])) {
                GetPlayerSettings().outfitPath = args[0];
                SavePlayerSettings();
            }
        }
    });
    Terminal::instance().registerCommand({
        "reloadoutfit", "Reload the current outfit PNG from disk", "reloadoutfit",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            AvatarSystem::applySingleTexture(player, GetPlayerSettings().outfitPath, true);
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
        "sizeme", "Set player size scale", "sizeme <value>",
        [](const std::vector<std::string>& args) {
            if (!gpPlayer) { Terminal::instance().addLog("[ERROR] No player"); return; }
            if (args.empty()) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[SIZE] current=%.4f", gpPlayer->sizeScale);
                Terminal::instance().addLog(buf);
                return;
            }
            float val = std::stof(args[0]);
            if (val < 0.001f) val = 0.001f;
            gpPlayer->sizeScale = val;
            const auto& sc = SizeScalingConfig::instance().data();
            int effectiveHp = (int)(100.0f * sc.scale(1.0f, sc.healthExponent, val));
            gpPlayer->maxHp = effectiveHp;
            gpPlayer->currentHp = effectiveHp;
            char buf[256];
            std::snprintf(buf, sizeof(buf), "[SIZE] set to %.4f (maxHp=%d)", val, effectiveHp);
            Terminal::instance().addLog(buf);
        }
    });
    Terminal::instance().registerCommand({
        "size_reset", "Reset player size to 1.0", "size_reset",
        [](const std::vector<std::string>&) {
            if (!gpPlayer) { Terminal::instance().addLog("[ERROR] No player"); return; }
            gpPlayer->sizeScale = 1.0f;
            gpPlayer->maxHp = 100;
            gpPlayer->currentHp = 100;
            Terminal::instance().addLog("[SIZE] Reset to 1.0 (maxHp=100)");
        }
    });
    Terminal::instance().registerCommand({
        "size_debug", "Show effective scaling values", "size_debug [1|0]",
        [](const std::vector<std::string>& args) {
            if (!gpPlayer) { Terminal::instance().addLog("[ERROR] No player"); return; }
            float s = std::max(gpPlayer->sizeScale, 0.001f);
            const auto& sc = SizeScalingConfig::instance().data();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[SIZE] scale=%.4f\n"
                "  effectiveHealth=%d\n"
                "  effectiveSpeed=%.2f (exp=%.2f)\n"
                "  effectiveJump=%.2f (exp=%.2f)\n"
                "  effectiveDash=%.2f (exp=%.2f)\n"
                "  effectiveDamage=%.2f (exp=%.2f)\n"
                "  effectiveSoundVol=%.2f (exp=%.2f)\n"
                "  effectiveSoundPitch=%.2f (exp=%.2f)",
                s,
                (int)(100.0f * sc.scale(1.0f, sc.healthExponent, s)),
                sc.scale(1.0f, sc.movementSpeedExponent, s), sc.movementSpeedExponent,
                sc.scale(1.0f, sc.jumpHeightExponent, s), sc.jumpHeightExponent,
                sc.scale(1.0f, sc.dashImpulseExponent, s), sc.dashImpulseExponent,
                sc.scale(1.0f, sc.damageExponent, s), sc.damageExponent,
                sc.scale(1.0f, sc.soundVolumeExponent, s), sc.soundVolumeExponent,
                sc.scale(1.0f, sc.soundPitchExponent, s), sc.soundPitchExponent);
            Terminal::instance().addLog(buf);
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


