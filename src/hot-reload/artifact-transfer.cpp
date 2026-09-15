// 09 15 2026
/* purpose
* Implements the chunked artifact wire-transfer streamer/receiver. See
* artifact-transfer.h for the contract. Mechanism only.
*/
#include "hot-reload/artifact-transfer.h"

#include <cstring>

namespace MimitaRuntime {

bool ArtifactStreamer::begin(std::uint64_t logicalGenerationId,
                             std::uint64_t platformArtifactHash, const void* data,
                             std::size_t size)
{
    // Validate first; only commit state on success so a rejected begin does not
    // clobber a valid stream.
    if (platformArtifactHash == 0 || !data || size == 0 ||
        size > 0xffffffffull)
        return false;
    if (hashArtifactBytes(data, size) != platformArtifactHash)
        return false;
    bytes_.assign(static_cast<const unsigned char*>(data),
                  static_cast<const unsigned char*>(data) + size);
    logicalGenerationId_ = logicalGenerationId;
    hash_ = platformArtifactHash;
    totalSize_ = static_cast<std::uint32_t>(size);
    chunkCount_ = (totalSize_ + ARTIFACT_CHUNK_BYTES - 1) / ARTIFACT_CHUNK_BYTES;
    return true;
}

bool ArtifactStreamer::makeBegin(ArtifactBeginPacket& out) const
{
    if (hash_ == 0)
        return false;
    out = ArtifactBeginPacket{};
    out.header.type = PACKET_ARTIFACT_BEGIN;
    out.logicalGenerationId = logicalGenerationId_;
    out.platformArtifactHash = hash_;
    out.totalSize = totalSize_;
    out.chunkCount = chunkCount_;
    out.chunkSize = (std::uint16_t)ARTIFACT_CHUNK_BYTES;
    return true;
}

bool ArtifactStreamer::makeChunk(std::uint32_t index, ArtifactChunkPacket& out) const
{
    if (hash_ == 0 || index >= chunkCount_)
        return false;
    const std::uint32_t offset = index * ARTIFACT_CHUNK_BYTES;
    const std::uint32_t size = totalSize_ - offset > ARTIFACT_CHUNK_BYTES
        ? ARTIFACT_CHUNK_BYTES
        : totalSize_ - offset;
    out = ArtifactChunkPacket{};
    out.header.type = PACKET_ARTIFACT_CHUNK;
    out.platformArtifactHash = hash_;
    out.chunkIndex = index;
    out.offset = offset;
    out.size = (std::uint16_t)size;
    std::memcpy(out.payload, bytes_.data() + offset, size);
    return true;
}

void ArtifactReceiver::begin(const ArtifactBeginPacket& pkt)
{
    hash_ = pkt.platformArtifactHash;
    logicalGenerationId_ = pkt.logicalGenerationId;
    totalSize_ = pkt.totalSize;
    chunkCount_ = pkt.chunkCount;
    received_ = 0;
    error_.clear();
    bytes_.assign(totalSize_, 0);
    have_.assign(chunkCount_, 0);
    if (hash_ == 0 || chunkCount_ == 0 || totalSize_ == 0 ||
        (std::uint64_t)chunkCount_ * ARTIFACT_CHUNK_BYTES < totalSize_) {
        phase_ = AcquirePhase::Failed;
        error_ = "invalid artifact begin";
        return;
    }
    phase_ = AcquirePhase::Receiving;
}

bool ArtifactReceiver::onChunk(const ArtifactChunkPacket& pkt)
{
    if (phase_ != AcquirePhase::Receiving && phase_ != AcquirePhase::Verifying)
        return false;
    if (pkt.platformArtifactHash != hash_)
        return false;
    if (pkt.chunkIndex >= chunkCount_)
        return false;
    const std::uint32_t expectedOffset = pkt.chunkIndex * ARTIFACT_CHUNK_BYTES;
    const std::uint32_t expectedSize =
        totalSize_ - expectedOffset > ARTIFACT_CHUNK_BYTES
            ? ARTIFACT_CHUNK_BYTES
            : totalSize_ - expectedOffset;
    if (pkt.offset != expectedOffset || pkt.size != expectedSize)
        return false;
    // Duplicate delivery is safe: copy again, count once.
    std::memcpy(bytes_.data() + expectedOffset, pkt.payload, pkt.size);
    if (!have_[pkt.chunkIndex]) {
        have_[pkt.chunkIndex] = 1;
        ++received_;
    }
    if (received_ == chunkCount_)
        phase_ = AcquirePhase::Verifying;
    return true;
}

bool ArtifactReceiver::commit(std::string& error)
{
    if (!complete()) {
        phase_ = AcquirePhase::Failed;
        error_.clear();
        error = "artifact incomplete";
        error_ = error;
        return false;
    }
    acq_.begin(logicalGenerationId_, hash_);
    acq_.appendChunk(bytes_.data(), bytes_.size());
    const bool ok = acq_.commit(error);
    phase_ = acq_.phase();
    error_ = error;
    return ok;
}

void ArtifactReceiver::fail(const std::string& reason)
{
    phase_ = AcquirePhase::Failed;
    error_ = reason;
}

} // namespace MimitaRuntime
