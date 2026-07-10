#include "network/server.h"
#include "network/multiplayer-context.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

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
        sendto(sock, (const char*)chat, sizeof(ChatPacket), 0,
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
    }

    NpcDamageEventPacket event{};
    event.header.type = PACKET_NPC_DAMAGE_EVENT;
    event.header.tick = tick;
    event.header.playerId = req->header.playerId;
    event.npcEntityId = req->npcEntityId;
    event.shooterPlayerId = req->header.playerId;
    event.damage = clamped;
    event.npcHealth = target.health;
    event.killed = killed ? 1 : 0;
    event.originX = req->originX; event.originY = req->originY; event.originZ = req->originZ;
    event.hitX = req->hitX; event.hitY = req->hitY; event.hitZ = req->hitZ;
    event.dirX = req->dirX; event.dirY = req->dirY; event.dirZ = req->dirZ;
    event.normalX = req->normalX; event.normalY = req->normalY; event.normalZ = req->normalZ;
    event.effectFlags = req->effectFlags;
    event.weapon = req->weapon;
    event.impactType = req->impactType;

    for (const auto& pe : players)
    {
        sendto(sock, (const char*)&event, sizeof(event), 0,
               (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
        ++totalPacketsOut;
    }

    if (killed)
        npcs.erase(npcIt);
}

void handleServerCommand(const char* buffer, int bytes,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs)
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

    printf("%s [SERVER COMMAND] playerId=%u name=\"%s\" cmd=\"%s\"\n",
           serverTimestamp(), it->second.id, it->second.name.c_str(),
           commandStr.c_str());

    if (commandStr == "npc_delete_all")
    {
        printf("%s [SERVER COMMAND] npc_delete_all by playerId=%u count=%zu\n",
               serverTimestamp(), it->second.id, npcs.size());
        npcs.clear();
    }
    else
    {
        printf("%s [SERVER COMMAND] unknown cmd=\"%s\" from playerId=%u\n",
               serverTimestamp(), commandStr.c_str(), it->second.id);
    }
}

void handleClientTimeout(std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (auto it = players.begin(); it != players.end(); )
    {
        const uint64_t silentMs = nowMs() - it->second.lastHeardMs;
        if (silentMs > CLIENT_TIMEOUT_MS)
        {
            printf("%s [NET DISCONNECT] reason=heartbeat_timeout id=%u name=\"%s\" lastHeard=%llums ago ping=%dms\n",
                   serverTimestamp(), it->second.id, it->second.name.c_str(),
                   (unsigned long long)silentMs, it->second.pingMs);
            it = players.erase(it);
        }
        else
            ++it;
    }
}

void checkVoidDeath(std::unordered_map<uint32_t, ServerPlayer>& players,
                    std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const VoidDeathConfig& vdc = getVoidDeathConfig();
    if (!vdc.enabled)
        return;

    for (auto& kv : players)
    {
        if (!kv.second.dead && kv.second.pos.z < vdc.killZ)
        {
            kv.second.health = 0;
            kv.second.dead = true;
            kv.second.respawnSeconds = 2.0f;
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
