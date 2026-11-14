// 2026-11-14
// Avatar manifest and asset wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

constexpr uint16_t AVATAR_MANIFEST_PAYLOAD_BYTES = 1024;
constexpr uint16_t AVATAR_ASSET_PAYLOAD_BYTES = 1024;
constexpr uint16_t AVATAR_ASSET_NAME_BYTES = 96;
constexpr uint8_t AVATAR_ASSET_TYPE_IMAGE = 1;

// Client -> server -> clients. A manifest is deliberately chunked so it stays
// below MAX_GAME_DATAGRAM_BYTES on both UDP and ICE transports.
struct AvatarManifestChunkPacket
{
    PacketHeader header;
    uint32_t ownerPlayerId = 0;
    uint8_t manifestHash[32] = {};
    uint16_t chunkIndex = 0;
    uint16_t chunkCount = 0;
    uint16_t payloadBytes = 0;
    uint16_t reserved = 0;
    uint8_t payload[AVATAR_MANIFEST_PAYLOAD_BYTES] = {};
};

static_assert(sizeof(AvatarManifestChunkPacket) <= MAX_GAME_DATAGRAM_BYTES,
              "AvatarManifestChunkPacket exceeds datagram limit");

// Receiver -> server. The server uses sourcePlayerId to find the owner's
// already-uploaded bytes, then forwards the matching chunks to this receiver.
struct AvatarAssetRequestPacket
{
    PacketHeader header;
    uint32_t sourcePlayerId = 0;
    uint32_t requestId = 0;
    uint8_t assetHash[32] = {};
};

static_assert(sizeof(AvatarAssetRequestPacket) <= MAX_GAME_DATAGRAM_BYTES,
              "AvatarAssetRequestPacket exceeds datagram limit");

// Owner -> server upload and server -> receiver lazy download. PNG/JPEG bytes
// are already compressed by their file format, so they are sent unmodified.
struct AvatarAssetChunkPacket
{
    PacketHeader header;
    uint32_t sourcePlayerId = 0;
    uint32_t targetPlayerId = 0;
    uint32_t requestId = 0;
    uint8_t assetHash[32] = {};
    uint16_t chunkIndex = 0;
    uint16_t chunkCount = 0;
    uint16_t payloadBytes = 0;
    uint32_t totalBytes = 0;
    uint8_t assetType = AVATAR_ASSET_TYPE_IMAGE;
    uint8_t reserved = 0;
    char logicalName[AVATAR_ASSET_NAME_BYTES] = {};
    uint8_t payload[AVATAR_ASSET_PAYLOAD_BYTES] = {};
};

static_assert(sizeof(AvatarAssetChunkPacket) <= MAX_GAME_DATAGRAM_BYTES,
              "AvatarAssetChunkPacket exceeds datagram limit");

#pragma pack(pop)

}  // namespace MimitaNet
