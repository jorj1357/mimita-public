// 08 03 2026, 17 20
/* purpose
* Owns compact snapshot chunk encode, decode, reassembly, and self-test coverage.
* Keeps multiplayer entity snapshot payloads under the safe datagram size limit.
* Copies bounded VIP appearance bytes through legacy and chunked snapshot paths.
* DOES NOT own server entitlement verification or render interpolation policy.
* DOES NOT send full player profile, style JSON, or payment state over the network.
* DOES NOT mutate gameplay entities outside snapshot serialization.
*/
#include "network/snapshot-chunks.h"
#include "vip/vip-appearance.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace MimitaNet {
namespace {

void setError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

bool finiteCompactEntity(const CompactEntityData& e)
{
    return std::isfinite(e.px) && std::isfinite(e.py) && std::isfinite(e.pz) &&
           std::isfinite(e.vx) && std::isfinite(e.vy) && std::isfinite(e.vz) &&
           std::isfinite(e.yaw) &&
           std::isfinite(e.aimX) && std::isfinite(e.aimY) && std::isfinite(e.aimZ);
}

bool sameCompactFields(const CompactEntityData& a, const CompactEntityData& b)
{
    return std::memcmp(&a, &b, sizeof(CompactEntityData)) == 0;
}

CompactEntityData makeTestEntity(uint32_t id)
{
    CompactEntityData e{};
    e.networkEntityId = id + 1;
    e.entityType = ENTITY_PLAYER;
    e.active = 1;
    e.stateFlags = (uint16_t)(NET_STATE_WALKING | NET_STATE_ON_GROUND);
    e.transformEpoch = static_cast<uint16_t>(id % 1024);
    e.ownerClientId = 1000 + id;
    e.px = 1.0f + id * 0.25f;
    e.py = 5.0f + id * 0.125f;
    e.pz = 30.0f - id * 0.0625f;
    e.vx = id * 0.01f;
    e.vy = id * 0.02f;
    e.vz = id * -0.03f;
    e.yaw = id * 0.1f;
    e.aimX = 1.0f;
    e.aimY = id * 0.001f;
    e.aimZ = 0.25f;
    e.health = 100 - static_cast<int32_t>(id % 50);
    e.onGround = 1;
    e.equippedSlot = (int16_t)(1 + (id % 6));
    e.weaponState = (uint8_t)(id % 8);
    e.pingMs = (int32_t)(30 + id * 7);
    e.sizeScale = 0.8f + (float)(id % 5) * 0.15f;
    e.dashSerial = (uint16_t)(id * 2);
    e.groundJumpSerial = (uint16_t)(id * 4);
    e.airJumpSerial = (uint16_t)(id * 3);
    e.downDashSerial = (uint16_t)(id * 5);
    e.directionChangeSerial = (uint16_t)(id * 8);
    e.equipSerial = (uint16_t)(id * 6);
    e.freezeSerial = (uint16_t)(id * 7);
    snprintf(e.displayName, sizeof(e.displayName), "Player_%u", id);
    MimitaVip::VipAppearance vip = MimitaVip::tierDefaultAppearance(
        (uint8_t)(id % (MimitaVip::VIP_TIER_ULTRA_VIP + 1)));
    MimitaVip::copyAppearanceToBytes(
        vip, e.vipTier, e.vipStyleKind, e.vipColorR, e.vipColorG, e.vipColorB, e.vipFlags);
    return e;
}

bool expectReject(const std::vector<uint8_t>& bytes, const char* name, std::ostringstream& report)
{
    SnapshotChunkPacket parsed{};
    std::string error;
    if (parseSnapshotChunk(bytes.data(), bytes.size(), parsed, &error))
    {
        report << "[SNAPSHOT CHUNK SELFTEST] expected reject failed case=" << name << "\n";
        return false;
    }
    return true;
}

} // namespace

size_t snapshotChunkHeaderBytes()
{
    return offsetof(SnapshotChunkPacket, entities);
}

size_t snapshotChunkWireSize(uint16_t entityCount)
{
    return snapshotChunkHeaderBytes() + entityCount * sizeof(CompactEntityData);
}

CompactEntityData compactEntityFromSnapshot(const SnapshotEntity& entity)
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
    std::memset(out.displayName, 0, sizeof(out.displayName));
    std::strncpy(out.displayName, entity.displayName, sizeof(out.displayName) - 1);
    return out;
}

SnapshotEntity snapshotEntityFromCompact(const CompactEntityData& entity)
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
    std::memset(out.displayName, 0, sizeof(out.displayName));
    std::strncpy(out.displayName, entity.displayName, sizeof(out.displayName) - 1);
    return out;
}

bool buildSnapshotChunks(const CompactEntityData* entities,
                         uint32_t entityCount,
                         uint32_t serverTick,
                         uint32_t ownerPlayerId,
                         std::vector<std::vector<uint8_t>>& outChunks,
                         std::string* error)
{
    outChunks.clear();
    if (entityCount > 0 && !entities)
    {
        setError(error, "entities-null");
        return false;
    }

    const uint32_t chunkCount = std::max<uint32_t>(
        1, (entityCount + SNAPSHOT_CHUNK_MAX_ENTITIES - 1) / SNAPSHOT_CHUNK_MAX_ENTITIES);
    if (chunkCount > MAX_SNAPSHOT_CHUNKS)
    {
        setError(error, "too-many-chunks");
        return false;
    }

    outChunks.reserve(chunkCount);
    for (uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
        const uint32_t first = chunkIndex * SNAPSHOT_CHUNK_MAX_ENTITIES;
        const uint16_t count = static_cast<uint16_t>(
            std::min<uint32_t>(SNAPSHOT_CHUNK_MAX_ENTITIES, entityCount - first));
        for (uint16_t i = 0; i < count; ++i)
        {
            if (!finiteCompactEntity(entities[first + i]))
            {
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
        packet.chunkIndex = static_cast<uint16_t>(chunkIndex);
        packet.chunkCount = static_cast<uint16_t>(chunkCount);
        packet.entityCount = count;
        packet.payloadBytes = static_cast<uint16_t>(count * sizeof(CompactEntityData));
        if (count > 0)
        {
            std::memcpy(packet.entities, entities + first,
                        count * sizeof(CompactEntityData));
        }

        const size_t bytes = snapshotChunkWireSize(count);
        if (bytes > MAX_GAME_DATAGRAM_BYTES)
        {
            setError(error, "chunk-too-large");
            return false;
        }

        std::vector<uint8_t> encoded(bytes);
        std::memcpy(encoded.data(), &packet, bytes);
        outChunks.push_back(std::move(encoded));
    }

    return true;
}

bool parseSnapshotChunk(const void* data,
                        size_t bytes,
                        SnapshotChunkPacket& out,
                        std::string* error)
{
    out = {};
    if (!data)
    {
        setError(error, "null-data");
        return false;
    }
    if (bytes > MAX_GAME_DATAGRAM_BYTES)
    {
        setError(error, "packet-too-large");
        return false;
    }
    if (bytes < snapshotChunkHeaderBytes())
    {
        setError(error, "truncated-header");
        return false;
    }

    std::memcpy(&out, data, bytes);
    if (out.header.magic != PROTOCOL_MAGIC)
    {
        setError(error, "bad-magic");
        return false;
    }
    if (out.header.version != PROTOCOL_VERSION)
    {
        setError(error, "bad-version");
        return false;
    }
    if (out.header.type != PACKET_SNAPSHOT)
    {
        setError(error, "bad-type");
        return false;
    }
    if (out.chunkCount == 0 || out.chunkCount > MAX_SNAPSHOT_CHUNKS)
    {
        setError(error, "bad-chunk-count");
        return false;
    }
    if (out.chunkIndex >= out.chunkCount)
    {
        setError(error, "bad-chunk-index");
        return false;
    }
    if (out.entityCount > SNAPSHOT_CHUNK_MAX_ENTITIES)
    {
        setError(error, "bad-entity-count");
        return false;
    }
    if (out.payloadBytes != out.entityCount * sizeof(CompactEntityData))
    {
        setError(error, "bad-payload-bytes");
        return false;
    }
    if (bytes != snapshotChunkWireSize(out.entityCount))
    {
        setError(error, "wire-size-mismatch");
        return false;
    }
    for (uint16_t i = 0; i < out.entityCount; ++i)
    {
        if (!finiteCompactEntity(out.entities[i]))
        {
            setError(error, "non-finite-entity");
            return false;
        }
    }
    return true;
}

bool reassembleSnapshotChunks(const std::vector<SnapshotChunkPacket>& chunks,
                              std::vector<CompactEntityData>& outEntities,
                              std::string* error)
{
    outEntities.clear();
    if (chunks.empty())
    {
        setError(error, "no-chunks");
        return false;
    }

    const uint32_t tick = chunks[0].serverTick;
    const uint16_t chunkCount = chunks[0].chunkCount;
    if (chunkCount == 0 || chunkCount > MAX_SNAPSHOT_CHUNKS)
    {
        setError(error, "bad-chunk-count");
        return false;
    }

    std::vector<const SnapshotChunkPacket*> ordered(chunkCount, nullptr);
    for (const SnapshotChunkPacket& chunk : chunks)
    {
        if (chunk.serverTick != tick)
        {
            setError(error, "mixed-ticks");
            return false;
        }
        if (chunk.chunkCount != chunkCount || chunk.chunkIndex >= chunkCount)
        {
            setError(error, "inconsistent-chunk");
            return false;
        }
        if (ordered[chunk.chunkIndex])
        {
            setError(error, "duplicate-chunk");
            return false;
        }
        ordered[chunk.chunkIndex] = &chunk;
    }

    for (uint16_t i = 0; i < chunkCount; ++i)
    {
        if (!ordered[i])
        {
            setError(error, "missing-chunk");
            return false;
        }
    }

    std::unordered_set<uint32_t> seenIds;
    for (const SnapshotChunkPacket* chunk : ordered)
    {
        for (uint16_t i = 0; i < chunk->entityCount; ++i)
        {
            const CompactEntityData& entity = chunk->entities[i];
            if (entity.networkEntityId != 0 && !seenIds.insert(entity.networkEntityId).second)
            {
                setError(error, "duplicate-entity");
                return false;
            }
            outEntities.push_back(entity);
        }
    }
    return true;
}

void clearSnapshotPacket(SnapshotPacket& snapshot, uint32_t serverTick)
{
    snapshot = {};
    snapshot.header.magic = PROTOCOL_MAGIC;
    snapshot.header.version = PROTOCOL_VERSION;
    snapshot.header.type = PACKET_SNAPSHOT;
    snapshot.header.tick = serverTick;
}

bool appendSnapshotChunkToPacket(const SnapshotChunkPacket& chunk,
                                 SnapshotPacket& snapshot)
{
    if (snapshot.header.tick != chunk.serverTick)
        clearSnapshotPacket(snapshot, chunk.serverTick);

    for (uint16_t i = 0; i < chunk.entityCount; ++i)
    {
        if (snapshot.entityCount >= MAX_SNAPSHOT_ENTITIES)
            return false;
        SnapshotEntity entity = snapshotEntityFromCompact(chunk.entities[i]);
        snapshot.entities[snapshot.entityCount++] = entity;
        if (entity.entityType == ENTITY_PLAYER)
            ++snapshot.playerCount;
        else if (entity.entityType == ENTITY_NPC)
            ++snapshot.npcCount;
    }
    return true;
}

bool runSnapshotChunkSelfTest(std::string* report)
{
    std::ostringstream out;
    bool ok = true;
    const uint32_t counts[] = {0, 1, 2, 18, 19, 36, 37, 96, 100, 500};

    for (uint32_t count : counts)
    {
        std::vector<CompactEntityData> source;
        source.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
            source.push_back(makeTestEntity(i));

        std::vector<std::vector<uint8_t>> encoded;
        std::string error;
        if (!buildSnapshotChunks(source.data(), count, 1000 + count, 7, encoded, &error))
        {
            out << "[SNAPSHOT CHUNK SELFTEST] build failed count=" << count
                << " error=" << error << "\n";
            ok = false;
            continue;
        }

        const uint32_t expectedChunks = std::max<uint32_t>(
            1, (count + SNAPSHOT_CHUNK_MAX_ENTITIES - 1) / SNAPSHOT_CHUNK_MAX_ENTITIES);
        if (encoded.size() != expectedChunks)
        {
            out << "[SNAPSHOT CHUNK SELFTEST] bad chunk count count=" << count
                << " actual=" << encoded.size()
                << " expected=" << expectedChunks << "\n";
            ok = false;
        }

        std::vector<SnapshotChunkPacket> parsed;
        parsed.reserve(encoded.size());
        for (size_t i = 0; i < encoded.size(); ++i)
        {
            if (encoded[i].size() > MAX_GAME_DATAGRAM_BYTES)
            {
                out << "[SNAPSHOT CHUNK SELFTEST] oversized chunk count=" << count
                    << " bytes=" << encoded[i].size() << "\n";
                ok = false;
            }

            SnapshotChunkPacket chunk{};
            if (!parseSnapshotChunk(encoded[i].data(), encoded[i].size(), chunk, &error))
            {
                out << "[SNAPSHOT CHUNK SELFTEST] parse failed count=" << count
                    << " chunk=" << i << " error=" << error << "\n";
                ok = false;
                continue;
            }
            if (chunk.chunkIndex != i || chunk.chunkCount != expectedChunks)
            {
                out << "[SNAPSHOT CHUNK SELFTEST] bad chunk numbering count=" << count
                    << " chunk=" << i << "\n";
                ok = false;
            }
            parsed.push_back(chunk);
        }

        std::vector<CompactEntityData> reassembled;
        if (!reassembleSnapshotChunks(parsed, reassembled, &error))
        {
            out << "[SNAPSHOT CHUNK SELFTEST] reassemble failed count=" << count
                << " error=" << error << "\n";
            ok = false;
            continue;
        }
        if (reassembled.size() != source.size())
        {
            out << "[SNAPSHOT CHUNK SELFTEST] entity loss count=" << count
                << " actual=" << reassembled.size() << "\n";
            ok = false;
            continue;
        }
        for (size_t i = 0; i < source.size(); ++i)
        {
            if (!sameCompactFields(source[i], reassembled[i]))
            {
                out << "[SNAPSHOT CHUNK SELFTEST] field mismatch count=" << count
                    << " entity=" << i << "\n";
                ok = false;
                break;
            }
        }
    }

    std::vector<CompactEntityData> one = {makeTestEntity(1)};
    std::vector<std::vector<uint8_t>> encoded;
    std::string error;
    if (buildSnapshotChunks(one.data(), (uint32_t)one.size(), 42, 1, encoded, &error) &&
        !encoded.empty())
    {
        std::vector<uint8_t> bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->header.magic = 0;
        ok = expectReject(bad, "bad-magic", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->header.version = 0;
        ok = expectReject(bad, "bad-version", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->entityCount =
            SNAPSHOT_CHUNK_MAX_ENTITIES + 1;
        ok = expectReject(bad, "bad-entity-count", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->payloadBytes = 1;
        ok = expectReject(bad, "bad-payload", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->chunkIndex = 1;
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->chunkCount = 1;
        ok = expectReject(bad, "bad-chunk-index", out) && ok;

        bad = encoded[0];
        bad.resize(bad.size() - 1);
        ok = expectReject(bad, "truncated-entity", out) && ok;

        bad = encoded[0];
        bad.resize(MAX_GAME_DATAGRAM_BYTES + 1);
        ok = expectReject(bad, "packet-too-large", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->entities[0].px =
            std::numeric_limits<float>::quiet_NaN();
        ok = expectReject(bad, "nan-position", out) && ok;

        bad = encoded[0];
        reinterpret_cast<SnapshotChunkPacket*>(bad.data())->entities[0].vx =
            std::numeric_limits<float>::infinity();
        ok = expectReject(bad, "inf-velocity", out) && ok;
    }
    else
    {
        out << "[SNAPSHOT CHUNK SELFTEST] failed to build corrupt-case base\n";
        ok = false;
    }

    if (report)
        *report = out.str();
    return ok;
}

} // namespace MimitaNet
