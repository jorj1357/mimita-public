// 08 03 2026, 17 20
/* purpose
* Handles server-side chat packet validation, rate limiting, and broadcast events.
* Copies verified server player identity metadata into accepted chat messages.
* Keeps chat moderation and gameplay command handling close to packet dispatch.
* DOES NOT render chat UI, verify website entitlements, or mutate account records.
* DOES NOT trust client-provided account ids or VIP appearance data.
* DOES NOT own NPC combat, projectile, or movement packet handling.
*/
#include "network/server.h"
#include "network/server-duel.h"
#include "network/multiplayer-context.h"
#include "network/chat-rate-limiter.h"
#include "void-death/void-death.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"
#include "map/map-catalog.h"
#include "network/community-server-config.h"
#include "npc/npc-difficulty-config.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <filesystem>

// Global chat rate limiter instance
static ChatRateLimiter gChatRateLimiter;

namespace MimitaNet {

void broadcastServerChatMessage(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    const char* message)
{
    static uint64_t nextServerMessageId = 1000000;

    ChatMessageEventPacket event{};
    event.header.type = PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = tick;
    event.header.playerId = 0;
    event.requestId = 0;
    event.messageId = nextServerMessageId++;
    event.serverTick = tick;
    event.utcUnixMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.senderEntityId = 0;
    event.senderAccountId = 0;
    event.senderType = 1; // Server
    event.channel = 0;
    std::strncpy(event.senderName, "server", sizeof(event.senderName) - 1);
    std::strncpy(event.utf8Message, message, sizeof(event.utf8Message) - 1);

    uint32_t eventId = nextReliableGameplayEventId();
    event.eventId = eventId;
    for (auto& kv : players)
    {
        uint32_t session = reliableGameplayEventSessionForPlayer(kv.second);
        queueReliableGameplayEventToPlayer(sock, kv.second, &event, sizeof(event),
                                           eventId, session, totalPacketsOut);
    }
}

void handleChatMessage(SOCKET sock, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ChatPacket))
        return;
    ChatPacket* chat = const_cast<ChatPacket*>(reinterpret_cast<const ChatPacket*>(buffer));
    auto it = players.find(chat->header.playerId);
    if (it == players.end())
        return;

    chat->header.tick = tick;
    printf("%s [CHAT] %s: %s\n", serverTimestamp(),
           it->second.name.c_str(), chat->text);

    for (const auto& playerEntry : players)
    {
        if (playerEntry.first == chat->header.playerId)
            continue;
        if (playerEntry.second.transport)
            playerEntry.second.transport->send(chat, sizeof(ChatPacket));
        else
            sendto(sock, (const char*)chat, sizeof(ChatPacket), 0,
                   (sockaddr*)&playerEntry.second.addr,
                   sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

void handleChatRequestV2(SOCKET sock, const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ChatRequestPacket))
        return;
    const ChatRequestPacket* req = reinterpret_cast<const ChatRequestPacket*>(buffer);
    auto it = players.find(req->header.playerId);
    if (it == players.end())
        return;

    // Retries of a request already accepted must not create a second message.
    // Re-send the cached event to the requesting player so its retry loop can
    // terminate even if the first reliable event copy was lost.
    if (req->requestId != 0 && it->second.hasCachedChatEvent &&
        it->second.lastChatRequestId == req->requestId)
    {
        ChatMessageEventPacket retry = it->second.cachedChatEvent;
        uint32_t session = reliableGameplayEventSessionForPlayer(it->second);
        queueReliableGameplayEventToPlayer(sock, it->second, &retry, sizeof(retry),
                                           retry.eventId, session, totalPacketsOut);
        return;
    }

    // Validate message is not empty
    const char* msg = req->utf8Message;
    // Skip leading whitespace
    while (*msg && (*msg == ' ' || *msg == '\t' || *msg == '\n' || *msg == '\r'))
        ++msg;
    if (!*msg)
    {
        // Send rejection to sender only
        ChatMessageEventPacket reject{};
        reject.header.type = PACKET_CHAT_MESSAGE_EVENT;
        reject.header.tick = tick;
        reject.header.playerId = req->header.playerId;
        reject.requestId = req->requestId;
        reject.messageId = 0;
        reject.serverTick = tick;
        reject.senderType = 1; // Server
        reject.senderEntityId = 0;
        reject.senderAccountId = 0;
        std::strncpy(reject.utf8Message, "u cant send nothing, silly!", sizeof(reject.utf8Message) - 1);
        std::strncpy(reject.senderName, "server", sizeof(reject.senderName) - 1);
        if (it->second.transport)
            it->second.transport->send(&reject, sizeof(reject));
        else
            sendto(sock, (const char*)&reject, sizeof(reject), 0,
                   (sockaddr*)&it->second.addr, sizeof(it->second.addr));
        ++totalPacketsOut;
        return;
    }

    // Rate limit check
    uint64_t remainingTicks = 0;
    if (!gChatRateLimiter.canSend(req->header.playerId, tick, &remainingTicks))
    {
        ChatMessageEventPacket reject{};
        reject.header.type = PACKET_CHAT_MESSAGE_EVENT;
        reject.header.tick = tick;
        reject.header.playerId = req->header.playerId;
        reject.requestId = req->requestId;
        reject.messageId = 0;
        reject.serverTick = tick;
        reject.senderType = 1; // Server
        reject.senderEntityId = 0;
        reject.senderAccountId = 0;
        std::snprintf(reject.utf8Message, sizeof(reject.utf8Message),
                      "too fast! u have %llu ticks left before u can chat again",
                      (unsigned long long)remainingTicks);
        std::strncpy(reject.senderName, "server", sizeof(reject.senderName) - 1);
        if (it->second.transport)
            it->second.transport->send(&reject, sizeof(reject));
        else
            sendto(sock, (const char*)&reject, sizeof(reject), 0,
                   (sockaddr*)&it->second.addr, sizeof(it->second.addr));
        ++totalPacketsOut;
        return;
    }

    gChatRateLimiter.recordSend(req->header.playerId, tick);

    Debug::log(Debug::Category::Chat,
               "[CHAT SERVER RECEIVED] player=%u len=%zu tick=%u\n",
               req->header.playerId, std::strlen(msg), tick);

    // Build accepted message event
    static uint64_t nextMessageId = 1;
    ChatMessageEventPacket event{};
    event.header.type = PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = tick;
    event.header.playerId = req->header.playerId;
    event.requestId = req->requestId;
    event.messageId = nextMessageId++;
    event.serverTick = tick;
    event.utcUnixMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.senderEntityId = req->header.playerId;
    event.senderAccountId = (uint32_t)std::max(0, it->second.vipAccountId);
    event.senderType = 0; // Player
    event.channel = 0; // Global
    std::strncpy(event.senderName, it->second.name.c_str(), sizeof(event.senderName) - 1);
    std::strncpy(event.utf8Message, msg, sizeof(event.utf8Message) - 1);
    Debug::log(Debug::Category::Chat, "[CHAT V2 ACCEPT] messageId=%llu player=%s tick=%u\n",
               (unsigned long long)event.messageId, it->second.name.c_str(), tick);

    // Broadcast to all players (including sender for confirmation) via reliable queue
    uint32_t eventId = nextReliableGameplayEventId();
    event.eventId = eventId;
    it->second.lastChatRequestId = req->requestId;
    it->second.cachedChatEvent = event;
    it->second.hasCachedChatEvent = true;
    for (auto& playerEntry : players)
    {
        uint32_t session = reliableGameplayEventSessionForPlayer(playerEntry.second);
        queueReliableGameplayEventToPlayer(sock, playerEntry.second, &event, sizeof(event),
                                           eventId, session, totalPacketsOut);
    }
    Debug::log(Debug::Category::Chat,
               "[CHAT SERVER BROADCAST] messageId=%llu recipients=%zu\n",
               (unsigned long long)event.messageId, players.size());
}

void handleChatTypingStateRequest(SOCKET sock, const char* buffer, int bytes,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ChatTypingStateRequestPacket))
        return;
    const ChatTypingStateRequestPacket* req =
        reinterpret_cast<const ChatTypingStateRequestPacket*>(buffer);
    auto it = players.find(req->header.playerId);
    if (it == players.end())
        return;

    ChatTypingStateEventPacket event{};
    event.header.type = PACKET_CHAT_TYPING_STATE_EVENT;
    event.header.tick = tick;
    event.header.playerId = req->header.playerId;
    event.playerId = req->header.playerId;
    event.isTyping = req->isTyping;
    event.serverTick = tick;

    for (auto& kv : players)
    {
        if (kv.first == req->header.playerId)
            continue;
        if (kv.second.transport)
            kv.second.transport->send(&event, sizeof(event));
        else
            sendto(sock, (const char*)&event, sizeof(event), 0,
                   (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        ++totalPacketsOut;
    }
}

void handleNpcDamageRequest(SOCKET sock, const char* buffer, int bytes,
                            const sockaddr_in& from,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(NpcDamageRequestPacket))
        return;
    const NpcDamageRequestPacket* req =
        reinterpret_cast<const NpcDamageRequestPacket*>(buffer);
    auto shooterIt = players.find(req->header.playerId);
    if (shooterIt == players.end() ||
        !sameAddress(shooterIt->second.addr, from))
        return;

    auto npcIt = npcs.find(req->npcEntityId);
    if (npcIt == npcs.end())
    {
        printf("%s [NET NPC DAMAGE] shooter=%u npcId=%u accepted=0 reason=npc-not-found\n",
               serverTimestamp(), req->header.playerId, req->npcEntityId);
        return;
    }

    ServerNpc& target = npcIt->second;
    const int clamped = std::clamp((int)req->damage, 1, 200);
    target.health -= clamped;
    const bool killed = target.health <= 0;
    if (killed)
    {
        target.health = 0;
        printf("%s [NET NPC KILL] shooter=%u npcId=%u name=\"%s\"\n",
               serverTimestamp(), req->header.playerId,
               target.entityId, target.name.c_str());
        // Heal the shooter to full health
        auto shooterIt = players.find(req->header.playerId);
        if (shooterIt != players.end())
            shooterIt->second.health = serverMaxHp();
    }

    const glm::vec3 origin(req->originX, req->originY, req->originZ);
    const glm::vec3 hit(req->hitX, req->hitY, req->hitZ);
    const glm::vec3 dir(req->dirX, req->dirY, req->dirZ);
    const glm::vec3 normal(req->normalX, req->normalY, req->normalZ);
    broadcastNpcDamageEvent(
        sock, players, tick, totalPacketsOut, req->header.playerId,
        target, clamped, killed, origin, hit, dir, normal, req->weapon);
}

void handleServerCommand(SOCKET sock, const sockaddr_in& from,
                         const char* buffer, int bytes,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ServerCommandPacket))
        return;
    ServerCommandPacket* cmd =
        const_cast<ServerCommandPacket*>(reinterpret_cast<const ServerCommandPacket*>(buffer));
    auto it = players.find(cmd->header.playerId);
    if (it == players.end())
        return;

    cmd->commandText[239] = '\0';
    const std::string commandStr(cmd->commandText);

    // Acknowledge the sender so the host gets terminal feedback.
    const auto ack = [&](bool accepted, const char* status) {
        ServerCommandResultPacket res{};
        res.header.type = PACKET_SERVER_COMMAND_RESULT;
        res.header.tick = tick;
        res.header.playerId = it->second.id;
        res.accepted = accepted ? 1 : 0;
        std::memset(res.statusText, 0, sizeof(res.statusText));
        std::strncpy(res.statusText, status, sizeof(res.statusText) - 1);
        if (it->second.transport)
            it->second.transport->send(&res, sizeof(res));
        else
            sendto(sock, (const char*)&res, sizeof(res), 0,
                   (sockaddr*)&from, sizeof(from));
        ++totalPacketsOut;
    };

    // Host-gate: only the player whose name matches the server host (or the
    // first joiner when no host name is set) may issue server-authoritative
    // commands. This also stops any client from deleting all NPCs.
    if (!it->second.isHost)
    {
        Debug::warn(Debug::Category::Networking,
            "%s [SERVER COMMAND REJECT] playerId=%u name=\"%s\" cmd=\"%s\" reason=not-host\n",
            serverTimestamp(), it->second.id, it->second.name.c_str(),
            commandStr.c_str());
        ack(false, "rejected: not host");
        return;
    }

    printf("%s [SERVER COMMAND] playerId=%u name=\"%s\" cmd=\"%s\"\n",
           serverTimestamp(), it->second.id, it->second.name.c_str(),
           commandStr.c_str());

    ServerGameOverrides& ov = serverGameOverrides();

    if (commandStr == "modelist")
    {
        auto& cfg = CommunityServerConfig::instance();
        if (cfg.modes().empty()) cfg.load();
        std::string list;
        for (size_t i = 0; i < cfg.modes().size(); ++i) {
            if (i) list += " | ";
            list += std::to_string(i + 1) + "=" + cfg.modes()[i].id;
        }
        ack(true, list.c_str());
    }
    else if (commandStr.rfind("modepick ", 0) == 0)
    {
        auto& cfg = CommunityServerConfig::instance();
        if (cfg.modes().empty()) cfg.load();
        const int index = std::atoi(commandStr.c_str() + 9) - 1;
        if (index < 0 || index >= (int)cfg.modes().size()) ack(false, "rejected: invalid mode number");
        else {
            serverCommunitySetMode(cfg.modes()[(size_t)index].id);
            ack(true, ("applied: modepick " + cfg.modes()[(size_t)index].id).c_str());
        }
    }
    else if (commandStr.rfind("modestart ", 0) == 0)
    {
        auto& cfg = CommunityServerConfig::instance();
        if (cfg.modes().empty()) cfg.load();
        const int index = std::atoi(commandStr.c_str() + 10) - 1;
        if (index < 0 || index >= (int)cfg.modes().size()) ack(false, "rejected: invalid mode number");
        else {
            const auto& mode = cfg.modes()[(size_t)index];
            serverCommunitySetMode(mode.id);
            serverCommunityStartMatch();
            ack(true, ("started: " + mode.id + " (" + mode.name + ")").c_str());
        }
    }
    else if (commandStr == "maplist")
    {
        const auto catalog = scanMapCatalog();
        std::string list;
        for (size_t i = 0; i < catalog.maps.size(); ++i) {
            if (i) list += " | ";
            list += std::to_string(i + 1) + "=" + catalog.maps[i].displayName;
        }
        ack(true, list.empty() ? "no maps found" : list.c_str());
    }
    else if (commandStr.rfind("mapchange ", 0) == 0)
    {
        const auto catalog = scanMapCatalog();
        const int index = std::atoi(commandStr.c_str() + 10) - 1;
        if (index < 0 || index >= (int)catalog.maps.size()) ack(false, "rejected: invalid map number");
        else {
            const std::string mapId = std::filesystem::path(catalog.maps[(size_t)index].assetPath).stem().string();
            serverDuelRequestMapChange(mapId);
            ack(true, ("applied: mapchange " + mapId).c_str());
        }
    }
    else if (commandStr == "npcdifflist")
    {
        std::vector<std::filesystem::path> presets;
        std::string list;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("config/npcpresets", ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".json") presets.push_back(entry.path());
        }
        std::sort(presets.begin(), presets.end());
        for (size_t i = 0; i < presets.size(); ++i) {
            if (i) list += " | ";
            list += std::to_string(i + 1) + "=" + presets[i].filename().string();
        }
        ack(true, list.empty() ? "no NPC presets found" : list.c_str());
    }
    else if (commandStr.rfind("npcdiffload ", 0) == 0)
    {
        std::vector<std::filesystem::path> presets;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("config/npcpresets", ec))
            if (entry.is_regular_file(ec) && entry.path().extension() == ".json") presets.push_back(entry.path());
        std::sort(presets.begin(), presets.end());
        const int index = std::atoi(commandStr.c_str() + 12) - 1;
        if (index < 0 || index >= (int)presets.size()) ack(false, "rejected: invalid NPC preset number");
        else if (NpcDifficultyConfig::instance().load(presets[(size_t)index].string()))
            ack(true, ("applied: npcdiffload " + presets[(size_t)index].filename().string()).c_str());
        else ack(false, "rejected: NPC preset could not be loaded");
    }
    else if (commandStr == "npc_delete_all")
    {
        printf("%s [SERVER COMMAND] npc_delete_all by playerId=%u count=%zu\n",
               serverTimestamp(), it->second.id, npcs.size());
        npcs.clear();
        ack(true, "applied: npc_delete_all");
    }
    else if (commandStr.rfind("healthall ", 0) == 0)
    {
        const std::string arg = commandStr.substr(10);
        bool applied = false;
        if (arg == "default" || arg == "reset")
        {
            ov.maxHpOverride = 0;
            Debug::warn(Debug::Category::Networking,
                "%s [SERVER COMMAND] healthall default (max HP 100)\n", serverTimestamp());
            ack(true, "applied: healthall default (max HP 100)");
            applied = true;
        }
        else
        {
            try
            {
                int value = std::stoi(arg);
                ov.maxHpOverride = value > 0 ? value : 0;
                Debug::warn(Debug::Category::Networking,
                    "%s [SERVER COMMAND] healthall=%d (all future spawns + kill-heals)\n",
                    serverTimestamp(), ov.maxHpOverride);
                ack(true, ("applied: healthall " + std::to_string(ov.maxHpOverride)).c_str());
                applied = true;
            }
            catch (...)
            {
                Debug::warn(Debug::Category::Networking,
                    "%s [SERVER COMMAND] healthall invalid value=\"%s\"\n",
                    serverTimestamp(), arg.c_str());
                ack(false, "rejected: invalid healthall value");
            }
        }
        if (applied)
        {
            // Retroactively heal every alive entity to the new max so
            // "healthall 999" immediately shows 999/999, not just future spawns.
            const int effectiveMax = serverMaxHp();
            for (auto& kv : players)
                if (!kv.second.dead)
                    kv.second.health = effectiveMax;
            for (auto& kv : npcs)
                if (kv.second.health > 0)
                    kv.second.health = effectiveMax;
        }
    }
    else if (commandStr == "setspawn_set")
    {
        // The host's current position becomes the spawn for every entity.
        ov.spawnOverridePosition = it->second.pos;
        ov.spawnOverrideEnabled = true;
        Debug::warn(Debug::Category::Networking,
            "%s [SERVER COMMAND] setspawn_set pos=(%.2f,%.2f,%.2f) enabled=1\n",
            serverTimestamp(), ov.spawnOverridePosition.x,
            ov.spawnOverridePosition.y, ov.spawnOverridePosition.z);
        ack(true, "applied: setspawn_set");
    }
    else if (commandStr.rfind("setspawn ", 0) == 0)
    {
        ov.spawnOverrideEnabled = commandStr.substr(9) == "1";
        Debug::warn(Debug::Category::Networking,
            "%s [SERVER COMMAND] setspawn=%d\n",
            serverTimestamp(), (int)ov.spawnOverrideEnabled);
        ack(true, ov.spawnOverrideEnabled ? "applied: setspawn 1" : "applied: setspawn 0");
    }
    else if (commandStr.rfind("changemap ", 0) == 0)
    {
        // Live map change without restarting (duel mode). Applied next tick by
        // serverDuelTick, which owns the world + player teleports.
        const std::string mapId = commandStr.substr(10);
        serverDuelRequestMapChange(mapId);
        Debug::warn(Debug::Category::Duel,
            "%s [SERVER COMMAND] changemap -> %s by playerId=%u\n",
            serverTimestamp(), mapId.c_str(), it->second.id);
        ack(true, ("applied: changemap " + mapId).c_str());
    }
    else
    {
        printf("%s [SERVER COMMAND] unknown cmd=\"%s\" from playerId=%u\n",
               serverTimestamp(), commandStr.c_str(), it->second.id);
        ack(false, "rejected: unknown command");
    }
}

// Broadcast a peer's connection-state change (disconnected / reconnected) to
// every other client so observers can show the red reconnect effect on the
// frozen body and the green effect on recovery. The target player is skipped
// (they cannot receive it while disconnected anyway).
static void broadcastPlayerConnectionState(
    SOCKET sock,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t targetPlayerId,
    bool connected,
    uint64_t disconnectedAtMs,
    uint64_t& totalPacketsOut)
{
    PlayerConnectionStatePacket packet{};
    packet.header.type = PACKET_PLAYER_CONNECTION_STATE;
    packet.header.playerId = targetPlayerId;
    packet.connected = connected ? 1 : 0;
    packet.disconnectedAtMs = disconnectedAtMs;
    for (const auto& kv : players)
    {
        if (kv.first == targetPlayerId)
            continue;
        if (serverSendToPlayer(sock, kv.second, &packet, sizeof(packet)))
            ++totalPacketsOut;
    }
}

void handleClientTimeout(std::unordered_map<uint32_t, ServerPlayer>& players,
                         SOCKET sock, uint32_t tick, uint64_t& totalPacketsOut)
{
    const auto& cfg = NetworkingConfig::instance().data();
    const uint64_t staleThreshold = (uint64_t)cfg.timeouts.stalePacketThresholdMs;
    const uint64_t hardTimeout = (uint64_t)cfg.timeouts.clientTimeoutMs;
    const uint64_t graceMs = (uint64_t)cfg.retries.reconnectGraceMs;
    const uint64_t now = nowMs();

    std::vector<uint32_t> toRemove;
    for (auto& kv : players)
    {
        ServerPlayer& p = kv.second;
        const uint64_t silentMs = now - p.lastHeardMs;

        if (silentMs <= staleThreshold)
        {
            // Healthy. If this player had been marked stale, they recovered:
            // unfreeze, clear grace, and broadcast "reconnected" exactly once.
            if (p.connectionStale)
            {
                p.connectionStale = false;
                p.disconnectedSinceMs = 0;
                if (p.connectionStateNotified)
                {
                    p.connectionStateNotified = false;
                    printf("%s [SERVER RECONNECTED] id=%u name=\"%s\" after %llums silent\n",
                           serverTimestamp(), p.id, p.name.c_str(),
                           (unsigned long long)(now - p.lastHeardMs));
                    broadcastPlayerConnectionState(sock, players, p.id, true,
                                                   0, totalPacketsOut);
                }
            }
            continue;
        }

        // First crossing the stale threshold: freeze the body and start grace.
        if (!p.connectionStale)
        {
            p.connectionStale = true;
            p.disconnectedSinceMs = now;
            printf("%s [SERVER STALE] id=%u name=\"%s\" lastHeard=%llums ago — body frozen, "
                   "slot kept for reconnect\n",
                   serverTimestamp(), p.id, p.name.c_str(),
                   (unsigned long long)silentMs);
        }

        // Full give-up: slot dies clientTimeoutMs + graceMs after the last
        // heard packet, matching the client's own 60s reconnect window so a
        // client still trying inside its grace never finds its token revoked.
        const uint64_t deadline = p.lastHeardMs + hardTimeout + graceMs;
        if (now >= deadline)
        {
            printf("%s [NET DISCONNECT] reason=grace_expired id=%u name=\"%s\" "
                   "silent=%llums slotReleased=1\n",
                   serverTimestamp(), p.id, p.name.c_str(),
                   (unsigned long long)silentMs);
            // Broadcast leave message to all remaining players
            {
                std::string leaveMsg = std::string(p.name) + " left the game";
                broadcastServerChatMessage(sock, players, 0, totalPacketsOut, leaveMsg.c_str());
            }
            if (p.transport)
                p.transport->close();
            toRemove.push_back(p.id);
            continue;
        }

        // Once the connection is judged truly lost (past the hard timeout),
        // notify every other client exactly once so they show the red effect.
        if (!p.connectionStateNotified && silentMs > hardTimeout)
        {
            p.connectionStateNotified = true;
            printf("%s [SERVER PEER LOST] id=%u name=\"%s\" silent=%llums — notifying peers\n",
                   serverTimestamp(), p.id, p.name.c_str(),
                   (unsigned long long)silentMs);
            broadcastPlayerConnectionState(sock, players, p.id, false,
                                           p.disconnectedSinceMs, totalPacketsOut);
        }
    }

    for (uint32_t id : toRemove)
        players.erase(id);
}

void checkVoidDeath(std::unordered_map<uint32_t, ServerPlayer>& players,
                    std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const VoidDeathConfig& vdc = getVoidDeathConfig();
    if (!vdc.enabled)
        return;

    for (auto& kv : players)
    {
        if (!kv.second.dead && kv.second.pos.z < vdc.killZ &&
            !kv.second.connectionStale)   // never kill a body while it is reconnecting
        {
            kv.second.health = 0;
            kv.second.dead = true;
            kv.second.respawnSeconds = 0.01f;  // instant respawn (next server tick)
            kv.second.vel = glm::vec3(0.0f);
            printf("%s [SERVER VOID DEATH] playerId=%u name=%s z=%.1f killZ=%.1f\n",
                   serverTimestamp(), kv.second.id, kv.second.name.c_str(),
                   kv.second.pos.z, vdc.killZ);
        }
    }
    for (auto& kv : npcs)
    {
        if (kv.second.pos.z < vdc.killZ)
        {
            printf("%s [SERVER VOID DEATH] npcId=%u z=%.1f killZ=%.1f\n",
                   serverTimestamp(), kv.second.entityId,
                   kv.second.pos.z, vdc.killZ);
            kv.second.health = 0;
            kv.second.vel = glm::vec3(0.0f);
            // No position overwrite here: simulateSharedNpcs respawns the real
            // body at its actual map spawn point (respawnServerNpc). The old
            // hardcoded (1+(id-1)*1.5, 5, 30) sent NPCs to a placeholder pos
            // that had nothing to do with the map.
        }
    }
}

} // namespace MimitaNet
