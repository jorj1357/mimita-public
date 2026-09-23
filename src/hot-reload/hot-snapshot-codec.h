// 09 23 2026
/* purpose
* Define the generic, hot-replaceable snapshot-chunk codec boundary and the ONE
* codec implementation shared by the cold EXE fallback and the hot provider.
* The EXE owns the chunk buffers, wire framing, and the socket; a hot module
* owns chunk/quantize/reassemble policy through a POD capability.
* The capability interface is plain data with caller-owned buffers: no STL,
* sockets, or engine objects cross the boundary.
* Does NOT own transport, snapshot storage, or render interpolation policy.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_set>

#include "hot-reload/game-api.h"
#include "network/snapshot-chunks.h"

namespace MimitaNet {

// POD capability requests. `outChunks`/`outEntities` are caller-owned arrays;
// the codec must not retain any pointer past the call.
struct GameSnapshotBuildV1 {
    std::uint32_t structSize;
    std::uint32_t entityCount;
    std::uint32_t serverTick;
    std::uint32_t ownerPlayerId;
    std::uint32_t logicalGenerationId;
    const CompactEntityData* entities;
    SnapshotChunkPacket* outChunks;   // capacity maxChunks
    std::uint32_t maxChunks;
    std::uint32_t outChunkCount;      // out
    std::uint32_t result;             // out: 1 = ok
    char error[64];                   // out
};

struct GameSnapshotParseV1 {
    std::uint32_t structSize;
    const void* data;
    std::uint32_t bytes;
    SnapshotChunkPacket* out;
    std::uint32_t result;             // out: 1 = ok
    char error[64];                   // out
};

struct GameSnapshotReassembleV1 {
    std::uint32_t structSize;
    const SnapshotChunkPacket* chunks;
    std::uint32_t chunkCount;
    CompactEntityData* outEntities;   // capacity maxEntities
    std::uint32_t maxEntities;
    std::uint32_t outEntityCount;     // out
    std::uint32_t outLogicalGenerationId;  // out
    std::uint32_t result;             // out: 1 = ok
    char error[64];                   // out
};

using GameSnapshotBuildFn = void (MIMITA_GAME_CALL *)(void* host, GameSnapshotBuildV1* request);
using GameSnapshotParseFn = void (MIMITA_GAME_CALL *)(void* host, GameSnapshotParseV1* request);
using GameSnapshotReassembleFn = void (MIMITA_GAME_CALL *)(void* host,
                                                           GameSnapshotReassembleV1* request);

// One snapshot codec generation. Append-only.
struct GameSnapshotCodecV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameSnapshotBuildFn build;
    GameSnapshotParseFn parse;
    GameSnapshotReassembleFn reassemble;
    const char* name;
};

// The one generic doorway. Resolve the provider by capability id and call it to
// get the active generation's codec. Returns null when none is registered.
using GameSnapshotCodecLookupFn = const GameSnapshotCodecV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_SIG_SNAPSHOT_CODECS =
    gameHash("sig.net.snapshot-codecs.v1");

// ── The single shared implementation ────────────────────────────────
// Header-only so the SAME code serves the cold EXE fallback and the hot game
// DLL; there is no second copy. Editing this header (listed in the manifest
// `headers`) hot-rebuilds the DLL, and the cold dispatcher prefers the hot
// provider, so the change is live.
namespace HotSnapshotCodecImpl {

inline void setError(char (&error)[64], const char* message)
{
    if (!message) {
        error[0] = '\0';
        return;
    }
    std::strncpy(error, message, 63);
    error[63] = '\0';
}

inline std::uint16_t quantizePos(float v)
{
    const float clamped = std::clamp(v, -327.67f, 327.67f);
    return static_cast<std::uint16_t>((clamped + 327.67f) / 0.01f);
}

inline float dequantizePos(std::uint16_t raw)
{
    return static_cast<float>(raw) * 0.01f - 327.67f;
}

inline std::int16_t quantizeVel(float v)
{
    const float clamped = std::clamp(v, -3276.7f, 3276.7f);
    return static_cast<std::int16_t>(clamped / 0.1f);
}

inline float dequantizeVel(std::int16_t raw)
{
    return static_cast<float>(raw) * 0.1f;
}

inline bool finiteCompactEntity(const CompactEntityData& e)
{
    return std::isfinite(e.px) && std::isfinite(e.py) && std::isfinite(e.pz) &&
           std::isfinite(e.vx) && std::isfinite(e.vy) && std::isfinite(e.vz) &&
           std::isfinite(e.yaw) &&
           std::isfinite(e.aimX) && std::isfinite(e.aimY) && std::isfinite(e.aimZ);
}

inline CompactEntityData compact(const SnapshotEntity& entity)
{
    CompactEntityData out{};
    out.networkEntityId = entity.networkEntityId;
    out.entityType = entity.entityType;
    out.active = entity.active;
    out.stateFlags = entity.stateFlags;
    out.transformEpoch = entity.transformEpoch;
    out.ownerClientId = entity.ownerClientId;
    out.px = entity.px;
    out.py = entity.py;
    out.pz = entity.pz;
    out.vx = entity.vx;
    out.vy = entity.vy;
    out.vz = entity.vz;
    out.yaw = entity.yaw;
    out.aimX = entity.aimX;
    out.aimY = entity.aimY;
    out.aimZ = entity.aimZ;
    out.health = entity.health;
    out.onGround = entity.onGround;
    out.equippedSlot = entity.equippedSlot;
    out.weaponState = entity.weaponState;
    out.pingMs = entity.pingMs;
    out.sizeScale = entity.sizeScale;
    out.dashSerial = entity.dashSerial;
    out.groundJumpSerial = entity.groundJumpSerial;
    out.airJumpSerial = entity.airJumpSerial;
    out.downDashSerial = entity.downDashSerial;
    out.directionChangeSerial = entity.directionChangeSerial;
    out.equipSerial = entity.equipSerial;
    out.freezeSerial = entity.freezeSerial;
    out.spawnGeneration = entity.spawnGeneration;
    out.vipTier = entity.vipTier;
    out.vipStyleKind = entity.vipStyleKind;
    out.vipColorR = entity.vipColorR;
    out.vipColorG = entity.vipColorG;
    out.vipColorB = entity.vipColorB;
    out.vipFlags = entity.vipFlags;
    out.vipStyleEpoch = entity.vipStyleEpoch;
    out.godballPosX = quantizePos(entity.godballX);
    out.godballPosY = quantizePos(entity.godballY);
    out.godballPosZ = quantizePos(entity.godballZ);
    out.godballVelX = quantizeVel(entity.godballVx);
    out.godballVelY = quantizeVel(entity.godballVy);
    out.godballVelZ = quantizeVel(entity.godballVz);
    std::memset(out.displayName, 0, sizeof(out.displayName));
    std::strncpy(out.displayName, entity.displayName, sizeof(out.displayName) - 1);
    std::memset(out.avatarName, 0, sizeof(out.avatarName));
    std::strncpy(out.avatarName, entity.avatarName, sizeof(out.avatarName) - 1);
    return out;
}

inline SnapshotEntity decompact(const CompactEntityData& entity)
{
    SnapshotEntity out{};
    out.networkEntityId = entity.networkEntityId;
    out.entityType = entity.entityType;
    out.active = entity.active;
    out.stateFlags = entity.stateFlags;
    out.transformEpoch = entity.transformEpoch;
    out.ownerClientId = entity.ownerClientId;
    out.px = entity.px;
    out.py = entity.py;
    out.pz = entity.pz;
    out.vx = entity.vx;
    out.vy = entity.vy;
    out.vz = entity.vz;
    out.yaw = entity.yaw;
    out.aimX = entity.aimX;
    out.aimY = entity.aimY;
    out.aimZ = entity.aimZ;
    out.health = entity.health;
    out.onGround = entity.onGround;
    out.equippedSlot = entity.equippedSlot;
    out.weaponState = entity.weaponState;
    out.pingMs = entity.pingMs;
    out.sizeScale = entity.sizeScale;
    out.dashSerial = entity.dashSerial;
    out.groundJumpSerial = entity.groundJumpSerial;
    out.airJumpSerial = entity.airJumpSerial;
    out.downDashSerial = entity.downDashSerial;
    out.directionChangeSerial = entity.directionChangeSerial;
    out.equipSerial = entity.equipSerial;
    out.freezeSerial = entity.freezeSerial;
    out.spawnGeneration = entity.spawnGeneration;
    out.vipTier = entity.vipTier;
    out.vipStyleKind = entity.vipStyleKind;
    out.vipColorR = entity.vipColorR;
    out.vipColorG = entity.vipColorG;
    out.vipColorB = entity.vipColorB;
    out.vipFlags = entity.vipFlags;
    out.vipStyleEpoch = entity.vipStyleEpoch;
    out.godballX = dequantizePos(entity.godballPosX);
    out.godballY = dequantizePos(entity.godballPosY);
    out.godballZ = dequantizePos(entity.godballPosZ);
    out.godballVx = dequantizeVel(entity.godballVelX);
    out.godballVy = dequantizeVel(entity.godballVelY);
    out.godballVz = dequantizeVel(entity.godballVelZ);
    std::memset(out.displayName, 0, sizeof(out.displayName));
    std::strncpy(out.displayName, entity.displayName, sizeof(out.displayName) - 1);
    std::memset(out.avatarName, 0, sizeof(out.avatarName));
    std::strncpy(out.avatarName, entity.avatarName, sizeof(out.avatarName) - 1);
    return out;
}

inline std::size_t chunkHeaderBytes()
{
    return offsetof(SnapshotChunkPacket, entities);
}

inline std::size_t chunkWireSize(std::uint16_t entityCount)
{
    return chunkHeaderBytes() + entityCount * sizeof(CompactEntityData);
}

inline bool build(const CompactEntityData* entities, std::uint32_t entityCount,
                  std::uint32_t serverTick, std::uint32_t ownerPlayerId,
                  std::uint32_t logicalGenerationId, SnapshotChunkPacket* outChunks,
                  std::uint32_t maxChunks, std::uint32_t* outChunkCount,
                  char (&error)[64])
{
    setError(error, nullptr);
    if (outChunkCount)
        *outChunkCount = 0;
    if (entityCount > 0 && !entities) {
        setError(error, "entities-null");
        return false;
    }
    if (!outChunks) {
        setError(error, "no-output");
        return false;
    }

    const std::uint32_t chunkCount = std::max<std::uint32_t>(
        1, (entityCount + SNAPSHOT_CHUNK_MAX_ENTITIES - 1) / SNAPSHOT_CHUNK_MAX_ENTITIES);
    if (chunkCount > MAX_SNAPSHOT_CHUNKS) {
        setError(error, "too-many-chunks");
        return false;
    }
    if (chunkCount > maxChunks) {
        setError(error, "output-capacity");
        return false;
    }

    for (std::uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
        const std::uint32_t first = chunkIndex * SNAPSHOT_CHUNK_MAX_ENTITIES;
        const std::uint16_t count = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(SNAPSHOT_CHUNK_MAX_ENTITIES, entityCount - first));
        for (std::uint16_t i = 0; i < count; ++i) {
            if (!finiteCompactEntity(entities[first + i])) {
                setError(error, "non-finite-entity");
                return false;
            }
        }

        SnapshotChunkPacket packet{};
        packet.header.magic = PROTOCOL_MAGIC;
        packet.header.version = PROTOCOL_VERSION;
        packet.header.type = PACKET_SNAPSHOT;
        packet.header.tick = serverTick;
        packet.header.playerId = ownerPlayerId;
        packet.serverTick = serverTick;
        packet.chunkIndex = static_cast<std::uint16_t>(chunkIndex);
        packet.chunkCount = static_cast<std::uint16_t>(chunkCount);
        packet.entityCount = count;
        packet.payloadBytes = static_cast<std::uint16_t>(count * sizeof(CompactEntityData));
        packet.logicalGenerationId = logicalGenerationId;
        if (count > 0) {
            std::memcpy(packet.entities, entities + first,
                        count * sizeof(CompactEntityData));
        }

        const std::size_t bytes = chunkWireSize(count);
        if (bytes > MAX_GAME_DATAGRAM_BYTES) {
            setError(error, "chunk-too-large");
            return false;
        }

        outChunks[chunkIndex] = packet;
    }

    if (outChunkCount)
        *outChunkCount = chunkCount;
    return true;
}

inline bool parse(const void* data, std::size_t bytes, SnapshotChunkPacket& out,
                  char (&error)[64])
{
    setError(error, nullptr);
    out = {};
    if (!data) {
        setError(error, "null-data");
        return false;
    }
    if (bytes > MAX_GAME_DATAGRAM_BYTES) {
        setError(error, "packet-too-large");
        return false;
    }
    if (bytes < chunkHeaderBytes()) {
        setError(error, "truncated-header");
        return false;
    }

    // Copy at most one chunk struct; an oversized datagram is rejected below by
    // the wire-size check without reading past `out`.
    const std::size_t copyBytes = std::min(bytes, sizeof(SnapshotChunkPacket));
    std::memcpy(&out, data, copyBytes);
    if (out.header.magic != PROTOCOL_MAGIC) {
        setError(error, "bad-magic");
        return false;
    }
    if (out.header.version != PROTOCOL_VERSION) {
        setError(error, "bad-version");
        return false;
    }
    if (out.header.type != PACKET_SNAPSHOT) {
        setError(error, "bad-type");
        return false;
    }
    if (out.chunkCount == 0 || out.chunkCount > MAX_SNAPSHOT_CHUNKS) {
        setError(error, "bad-chunk-count");
        return false;
    }
    if (out.chunkIndex >= out.chunkCount) {
        setError(error, "bad-chunk-index");
        return false;
    }
    if (out.entityCount > SNAPSHOT_CHUNK_MAX_ENTITIES) {
        setError(error, "bad-entity-count");
        return false;
    }
    if (out.payloadBytes != out.entityCount * sizeof(CompactEntityData)) {
        setError(error, "bad-payload-bytes");
        return false;
    }
    if (bytes != chunkWireSize(out.entityCount)) {
        setError(error, "wire-size-mismatch");
        return false;
    }
    for (std::uint16_t i = 0; i < out.entityCount; ++i) {
        if (!finiteCompactEntity(out.entities[i])) {
            setError(error, "non-finite-entity");
            return false;
        }
    }
    return true;
}

inline bool reassemble(const SnapshotChunkPacket* chunks, std::uint32_t chunkCount,
                       CompactEntityData* outEntities, std::uint32_t maxEntities,
                       std::uint32_t* outEntityCount,
                       std::uint32_t* outLogicalGenerationId, char (&error)[64])
{
    setError(error, nullptr);
    if (outEntityCount)
        *outEntityCount = 0;
    if (outLogicalGenerationId)
        *outLogicalGenerationId = 0;
    if (!chunks || chunkCount == 0) {
        setError(error, "no-chunks");
        return false;
    }

    const std::uint32_t tick = chunks[0].serverTick;
    const std::uint16_t declaredCount = chunks[0].chunkCount;
    if (declaredCount == 0 || declaredCount > MAX_SNAPSHOT_CHUNKS) {
        setError(error, "bad-chunk-count");
        return false;
    }

    const SnapshotChunkPacket* ordered[MAX_SNAPSHOT_CHUNKS] = {};
    for (std::uint32_t i = 0; i < chunkCount; ++i) {
        const SnapshotChunkPacket& chunk = chunks[i];
        if (chunk.serverTick != tick) {
            setError(error, "mixed-ticks");
            return false;
        }
        if (chunk.chunkCount != declaredCount || chunk.chunkIndex >= declaredCount) {
            setError(error, "inconsistent-chunk");
            return false;
        }
        if (chunk.logicalGenerationId != chunks[0].logicalGenerationId) {
            setError(error, "mixed-generations");
            return false;
        }
        if (ordered[chunk.chunkIndex]) {
            setError(error, "duplicate-chunk");
            return false;
        }
        ordered[chunk.chunkIndex] = &chunk;
    }

    for (std::uint16_t i = 0; i < declaredCount; ++i) {
        if (!ordered[i]) {
            setError(error, "missing-chunk");
            return false;
        }
    }

    std::unordered_set<std::uint32_t> seenIds;
    std::uint32_t written = 0;
    for (std::uint16_t c = 0; c < declaredCount; ++c) {
        const SnapshotChunkPacket* chunk = ordered[c];
        for (std::uint16_t i = 0; i < chunk->entityCount; ++i) {
            const CompactEntityData& entity = chunk->entities[i];
            if (entity.networkEntityId != 0 &&
                !seenIds.insert(entity.networkEntityId).second) {
                setError(error, "duplicate-entity");
                return false;
            }
            if (written >= maxEntities || !outEntities) {
                setError(error, "output-capacity");
                return false;
            }
            outEntities[written++] = entity;
        }
    }
    if (outEntityCount)
        *outEntityCount = written;
    if (outLogicalGenerationId)
        *outLogicalGenerationId = chunks[0].logicalGenerationId;
    return true;
}

} // namespace HotSnapshotCodecImpl

} // namespace MimitaNet
