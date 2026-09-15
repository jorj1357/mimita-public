// 09 15 2026
/* purpose
* Generic chunked wire transfer for hot-generation platform artifacts, built on
* the content-addressed ArtifactCache and the ArtifactAcquirer state machine.
* Server side chunks an immutable artifact into bounded packets; client side
* reassembles with an indexed, duplicate-safe, order-tolerant receiver and only
* commits when the full set is present and hashes. Acquire is separate from
* activate. Does NOT own sockets, activation, or the loader.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hot-reload/artifact-cache.h"
#include "network/packets.h"

namespace MimitaRuntime {

using MimitaNet::ArtifactBeginPacket;
using MimitaNet::ArtifactChunkPacket;
using MimitaNet::ARTIFACT_CHUNK_BYTES;
using MimitaNet::PACKET_ARTIFACT_BEGIN;
using MimitaNet::PACKET_ARTIFACT_CHUNK;

// Server-side: chunk an immutable artifact into bounded packets.
class ArtifactStreamer {
public:
    bool begin(std::uint64_t logicalGenerationId, std::uint64_t platformArtifactHash,
               const void* data, std::size_t size);
    std::uint64_t logicalGenerationId() const { return logicalGenerationId_; }
    std::uint64_t hash() const { return hash_; }
    std::uint32_t chunkCount() const { return chunkCount_; }
    std::uint32_t totalSize() const { return totalSize_; }
    bool makeBegin(ArtifactBeginPacket& out) const;
    bool makeChunk(std::uint32_t index, ArtifactChunkPacket& out) const;

private:
    std::vector<unsigned char> bytes_;
    std::uint64_t logicalGenerationId_ = 0;
    std::uint64_t hash_ = 0;
    std::uint32_t chunkCount_ = 0;
    std::uint32_t totalSize_ = 0;
};

// Client-side: indexed, duplicate-safe, order-tolerant reassembly.
class ArtifactReceiver {
public:
    void begin(const ArtifactBeginPacket& pkt);
    // Returns false when a chunk is rejected (wrong hash/size/range).
    bool onChunk(const ArtifactChunkPacket& pkt);
    bool complete() const { return chunkCount_ > 0 && received_ == chunkCount_; }
    std::uint32_t receivedChunks() const { return received_; }
    std::uint32_t chunkCount() const { return chunkCount_; }
    AcquirePhase phase() const { return phase_; }
    bool commit(std::string& error);
    void fail(const std::string& reason);

    std::uint64_t hash() const { return hash_; }
    std::uint64_t logicalGenerationId() const { return logicalGenerationId_; }
    std::uint32_t totalSize() const { return totalSize_; }
    const std::string& error() const { return error_; }

private:
    ArtifactAcquirer acq_;
    std::uint64_t hash_ = 0;
    std::uint64_t logicalGenerationId_ = 0;
    std::uint32_t totalSize_ = 0;
    std::uint32_t chunkCount_ = 0;
    std::vector<unsigned char> bytes_;
    std::vector<unsigned char> have_;
    std::uint32_t received_ = 0;
    AcquirePhase phase_ = AcquirePhase::Idle;
    std::string error_;
};

} // namespace MimitaRuntime
