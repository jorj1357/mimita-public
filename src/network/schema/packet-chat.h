// 2026-11-14
// Chat packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

struct ChatPacket
{
    PacketHeader header;
    char senderName[MAX_NAME_BYTES];
    char text[240];
};

// Chat v2: client request to send a message
struct ChatRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint64_t clientSimulationTick = 0;
    char utf8Message[256] = {};
};

// Chat v2: server broadcasts this to all clients when a message is accepted
struct ChatMessageEventPacket
{
    PacketHeader header;
    // Reliable event dedup fields (must be the first two uint32s after the
    // header — the reliable-queue overwrites them per player and the client
    // ACKs them so every chat message is delivered even under loss).
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t requestId = 0;
    uint64_t messageId = 0;
    uint64_t serverTick = 0;
    int64_t utcUnixMilliseconds = 0;
    uint32_t senderEntityId = 0;
    uint32_t senderAccountId = 0;
    uint8_t senderType = 0; // 0=Player, 1=Server
    uint8_t channel = 0;    // 0=Global
    char senderName[MAX_NAME_BYTES] = {};
    char utf8Message[256] = {};
};

// Client → Server: typing state notification
struct ChatTypingStateRequestPacket
{
    PacketHeader header;
    bool isTyping = false;
    uint32_t sequence = 0;
};

// Server → All: typing state broadcast
struct ChatTypingStateEventPacket
{
    PacketHeader header;
    uint32_t playerId = 0;
    bool isTyping = false;
    uint64_t serverTick = 0;
};

#pragma pack(pop)

}  // namespace MimitaNet
