// 09 15 2026
/* purpose
* Content-addressed, immutable cache + local acquisition/verify state machine for
* hot generation platform artifacts. A candidate artifact is stored keyed by its
* platform artifact hash and verified by re-hashing the bytes; an existing hash
* is never overwritten in place. Acquisition is deliberately separate from
* activation: committing an artifact never activates a generation.
* Mechanism only: does NOT transfer over the network, build, load, or activate.
* Platform-agnostic at this layer (a path is just bytes; the loader is platform).
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace MimitaRuntime {

// Deterministic 64-bit hash of artifact bytes (FNV-1a, matches gameHash style).
std::uint64_t hashArtifactBytes(const void* data, std::size_t size);

enum class AcquirePhase : std::uint32_t {
    Idle = 0,
    CheckingCache = 1,
    Requesting = 2,
    Receiving = 3,
    Verifying = 4,
    Complete = 5,
    Failed = 6,
};

class ArtifactCache {
public:
    static ArtifactCache& instance();

    // Default root is the process temp/working "hot-cache" directory.
    void setRoot(const std::filesystem::path& root);
    const std::filesystem::path& root() const { return root_; }

    bool contains(std::uint64_t artifactHash) const;
    std::filesystem::path pathForHash(std::uint64_t artifactHash) const;

    // Verify bytes against the expected hash without writing.
    static bool verify(std::uint64_t expectedHash, const void* data, std::size_t size);

    // Store bytes immutably under <root>/<hashHex>.bin. Re-hashes the bytes and
    // rejects a mismatch. Idempotent: an existing artifact is never overwritten
    // (deduplicated by content hash).
    bool store(std::uint64_t expectedHash, const void* data, std::size_t size);

    bool read(std::uint64_t artifactHash, std::vector<unsigned char>& out) const;

    // Re-hash a stored artifact and confirm it still matches its key.
    bool verifyStored(std::uint64_t artifactHash) const;

    std::size_t count() const;

private:
    ArtifactCache() = default;
    std::filesystem::path root_ = "hot-cache";
};

// Local acquisition state machine: begin(expected) -> receive chunks -> commit
// (verify + store). Phase transitions are observable so a peer can report
// ACQUIRING/VALIDATING to the host without a boolean "ready" flag.
class ArtifactAcquirer {
public:
    void begin(std::uint64_t logicalGenerationId, std::uint64_t expectedHash);
    // A cache hit short-circuits to Verifying (no transfer needed).
    void beginFromCache(std::uint64_t logicalGenerationId, std::uint64_t expectedHash,
                        bool cached);
    void appendChunk(const void* data, std::size_t size);
    // Verify + store; returns false and sets `error` on hash mismatch.
    bool commit(std::string& error);
    void fail(const std::string& reason);

    AcquirePhase phase() const { return phase_; }
    std::uint64_t expectedHash() const { return expectedHash_; }
    std::uint64_t logicalGenerationId() const { return logicalGenerationId_; }
    std::size_t receivedBytes() const { return bytes_.size(); }
    const std::string& error() const { return error_; }

private:
    AcquirePhase phase_ = AcquirePhase::Idle;
    std::uint64_t logicalGenerationId_ = 0;
    std::uint64_t expectedHash_ = 0;
    std::vector<unsigned char> bytes_;
    std::string error_;
};

} // namespace MimitaRuntime
