#pragma once

#include "network/packets.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MimitaNet {

constexpr uint16_t MAX_SNAPSHOT_CHUNKS = 256;
constexpr uint32_t SNAPSHOT_CHUNK_MAX_ENTITIES = 7;

size_t snapshotChunkHeaderBytes();
size_t snapshotChunkWireSize(uint16_t entityCount);

CompactEntityData compactEntityFromSnapshot(const SnapshotEntity& entity);
SnapshotEntity snapshotEntityFromCompact(const CompactEntityData& entity);

bool buildSnapshotChunks(const CompactEntityData* entities,
                         uint32_t entityCount,
                         uint32_t serverTick,
                         uint32_t ownerPlayerId,
                         std::vector<std::vector<uint8_t>>& outChunks,
                         std::string* error = nullptr);

bool parseSnapshotChunk(const void* data,
                        size_t bytes,
                        SnapshotChunkPacket& out,
                        std::string* error = nullptr);

bool reassembleSnapshotChunks(const std::vector<SnapshotChunkPacket>& chunks,
                              std::vector<CompactEntityData>& outEntities,
                              std::string* error = nullptr);

void clearSnapshotPacket(SnapshotPacket& snapshot, uint32_t serverTick);
bool appendSnapshotChunkToPacket(const SnapshotChunkPacket& chunk,
                                 SnapshotPacket& snapshot);

bool runSnapshotChunkSelfTest(std::string* report = nullptr);

} // namespace MimitaNet
