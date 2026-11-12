# Packet catalog (2026-11-12)

**Status:** static catalog of packet ids, wire structs, and usage sites.
This is the hot-networking inventory; it is not a schema migration and not runtime evidence.

- Packet ids: 79
- Parsed wire structs: 81
- Protocol constants: 33
- Machine table: `docs\handoffs\cpp-packet-catalog-20261112.csv`

## Hot-networking target

Transport mechanism stays resident: sockets, ICE/UDP, framing, encryption,
channel/reliability machinery, and the opaque payload envelope.
Packet ids, fields, encode/decode, handlers, and error/reconnect behavior move
to `game.dll`. Adding/removing a packet must not relink the EXE.

## Packet table

| Id | Packet | Wire struct | Fields | Used by | Handler files |
|---:|---|---|---:|---|---|
| 1 | `PACKET_HELLO` | `HelloPacket` | 3 | src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/server-packets.cpp |
| 2 | `PACKET_WELCOME` | `WelcomePacket` | 9 | src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 3 | `PACKET_INPUT` | `InputPacket` | 40 | src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/ice/ice-test.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/server-packets.cpp |
| 4 | `PACKET_SNAPSHOT` | `SnapshotPacket` | 5 | src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/ice/ice-test.cpp,src/network/multiplayer-tick.cpp,src/network/snapshot-chunks.cpp | ? |
| 5 | `PACKET_DISCONNECT` | `DisconnectPacket` | 1 | src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-packets.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 6 | `PACKET_PING` | `PingPacket` | 2 | src/network/multiplayer-tick.cpp,src/network/server-browser.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 7 | `PACKET_PLAYER_LIST` | `?` | ? | src/network/badconn/badconn.cpp | ? |
| 8 | `PACKET_PROFILE` | `ProfilePacket` | 2 | ? | ? |
| 9 | `PACKET_ENTITY_SPAWN` | `EntitySpawnPacket` | 2 | ? | ? |
| 10 | `PACKET_ENTITY_DESPAWN` | `EntityDespawnPacket` | 4 | ? | ? |
| 11 | `PACKET_SPAWN_NPC_REQUEST` | `SpawnNpcRequestPacket` | 5 | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 12 | `PACKET_TELEPORT_REQUEST` | `TeleportRequestPacket` | 4 | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 13 | `PACKET_EXPLODE_REQUEST` | `ExplodeRequestPacket` | 1 | src/network/ice/ice-server.cpp,src/network/multiplayer-packets.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 14 | `PACKET_SHOT_REQUEST` | `ShotRequestPacket` | 25 | src/network/multiplayer-shots.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 15 | `PACKET_SHOT_EVENT` | `ShotEventPacket` | 35 | src/network/multiplayer-tick.cpp,src/network/server-attack.cpp,src/network/server-melee-actions.cpp,src/network/server-npcs.cpp,src/network/server-packet-handlers.cpp | src/network/server-packet-handlers.cpp |
| 16 | `PACKET_CHAT_MESSAGE` | `?` | ? | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 17 | `PACKET_NPC_DAMAGE_REQUEST` | `NpcDamageRequestPacket` | 21 | src/combat/melee-client.cpp,src/combat/weapon-spyknife.cpp,src/network/multiplayer-shots.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 18 | `PACKET_NPC_DAMAGE_EVENT` | `NpcDamageEventPacket` | 27 | src/network/multiplayer-tick.cpp,src/network/server-npcs.cpp | ? |
| 19 | `PACKET_SERVER_COMMAND` | `ServerCommandPacket` | 2 | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp,src/terminal/procedural-world-commands.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 20 | `PACKET_JOIN_REQUEST` | `JoinRequestPacket` | 7 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/ice/ice-test.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/server-packets.cpp |
| 21 | `PACKET_JOIN_ACCEPT` | `JoinAcceptPacket` | 9 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/ice/ice-test.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 22 | `PACKET_JOIN_REJECT` | `JoinRejectPacket` | 3 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/ice/ice-test.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 23 | `PACKET_RECONNECT_REQUEST` | `ReconnectRequestPacket` | 2 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-packets.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 24 | `PACKET_RECONNECT_ACCEPT` | `ReconnectAcceptPacket` | 16 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 25 | `PACKET_DISAGREEMENT` | `DisagreementPacket` | 16 | src/network/badconn/badconn.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 26 | `PACKET_CLIENT_MAP_READY` | `ClientMapReadyPacket` | 3 | src/engine/engine-tick-net.cpp,src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 28 | `PACKET_PROJECTILE_SPAWN_EVENT` | `ProjectileSpawnEventPacket` | 23 | src/network/multiplayer-tick.cpp,src/network/server-npcs.cpp,src/network/server-projectiles.cpp | ? |
| 29 | `PACKET_PROJECTILE_STATE_EVENT` | `ProjectileStateEventPacket` | 21 | src/network/multiplayer-tick.cpp,src/network/server-projectiles.cpp | ? |
| 30 | `PACKET_PROJECTILE_EXPLODE_EVENT` | `ProjectileExplodeEventPacket` | 18 | src/network/multiplayer-tick.cpp,src/network/server-projectiles.cpp | ? |
| 31 | `PACKET_PROJECTILE_DESPAWN_EVENT` | `ProjectileDespawnEventPacket` | 8 | src/network/multiplayer-tick.cpp,src/network/server-projectiles.cpp | ? |
| 33 | `PACKET_MELEE_HIT_EVENT` | `MeleeHitEventPacket` | 22 | src/network/multiplayer-tick.cpp,src/network/server-physical-contact-actors.cpp | ? |
| 34 | `PACKET_PELLET_BLAST_REQUEST` | `PelletBlastRequestPacket` | 14 | src/network/multiplayer-shots.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 35 | `PACKET_PELLET_BLAST_EVENT` | `PelletBlastEventPacket` | 24 | src/network/multiplayer-tick.cpp,src/network/server-attack.cpp,src/network/server-npcs.cpp,src/network/server-packet-handlers.cpp | src/network/server-packet-handlers.cpp |
| 36 | `PACKET_GODBALL_STATE` | `GodballStatePacket` | 5 | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 37 | `PACKET_PROJECTILE_FIRE_RESULT` | `ProjectileFireResultPacket` | 8 | src/network/multiplayer-tick.cpp | ? |
| 38 | `PACKET_ATTACK_REQUEST` | `AttackRequestPacket` | 20 | src/network/badconn/badconn.cpp,src/network/multiplayer-packets.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 39 | `PACKET_ATTACK_RESULT` | `AttackResultPacket` | 13 | src/network/multiplayer-tick.cpp,src/network/server-attack.cpp | ? |
| 40 | `PACKET_RELOAD_REQUEST` | `ReloadRequestPacket` | 6 | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/network/weapon-runtime-reconciliation.cpp | src/network/server-packets.cpp |
| 41 | `PACKET_RELOAD_RESULT` | `ReloadResultPacket` | 12 | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 42 | `PACKET_RESPAWN_REQUEST` | `RespawnRequestPacket` | 3 | ? | ? |
| 43 | `PACKET_PLAYER_RESPAWNED` | `PlayerRespawnedPacket` | 18 | src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-tick.cpp,src/network/server-players.cpp | ? |
| 44 | `PACKET_SPAWN_ACK` | `SpawnAckPacket` | 3 | src/engine/engine-tick-net.cpp,src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 45 | `PACKET_SPAWN_ACTIVATED` | `SpawnActivatedPacket` | 4 | src/network/badconn/badconn.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 46 | `PACKET_RELIABLE_EVENT_ACK` | `ReliableEventAckPacket` | 3 | src/network/badconn/badconn.cpp,src/network/client.cpp,src/network/ice/ice-server.cpp,src/network/multiplayer-projectiles.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 47 | `PACKET_DAMAGE_CONFIRMED_EVENT` | `DamageConfirmedEventPacket` | 30 | src/network/multiplayer-tick.cpp,src/network/server-damage.cpp | ? |
| 48 | `PACKET_CHAT_REQUEST` | `ChatRequestPacket` | 4 | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/terminal/player-commands.cpp | src/network/server-packets.cpp |
| 49 | `PACKET_CHAT_MESSAGE_EVENT` | `ChatMessageEventPacket` | 13 | src/network/multiplayer-tick.cpp,src/network/reliable-gameplay-events.cpp,src/network/server-packet-chat.cpp | src/network/server-packet-chat.cpp |
| 50 | `PACKET_CHAT_TYPING_STATE_REQUEST` | `ChatTypingStateRequestPacket` | 3 | src/gui/hud/chat-window.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 51 | `PACKET_CHAT_TYPING_STATE_EVENT` | `ChatTypingStateEventPacket` | 4 | src/network/multiplayer-tick.cpp,src/network/server-packet-chat.cpp | src/network/server-packet-chat.cpp |
| 52 | `PACKET_VIP_STYLE_EVENT` | `VipStyleEventPacket` | 11 | src/network/badconn/badconn.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 53 | `PACKET_PLAYER_CONNECTION_STATE` | `PlayerConnectionStatePacket` | 4 | src/network/badconn/badconn.cpp,src/network/multiplayer-tick.cpp,src/network/server-packet-chat.cpp | src/network/server-packet-chat.cpp |
| 54 | `PACKET_SERVER_COMMAND_RESULT` | `ServerCommandResultPacket` | 3 | src/network/multiplayer-tick.cpp,src/network/server-packet-chat.cpp | src/network/server-packet-chat.cpp |
| 55 | `PACKET_DUEL_STATE` | `DuelStatePacket` | 150 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 56 | `PACKET_DUEL_ENEMY_SPAWN` | `DuelEnemySpawnPacket` | 11 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 57 | `PACKET_DUEL_REMATCH_REQUEST` | `DuelRematchRequestPacket` | 1 | src/duel/duel-queue.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 58 | `PACKET_MAP_CHANGE` | `MapChangePacket` | 9 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 59 | `PACKET_SPYKNIFE_HIT_CLAIM` | `?` | ? | src/network/server-packets.cpp | src/network/server-packets.cpp |
| 60 | `PACKET_GODBALL_HIT_CLAIM` | `GodballHitClaimPacket` | 10 | src/combat/weapon-godball-movement.cpp,src/engine/engine-tick-net.cpp,src/network/multiplayer-packets.cpp,src/network/server-packets.cpp | src/network/multiplayer-packets.cpp,src/network/server-packets.cpp |
| 61 | `PACKET_SERVER_NOTIFICATION` | `ServerNotificationPacket` | 6 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/server-packets.cpp |
| 62 | `PACKET_AVATAR_MANIFEST` | `?` | ? | src/network/avatar-network-client.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 63 | `PACKET_AVATAR_ASSET_REQUEST` | `AvatarAssetRequestPacket` | 4 | src/network/avatar-network-client.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 64 | `PACKET_AVATAR_ASSET_CHUNK` | `AvatarAssetChunkPacket` | 13 | src/network/avatar-network-client.cpp,src/network/multiplayer-tick.cpp,src/network/server-packets.cpp | src/network/server-packets.cpp |
| 65 | `PACKET_PROGRESSION_EVENT` | `ProgressionEventPacket` | 6 | src/network/multiplayer-tick.cpp,src/persistence/persistence-emit.cpp | ? |
| 66 | `PACKET_BOMB_TAG_STATE` | `BombTagStatePacket` | 16 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 67 | `PACKET_BOMB_TAG_PASS_EVENT` | `BombTagPassEventPacket` | 18 | src/network/server-gamemode.cpp | ? |
| 68 | `PACKET_KILL_EVENT` | `KillEventPacket` | 13 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 69 | `PACKET_RAGDOLL_STATE` | `RagdollStatePacket` | 9 | src/network/multiplayer-tick.cpp,src/network/server-packets.cpp,src/network/server.cpp | src/network/server-packets.cpp |
| 70 | `PACKET_PHYSICAL_ENTITY_SPAWN` | `?` | ? | src/network/destruction-replication-selftest.cpp,src/network/multiplayer-tick.cpp,src/network/physical-entity-replication.cpp | ? |
| 71 | `PACKET_PHYSICAL_ENTITY_DESPAWN` | `?` | ? | src/network/destruction-replication-selftest.cpp,src/network/multiplayer-tick.cpp,src/network/physical-entity-replication.cpp | ? |
| 72 | `PACKET_PHYSICAL_ENTITY_STATE` | `PhysicalEntityStatePacket` | 4 | src/network/destruction-replication-selftest.cpp,src/network/multiplayer-tick.cpp,src/network/physical-entity-replication.cpp | ? |
| 73 | `PACKET_ENTITY_CUT_EVENT` | `?` | ? | src/network/destruction-replication-selftest.cpp,src/network/multiplayer-tick.cpp,src/network/physical-entity-replication.cpp | ? |
| 74 | `PACKET_DISASTER_STATE` | `DisasterStatePacket` | 16 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |
| 75 | `PACKET_VEHICLE_INPUT` | `VehicleInputPacket` | 10 | src/network/server-packets.cpp,src/network/server.cpp,src/network/vehicle-networking.cpp | src/network/server-packets.cpp |
| 76 | `PACKET_VEHICLE_SEAT_REQUEST` | `VehicleSeatRequestPacket` | 6 | src/network/server-packets.cpp,src/network/server.cpp,src/network/vehicle-networking.cpp | src/network/server-packets.cpp |
| 77 | `PACKET_VEHICLE_SPAWN_EVENT` | `VehicleSpawnEventPacket` | 10 | src/network/multiplayer-tick.cpp,src/network/vehicle-networking.cpp | ? |
| 78 | `PACKET_VEHICLE_DESPAWN_EVENT` | `VehicleDespawnEventPacket` | 5 | src/network/multiplayer-tick.cpp,src/network/vehicle-networking.cpp | ? |
| 79 | `PACKET_VEHICLE_STATE` | `VehicleStatePacket` | 4 | src/network/multiplayer-tick.cpp,src/network/vehicle-networking.cpp | ? |
| 80 | `PACKET_VEHICLE_SEAT_EVENT` | `VehicleSeatEventPacket` | 8 | src/network/multiplayer-tick.cpp,src/network/vehicle-networking.cpp | ? |
| 81 | `PACKET_MAP_CONFIG_SNAPSHOT` | `MapConfigSnapshotPacket` | 13 | src/network/multiplayer-tick.cpp,src/network/server-gamemode.cpp | ? |

## Encode/decode and handler owners

| File | Role |
|---|---|
| `src/network/multiplayer-packets.cpp` | encode/decode |
| `src/network/packets.cpp` | helpers |
| `src/network/server-packets.cpp` | server send |
| `src/network/server-packet-handlers.cpp` | server recv |
| `src/network/server-packet-chat.cpp` | server chat |

## Protocol constants

| Line | Name | Type | Value |
|---:|---|---|---|
| 17 | `PROTOCOL_MAGIC` | uint32_t | `constexpr uint32_t PROTOCOL_MAGIC = 0x4d494d38; // MIM8` |
| 34 | `PROTOCOL_VERSION` | uint16_t | `constexpr uint16_t PROTOCOL_VERSION = 52;` |
| 65 | `MAX_RECONNECT_TOKEN_BYTES` | int | `constexpr int MAX_RECONNECT_TOKEN_BYTES = 64;` |
| 66 | `MAX_JOIN_TOKEN_BYTES` | int | `constexpr int MAX_JOIN_TOKEN_BYTES = 64;` |
| 67 | `MAX_VIP_JOIN_TICKET_BYTES` | int | `constexpr int MAX_VIP_JOIN_TICKET_BYTES = 64;` |
| 68 | `MAX_RUNTIME_RUN_ID_BYTES` | int | `constexpr int MAX_RUNTIME_RUN_ID_BYTES = 64;` |
| 69 | `MAX_RUNTIME_BUILD_ID_BYTES` | int | `constexpr int MAX_RUNTIME_BUILD_ID_BYTES = 96;` |
| 70 | `MAX_PLAYERS` | int | `constexpr int MAX_PLAYERS = 32;` |
| 71 | `MAX_SNAPSHOT_ENTITIES` | int | `constexpr int MAX_SNAPSHOT_ENTITIES = 90;` |
| 72 | `MAX_NAME_BYTES` | int | `constexpr int MAX_NAME_BYTES = 32;` |
| 77 | `MAX_MAP_ID_BYTES` | int | `constexpr int MAX_MAP_ID_BYTES = 64;` |
| 81 | `MAX_AVATAR_NAME_BYTES` | int | `constexpr int MAX_AVATAR_NAME_BYTES = 24;` |
| 86 | `MAX_GAME_DATAGRAM_BYTES` | int | `constexpr int MAX_GAME_DATAGRAM_BYTES = 1200;` |
| 87 | `MAX_RECEIVE_PACKET_BYTES` | int | `constexpr int MAX_RECEIVE_PACKET_BYTES = 32768; // includes legacy assembled snapshots` |
| 319 | `MAX_NETWORK_PELLETS` | int | `constexpr int MAX_NETWORK_PELLETS = 16;` |
| 352 | `AVATAR_MANIFEST_PAYLOAD_BYTES` | uint16_t | `constexpr uint16_t AVATAR_MANIFEST_PAYLOAD_BYTES = 1024;` |
| 353 | `AVATAR_ASSET_PAYLOAD_BYTES` | uint16_t | `constexpr uint16_t AVATAR_ASSET_PAYLOAD_BYTES = 1024;` |
| 354 | `AVATAR_ASSET_NAME_BYTES` | uint16_t | `constexpr uint16_t AVATAR_ASSET_NAME_BYTES = 96;` |
| 355 | `AVATAR_ASSET_TYPE_IMAGE` | uint8_t | `constexpr uint8_t AVATAR_ASSET_TYPE_IMAGE = 1;` |
| 770 | `MAX_PROJECTILE_DAMAGE_RESULTS` | int | `constexpr int MAX_PROJECTILE_DAMAGE_RESULTS = 8;` |
| 912 | `MELEE_CLASH_FX_SPARK` | uint8_t | `constexpr uint8_t MELEE_CLASH_FX_SPARK = 1 << 0;` |
| 913 | `MELEE_CLASH_FX_RICOCHET` | uint8_t | `constexpr uint8_t MELEE_CLASH_FX_RICOCHET = 1 << 1;` |
| 999 | `MAX_VIP_STYLE_COLORS` | int | `constexpr int MAX_VIP_STYLE_COLORS = 32;` |
| 1396 | `MAP_CONFIG_SNAPSHOT_CHUNK_BYTES` | uint32_t | `constexpr uint32_t MAP_CONFIG_SNAPSHOT_CHUNK_BYTES = 1024;` |
| 1397 | `MAP_CONFIG_SNAPSHOT_MAX_BYTES` | uint32_t | `constexpr uint32_t MAP_CONFIG_SNAPSHOT_MAX_BYTES = 128 * 1024;` |
| 1533 | `MAX_PELLET_BLAST_TARGETS` | int | `constexpr int MAX_PELLET_BLAST_TARGETS = 8;` |
| 1741 | `MAX_CLAIMED_HIT_TARGETS` | int | `constexpr int MAX_CLAIMED_HIT_TARGETS = 8;` |
| 2014 | `MAX_PHYSICAL_ENTITY_PATH_BYTES` | int | `constexpr int MAX_PHYSICAL_ENTITY_PATH_BYTES = 128;` |
| 2015 | `MAX_PHYSICAL_ENTITY_STATE_ENTRIES` | int | `constexpr int MAX_PHYSICAL_ENTITY_STATE_ENTRIES = 16;` |
| 2082 | `MAX_VEHICLE_TEMPLATE_BYTES` | int | `constexpr int MAX_VEHICLE_TEMPLATE_BYTES = 32;` |
| 2083 | `MAX_VEHICLE_SEATS` | int | `constexpr int MAX_VEHICLE_SEATS = 4;` |
| 2084 | `MAX_VEHICLE_WHEELS` | int | `constexpr int MAX_VEHICLE_WHEELS = 8;` |
| 2085 | `MAX_VEHICLE_STATE_ENTRIES` | int | `constexpr int MAX_VEHICLE_STATE_ENTRIES = 4;` |

## Gaps to resolve before cutover

- Packets with no matching wire struct or no usage site need a confirmation read.
- Packet ids must never be reused within a live session; removal leaves a tombstone until all peers upgrade.
- Every packet needs an explicit error/timeout/reconnect path in the catalog.
- The opaque transport envelope and schema-hash agreement must be designed before packet code moves.
