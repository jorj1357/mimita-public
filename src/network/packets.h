// 08 24 2026, 10 50
/* purpose
* Defines MiMITA network packet IDs, fixed wire structs, flags, and datagram limits.
* Carries compact gameplay state for join, input, snapshots, weapons, projectiles, and lifecycle.
* Keeps packet layouts explicit and protocol-versioned for client/server compatibility.
* Does NOT own transport sockets, validation policy, gameplay simulation, or rendering.
* Carries validated client-first hitscan aggregates; the server caps and validates them.
* Does NOT encode private runtime-only Player or MovementState object layouts.
*/

#pragma once

#include <cstdint>

#include "network/schema/packet-header.h"
#include "network/schema/packet-common.h"
#include "network/schema/packet-session.h"
#include "network/schema/packet-snapshot.h"
#include "network/schema/packet-combat.h"
#include "network/schema/packet-player.h"
#include "network/schema/packet-gamemode.h"
#include "network/schema/packet-physical.h"
#include "network/schema/packet-vehicle.h"
#include "network/schema/packet-input.h"
#include "network/schema/packet-chat.h"
#include "network/schema/packet-avatar.h"

namespace MimitaNet {

#pragma pack(push, 1)

enum PacketType : uint8_t
{
    PACKET_HELLO = 1,
    PACKET_WELCOME = 2,
    PACKET_INPUT = 3,
    PACKET_SNAPSHOT = 4,
    PACKET_DISCONNECT = 5,
    PACKET_PING = 6,
    PACKET_PLAYER_LIST = 7,
    PACKET_PROFILE = 8,
    PACKET_ENTITY_SPAWN = 9,
    PACKET_ENTITY_DESPAWN = 10,
    PACKET_SPAWN_NPC_REQUEST = 11,
    PACKET_TELEPORT_REQUEST = 12,
    PACKET_EXPLODE_REQUEST = 13,
    PACKET_SHOT_REQUEST = 14,
    PACKET_SHOT_EVENT = 15,
    PACKET_CHAT_MESSAGE = 16,
    PACKET_NPC_DAMAGE_REQUEST = 17,
    PACKET_NPC_DAMAGE_EVENT = 18,
    PACKET_SERVER_COMMAND = 19,
    // ── Migration packet types ──────────────────────────────────
    PACKET_JOIN_REQUEST = 20,
    PACKET_JOIN_ACCEPT = 21,
    PACKET_JOIN_REJECT = 22,
    PACKET_RECONNECT_REQUEST = 23,
    PACKET_RECONNECT_ACCEPT = 24,
    PACKET_DISAGREEMENT = 25,
    PACKET_CLIENT_MAP_READY = 26,
    PACKET_PROJECTILE_SPAWN_EVENT = 28,
    PACKET_PROJECTILE_STATE_EVENT = 29,
    PACKET_PROJECTILE_EXPLODE_EVENT = 30,
    PACKET_PROJECTILE_DESPAWN_EVENT = 31,
    PACKET_MELEE_HIT_EVENT = 33,
    PACKET_PELLET_BLAST_REQUEST = 34,
    PACKET_PELLET_BLAST_EVENT = 35,
    PACKET_GODBALL_STATE = 36,
    PACKET_PROJECTILE_FIRE_RESULT = 37,
    // ── Generic attack pipeline ─────────────────────────────────────
    PACKET_ATTACK_REQUEST = 38,
    PACKET_ATTACK_RESULT = 39,
    // ── Authoritative reload ────────────────────────────────────────
    PACKET_RELOAD_REQUEST = 40,
    PACKET_RELOAD_RESULT = 41,
    // ── Respawn ─────────────────────────────────────────────────────
    PACKET_RESPAWN_REQUEST = 42,
    PACKET_PLAYER_RESPAWNED = 43,
    PACKET_SPAWN_ACK = 44,
    PACKET_SPAWN_ACTIVATED = 45,
    PACKET_RELIABLE_EVENT_ACK = 46,
    PACKET_DAMAGE_CONFIRMED_EVENT = 47,
    // ── Chat v2 ───────────────────────────────────────────────────────
    PACKET_CHAT_REQUEST = 48,
    PACKET_CHAT_MESSAGE_EVENT = 49,
    PACKET_CHAT_TYPING_STATE_REQUEST = 50,
    PACKET_CHAT_TYPING_STATE_EVENT = 51,
    // ── VIP name style sync ───────────────────────────────────────────
    PACKET_VIP_STYLE_EVENT = 52,
    // ── Peer connection state (server → clients) ─────────────────────
    // Tells every client when a peer player's connection was lost (so they can
    // show a red reconnect effect on the frozen body) or recovered (green
    // "reconnected" effect). `connected` is 0 on disconnect, 1 on reconnect.
    PACKET_PLAYER_CONNECTION_STATE = 53,
    // ── Server command result (server → sender) ───────────────────────
    // Confirms whether a host-only command (healthall / setspawn / etc.) was
    // applied or rejected, so the host gets feedback in their terminal.
    PACKET_SERVER_COMMAND_RESULT = 54,
    // ── Duel match state (server → clients) ────────────────────────────
    // Drives the first-to-goal duel: countdown, live score, match end, and
    // the post-match rematch window. One source of truth per match.
    PACKET_DUEL_STATE = 55,
    // Server → the surviving player: where their opponent just respawned so
    // they can draw a "look here" tracer.
    PACKET_DUEL_ENEMY_SPAWN = 56,
    // ── Duel rematch now (client → server) ─────────────────────────────
    // A player pressed Space on the win/lose screen to skip the rematch
    // timer and start the next duel immediately.
    PACKET_DUEL_REMATCH_REQUEST = 57,
    // ── Live map change (server → clients) ─────────────────────────────
    // The server swapped the map without restarting; clients must load the
    // new map and re-complete the spawn handshake.
    PACKET_MAP_CHANGE = 58,
    PACKET_SPYKNIFE_HIT_CLAIM = 59,
    PACKET_GODBALL_HIT_CLAIM = 60,
    PACKET_SERVER_NOTIFICATION = 61,
    // Avatar identity is transferred separately from snapshots. The manifest
    // contains avatar.json and SHA-256 references to image assets; image bytes
    // are requested only when the receiving client cache misses them.
    PACKET_AVATAR_MANIFEST = 62,
    PACKET_AVATAR_ASSET_REQUEST = 63,
    PACKET_AVATAR_ASSET_CHUNK = 64,
    PACKET_PROGRESSION_EVENT = 65,
    // ── Bomb Tag state (server → clients) ───────────────────────────
    // Replicates authoritative bomb holder, timer, and inactive state.
    PACKET_BOMB_TAG_STATE = 66,
    // ── Bomb Tag pass event (server → all clients) ──────────────────
    // Broadcast on every successful bomb transfer for pass visualization.
    PACKET_BOMB_TAG_PASS_EVENT = 67,
    // ── Authoritative kill event (server → all clients) ─────────────
    // Single source for the live killfeed/chat line. Every viewer (killer,
    // victim, and observers) receives the same entry exactly once.
    PACKET_KILL_EVENT = 68,
    // ── Ragdoll / physical-body limb state (owner → server → clients) ──
    // The owning client sends its six limb world transforms at a low rate; the
    // server relays to everyone else; remote clients interpolate and apply them
    // to the replica skeleton (render + client hitboxes).
    PACKET_RAGDOLL_STATE = 69,
    // ── Destructible physical-entity replication (server → clients) ──
    // Spawn/despawn/cut are reliable gameplay events; state is an unreliable
    // per-tick transform broadcast. Entity ids are the server's runtime
    // PhysicalEntity ids, so a client mirror never becomes an authority.
    PACKET_PHYSICAL_ENTITY_SPAWN = 70,
    PACKET_PHYSICAL_ENTITY_DESPAWN = 71,
    PACKET_PHYSICAL_ENTITY_STATE = 72,
    PACKET_ENTITY_CUT_EVENT = 73,
    // ── Disaster state (server → clients) ───────────────────────────
    // Replicates the active mode-pack disaster: identity, seed, start tick,
    // duration, and resolved winner. The client only displays this
    // server-authoritative state; it never decides disaster outcomes.
    PACKET_DISASTER_STATE = 74,
    // ── Vehicles ─────────────────────────────────────────────────────
    // Server-authoritative vehicle motion, seats, and presentation.
    PACKET_VEHICLE_INPUT = 75,
    PACKET_VEHICLE_SEAT_REQUEST = 76,
    PACKET_VEHICLE_SPAWN_EVENT = 77,
    PACKET_VEHICLE_DESPAWN_EVENT = 78,
    PACKET_VEHICLE_STATE = 79,
    PACKET_VEHICLE_SEAT_EVENT = 80,
    // ── Authoritative live map-config snapshot ────────────────────────
    // The host edits the map file; the server applies it and sends this
    // chunked JSON snapshot so remote clients rebuild the same collision and
    // render geometry without writing their own map file.
    PACKET_MAP_CONFIG_SNAPSHOT = 81,
    // Client-owned car pose. The local client is authoritative for its own
    // chassis; the server adopts and rebroadcasts this pose so other clients
    // see exactly what the driver sees.
    PACKET_VEHICLE_POSE = 82,
    // Depleted arena boost pads and their remaining recharge time.
    PACKET_BOOST_PAD_STATE = 83
};

constexpr int MAX_VIP_STYLE_COLORS = 32;

struct VipStyleEventPacket
{
    PacketHeader header;
    uint32_t playerId = 0;
    uint32_t styleEpoch = 0;
    uint8_t styleKind = 0;
    uint8_t animation = 0;
    uint8_t direction = 0;
    uint8_t colorCount = 0;
    float rainbowSpeed = 1.0f;
    struct VipStyleColor
    {
        uint8_t r = 158;
        uint8_t g = 158;
        uint8_t b = 158;
    };
    VipStyleColor colors[MAX_VIP_STYLE_COLORS];
};

static_assert(sizeof(VipStyleEventPacket) <= 200, "VipStyleEventPacket is too large");

struct NpcDamageRequestPacket
{
    PacketHeader header;
    uint32_t npcEntityId = 0;
    int32_t damage = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_ENTITY;
};

struct NpcDamageEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t npcEntityId = 0;
    uint32_t shooterPlayerId = 0;
    int32_t damage = 0;
    int32_t npcHealth = 0;
    uint8_t killed = 0;
    uint8_t attackerEntityType = ENTITY_PLAYER;
    // Reused without growing the packet: the confirmed body part (ActorBodyPart)
    // and whether the projectile punched through the part. Remote clients use
    // these to apply the local per-part damage hole on the NPC.
    uint8_t bodyPart = 0;
    uint8_t punchThrough = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_ENTITY;
    float contactForce = -1.0f;
};

// Server → all clients: one authoritative kill. The reliable envelope encodes
// eventId at the byte immediately after the header, so eventId/eventSessionId
// must stay first (see RELIABLE_EVENT_ID_OFFSET).
struct KillEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t killerId = 0;
    uint32_t victimId = 0;
    uint32_t serverTick = 0;
    uint64_t correlationId = 0;
    uint8_t killerEntityType = ENTITY_PLAYER;
    uint8_t victimEntityType = ENTITY_PLAYER;
    uint8_t reserved[6] = {};
    char killerName[64] = {};
    char victimName[64] = {};
    char weaponDisplay[64] = {};
};

struct ServerCommandPacket
{
    PacketHeader header;
    char commandText[240];
};

struct ServerCommandResultPacket
{
    PacketHeader header;
    int32_t accepted = 0;      // 1 = applied, 0 = rejected (not authorized)
    char statusText[240];      // human-readable result for the terminal
};

#pragma pack(pop)

bool validHeader(const PacketHeader& header, uint8_t expectedType);

} // namespace MimitaNet