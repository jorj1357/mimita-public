// 08 03 2026, 17 20
/* purpose
* Cold snapshot-chunk bridge. The chunk/quantize/reassemble policy lives in the
* shared header `hot-reload/hot-snapshot-codec.h` (one source for the cold EXE
* fallback and the hot provider). These functions adapt the existing STL-facing
* API to the POD `net.snapshot-codecs` capability and prefer the active hot
* codec; when no hot package is loaded (headless tests) they run the shared
* implementation directly.
* Keeps multiplayer entity snapshot payloads under the safe datagram size limit.
* DOES NOT own server entitlement verification or render interpolation policy.
* DOES NOT send full player profile, style JSON, or payment state over the network.
* DOES NOT mutate gameplay entities outside snapshot serialization.
*/
#include "network/snapshot-chunks.h"

#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-snapshot-codec.h"
#include "vip/vip-appearance.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

namespace MimitaNet {
namespace {

void setError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

bool sameCompactFields(const CompactEntityData& a, const CompactEntityData& b)
{
    return std::memcmp(&a, &b, sizeof(CompactEntityData)) == 0;
}

// Resolve the active generation's snapshot codec through the one generic
// doorway. Never cached across a generation swap. Null when no hot provider is
// registered (headless selftests, early startup).
const GameSnapshotCodecV1* hotSnapshotCodec()
{
    void* callable =
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_SNAPSHOT_CODECS);
    if (!callable)
        return nullptr;
    auto lookup = reinterpret_cast<GameSnapshotCodecLookupFn>(callable);
    return lookup ? lookup(nullptr) : nullptr;
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

uint16_t quantizeGodballPos(float v) { return HotSnapshotCodecImpl::quantizePos(v); }
float dequantizeGodballPos(uint16_t raw) { return HotSnapshotCodecImpl::dequantizePos(raw); }
int16_t quantizeGodballVel(float v) { return HotSnapshotCodecImpl::quantizeVel(v); }
float dequantizeGodballVel(int16_t raw) { return HotSnapshotCodecImpl::dequantizeVel(raw); }

size_t snapshotChunkHeaderBytes()
{
    return HotSnapshotCodecImpl::chunkHeaderBytes();
}

size_t snapshotChunkWireSize(uint16_t entityCount)
{
    return HotSnapshotCodecImpl::chunkWireSize(entityCount);
}

CompactEntityData compactEntityFromSnapshot(const SnapshotEntity& entity)
{
    return HotSnapshotCodecImpl::compact(entity);
}

SnapshotEntity snapshotEntityFromCompact(const CompactEntityData& entity)
{
    return HotSnapshotCodecImpl::decompact(entity);
}

bool buildSnapshotChunks(const CompactEntityData* entities,
                         uint32_t entityCount,
                         uint32_t serverTick,
                         uint32_t ownerPlayerId,
                         uint32_t logicalGenerationId,
                         std::vector<std::vector<uint8_t>>& outChunks,
                         std::string* error)
{
    outChunks.clear();

    std::vector<SnapshotChunkPacket> chunks(MAX_SNAPSHOT_CHUNKS);
    GameSnapshotBuildV1 request{};
    request.structSize = sizeof(GameSnapshotBuildV1);
    request.entityCount = entityCount;
    request.serverTick = serverTick;
    request.ownerPlayerId = ownerPlayerId;
    request.logicalGenerationId = logicalGenerationId;
    request.entities = entities;
    request.outChunks = chunks.data();
    request.maxChunks = MAX_SNAPSHOT_CHUNKS;

    bool ok = false;
    const GameSnapshotCodecV1* codec = hotSnapshotCodec();
    if (codec && codec->build) {
        codec->build(nullptr, &request);
        ok = request.result != 0;
    } else {
        char err[64] = {};
        ok = HotSnapshotCodecImpl::build(entities, entityCount, serverTick,
                                         ownerPlayerId, logicalGenerationId,
                                         chunks.data(), MAX_SNAPSHOT_CHUNKS,
                                         &request.outChunkCount, err);
        std::strncpy(request.error, err, sizeof(request.error) - 1);
    }
    if (!ok) {
        setError(error, request.error[0] ? request.error : "build-failed");
        return false;
    }

    outChunks.reserve(request.outChunkCount);
    for (uint32_t i = 0; i < request.outChunkCount; ++i) {
        const size_t bytes = snapshotChunkWireSize(chunks[i].entityCount);
        const auto* begin = reinterpret_cast<const uint8_t*>(&chunks[i]);
        outChunks.emplace_back(begin, begin + bytes);
    }
    return true;
}

bool parseSnapshotChunk(const void* data,
                        size_t bytes,
                        SnapshotChunkPacket& out,
                        std::string* error)
{
    const GameSnapshotCodecV1* codec = hotSnapshotCodec();
    bool ok = false;
    char err[64] = {};
    if (codec && codec->parse) {
        GameSnapshotParseV1 request{};
        request.structSize = sizeof(GameSnapshotParseV1);
        request.data = data;
        request.bytes = static_cast<uint32_t>(bytes);
        request.out = &out;
        codec->parse(nullptr, &request);
        ok = request.result != 0;
        std::strncpy(err, request.error, sizeof(err) - 1);
    } else {
        ok = HotSnapshotCodecImpl::parse(data, bytes, out, err);
    }
    if (!ok)
        setError(error, err[0] ? err : "parse-failed");
    return ok;
}

bool reassembleSnapshotChunks(const std::vector<SnapshotChunkPacket>& chunks,
                              std::vector<CompactEntityData>& outEntities,
                              std::string* error,
                              uint32_t* outLogicalGenerationId)
{
    outEntities.clear();
    if (outLogicalGenerationId)
        *outLogicalGenerationId = 0;

    const uint32_t maxEntities =
        static_cast<uint32_t>(chunks.size()) * SNAPSHOT_CHUNK_MAX_ENTITIES;
    std::vector<CompactEntityData> scratch(maxEntities);

    uint32_t entityCount = 0;
    uint32_t logicalGenerationId = 0;
    bool ok = false;
    const GameSnapshotCodecV1* codec = hotSnapshotCodec();
    if (codec && codec->reassemble) {
        GameSnapshotReassembleV1 request{};
        request.structSize = sizeof(GameSnapshotReassembleV1);
        request.chunks = chunks.data();
        request.chunkCount = static_cast<uint32_t>(chunks.size());
        request.outEntities = scratch.data();
        request.maxEntities = maxEntities;
        codec->reassemble(nullptr, &request);
        ok = request.result != 0;
        entityCount = request.outEntityCount;
        logicalGenerationId = request.outLogicalGenerationId;
        if (!ok)
            setError(error, request.error[0] ? request.error : "reassemble-failed");
    } else {
        char err[64] = {};
        ok = HotSnapshotCodecImpl::reassemble(
            chunks.data(), static_cast<uint32_t>(chunks.size()), scratch.data(),
            maxEntities, &entityCount, &logicalGenerationId, err);
        if (!ok)
            setError(error, err[0] ? err : "reassemble-failed");
    }
    if (!ok)
        return false;

    outEntities.assign(scratch.begin(), scratch.begin() + entityCount);
    if (outLogicalGenerationId)
        *outLogicalGenerationId = logicalGenerationId;
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
        if (!buildSnapshotChunks(source.data(), count, 1000 + count, 7, 0, encoded, &error))
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
    if (buildSnapshotChunks(one.data(), (uint32_t)one.size(), 42, 1, 0, encoded, &error) &&
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
