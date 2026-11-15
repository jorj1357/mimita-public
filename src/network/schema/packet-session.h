// 2026-11-14
// MiMITA session packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

struct HelloPacket
{
    PacketHeader header;
    char name[MAX_NAME_BYTES];
    char avatarName[16];
};

static_assert(sizeof(HelloPacket) == 68, "HelloPacket wire size changed");

struct WelcomePacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_MAP_ID_BYTES];
    char runtimeRunId[MAX_RUNTIME_RUN_ID_BYTES] = {};
    char runtimeBuildId[MAX_RUNTIME_BUILD_ID_BYTES] = {};
};

static_assert(sizeof(WelcomePacket) == 352, "WelcomePacket wire size changed");

struct ProfilePacket
{
    PacketHeader header;
    char name[MAX_NAME_BYTES];
};

struct PingPacket
{
    PacketHeader header;
    uint64_t clientTimeMs = 0;
};

struct DisconnectPacket
{
    PacketHeader header;
};

// Server → all clients: a peer player's connection was lost or recovered.
// Clients show a red "connection lost... reconnecting X.XXs" effect on the
// frozen body while disconnected, and a green effect on reconnect.
struct PlayerConnectionStatePacket
{
    PacketHeader header;
    uint8_t connected = 0;      // 0 = disconnected, 1 = reconnected
    uint8_t reserved[3] = {};
    uint64_t disconnectedAtMs = 0; // server wall-clock when the loss began
};

static_assert(sizeof(PlayerConnectionStatePacket) == 32,
              "PlayerConnectionStatePacket wire size changed");

// ── Duel match state ────────────────────────────────────────────────────
// Broadcast by the server during a --duel match. Carries phase, per-team
// score, the goal, the countdown/rematch timers, and team names. Clients
// derive "my score" by comparing their assigned player id to playerAId.
// Also supports FFA and TDM match modes with multi-player scoring.

struct MapChangePacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t duelId = 0;
    uint32_t mapVersion = 0;
    char mapId[MAX_MAP_ID_BYTES];
    // Canonical Infinite World identity. Zero for non-procedural maps.
    uint32_t worldSeed = 0;
    char generatorVersion[16] = {};
    uint64_t descriptorHash = 0;
};

// One reliable fragment of the authoritative map config JSON. The payload is
// sized so the complete packet remains below MAX_GAME_DATAGRAM_BYTES.
constexpr uint32_t MAP_CONFIG_SNAPSHOT_CHUNK_BYTES = 1024;
constexpr uint32_t MAP_CONFIG_SNAPSHOT_MAX_BYTES = 128 * 1024;

struct MapConfigSnapshotPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    char mapId[MAX_MAP_ID_BYTES];
    uint64_t configRevision = 0;
    uint32_t geometryRevision = 0;
    uint64_t geometrySignature = 0;
    uint64_t contentHash = 0;
    uint32_t totalBytes = 0;
    uint32_t offset = 0;
    uint16_t chunkBytes = 0;
    uint16_t reserved = 0;
    uint8_t data[MAP_CONFIG_SNAPSHOT_CHUNK_BYTES] = {};
};

static_assert(sizeof(MapConfigSnapshotPacket) <= MAX_GAME_DATAGRAM_BYTES,
              "MapConfigSnapshotPacket exceeds the game datagram budget");

struct JoinRequestPacket
{
    PacketHeader header;
    char joinToken[MAX_JOIN_TOKEN_BYTES];
    char vipJoinTicket[MAX_VIP_JOIN_TICKET_BYTES];
    uint32_t accountId = 0;
    char name[MAX_NAME_BYTES];
    char avatarName[16];
    char password[64];
};

static_assert(sizeof(JoinRequestPacket) == 264, "JoinRequestPacket wire size changed");

struct JoinAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_MAP_ID_BYTES];
    char runtimeRunId[MAX_RUNTIME_RUN_ID_BYTES] = {};
    char runtimeBuildId[MAX_RUNTIME_BUILD_ID_BYTES] = {};
};

static_assert(sizeof(JoinAcceptPacket) == 352, "JoinAcceptPacket wire size changed");

struct JoinRejectPacket
{
    PacketHeader header;
    uint8_t reason = 0; // 1=full, 2=bad-token, 3=banned, 4=version-mismatch, 5=wrong-password
    uint8_t reserved[3] = {};
};

struct ReconnectRequestPacket
{
    PacketHeader header;
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
};

static_assert(sizeof(ReconnectRequestPacket) == 84, "ReconnectRequestPacket wire size changed");

struct ReconnectAcceptPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    uint32_t reliableEventSessionId = 0;
    float tickRate = 60.0f;
    uint32_t spawnGeneration = 0;
    char approvedName[MAX_NAME_BYTES];
    char reconnectToken[MAX_RECONNECT_TOKEN_BYTES];
    char mapId[MAX_MAP_ID_BYTES] = {};
    // Restored state
    int32_t restoredHealth = 100;
    int32_t restoredKills = 0;
    int32_t restoredDeaths = 0;
    float restorePx = 0.0f;
    float restorePy = 0.0f;
    float restorePz = 0.0f;
    char runtimeRunId[MAX_RUNTIME_RUN_ID_BYTES] = {};
    char runtimeBuildId[MAX_RUNTIME_BUILD_ID_BYTES] = {};
};

static_assert(sizeof(ReconnectAcceptPacket) == 380, "ReconnectAcceptPacket wire size changed");

struct ClientMapReadyPacket
{
    PacketHeader header;
    uint32_t assignedPlayerId = 0;
    char mapId[MAX_MAP_ID_BYTES];
};

static_assert(sizeof(ClientMapReadyPacket) == 88, "ClientMapReadyPacket wire size changed");

#pragma pack(pop)

}  // namespace MimitaNet
