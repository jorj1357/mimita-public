// 09 22 2026
/* purpose
* Implements the generic gameplay-event transport primitive.
* See server-event-broadcast.h for scope.
*/
#include "network/server-event-broadcast.h"

#include <cstddef>
#include <unordered_map>

#include "network/server-context.h"
#include "network/server.h"

namespace MimitaNet {

void serverEventNextId(GameReliableEventTicketV1* out)
{
    if (!out)
        return;
    out->eventId = nextReliableGameplayEventId();
    out->eventSessionId = serverReliableEventSessionId();
}

void serverEventBroadcast(const GameEventBroadcastV1& request)
{
    ServerContextV1* context = activeServerContext();
    if (!context || !context->players || !context->totalPacketsOut)
        return;
    if (request.payloadSize == 0 ||
        request.payloadSize > (std::uint32_t)GAME_EVENT_BROADCAST_MAX_BYTES)
        return;
    auto& players =
        *static_cast<std::unordered_map<std::uint32_t, ServerPlayer>*>(context->players);
    const SOCKET sock = static_cast<SOCKET>(context->sock);
    std::uint64_t& totalPacketsOut = *context->totalPacketsOut;
    const void* data = request.payload;
    const int size = static_cast<int>(request.payloadSize);

    if (request.flags & GAME_EVENT_BROADCAST_RELIABLE)
    {
        queueReliableGameplayEventToAll(sock, players, data, (std::size_t)size,
                                        request.eventId, request.eventSessionId,
                                        totalPacketsOut);
        return;
    }

    for (const auto& pe : players)
    {
        if ((request.flags & GAME_EVENT_BROADCAST_EXCLUDE_OWNER) &&
            pe.second.id == request.ownerPlayerId)
            continue;
        if (pe.second.transport)
            pe.second.transport->send(data, size);
        else
            sendto(sock, (const char*)data, size, 0,
                   (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
        ++totalPacketsOut;
    }
}

void serverPacketReply(std::uint32_t connectionId, const void* bytes,
                       std::uint32_t size)
{
    ServerContextV1* context = activeServerContext();
    if (!context || !context->players || !bytes || size == 0)
        return;
    auto& players =
        *static_cast<std::unordered_map<std::uint32_t, ServerPlayer>*>(context->players);
    auto it = players.find(connectionId);
    if (it == players.end())
        return;
    const SOCKET sock = static_cast<SOCKET>(context->sock);
    const int byteCount = static_cast<int>(size);
    if (it->second.transport)
        it->second.transport->send(bytes, byteCount);
    else
        sendto(sock, (const char*)bytes, byteCount, 0,
               (sockaddr*)&it->second.addr, sizeof(it->second.addr));
    if (context->totalPacketsOut)
        ++(*context->totalPacketsOut);
}

} // namespace MimitaNet
