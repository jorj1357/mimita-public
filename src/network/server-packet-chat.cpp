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
#include "network/multiplayer-context.h"
#include "network/chat-rate-limiter.h"
#include "void-death/void-death.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>

// Global chat rate limiter instance
static ChatRateLimiter gChatRateLimiter;

namespace MimitaNet {

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

    // Build accepted message event
    static uint64_t nextMessageId = 1;
    ChatMessageEventPacket event{};
    event.header.type = PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = tick;
    event.header.playerId = req->header.playerId;
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

    // Broadcast to all players (including sender for confirmation)
    for (const auto& playerEntry : players)
    {
        if (playerEntry.second.transport)
            playerEntry.second.transport->send(&event, sizeof(event));
        else
            sendto(sock, (const char*)&event, sizeof(event), 0,
                   (sockaddr*)&playerEntry.second.addr,
                   sizeof(playerEntry.second.addr));
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

    if (commandStr == "npc_delete_all")
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
                         SOCKET sock, uint64_t& totalPacketsOut)
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
            kv.second.pos = {1.0f + (float)(kv.second.entityId - 1) * 1.5f, 5.0f, 30.0f};
            kv.second.vel = glm::vec3(0.0f);
        }
    }
}

} // namespace MimitaNet
