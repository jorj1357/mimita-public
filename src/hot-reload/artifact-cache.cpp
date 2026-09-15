// 09 15 2026
/* purpose
* Implements the content-addressed artifact cache + acquisition/verify machine.
* See artifact-cache.h for the contract. Mechanism only.
*/
#include "hot-reload/artifact-cache.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace MimitaRuntime {

std::uint64_t hashArtifactBytes(const void* data, std::size_t size)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        h ^= static_cast<std::uint64_t>(p[i]);
        h *= 1099511628211ull;
    }
    return h;
}

namespace {
std::string hexOf(std::uint64_t v)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)v);
    return std::string(buf);
}
} // namespace

ArtifactCache& ArtifactCache::instance()
{
    static ArtifactCache cache;
    return cache;
}

void ArtifactCache::setRoot(const std::filesystem::path& root)
{
    root_ = root;
}

std::filesystem::path ArtifactCache::pathForHash(std::uint64_t artifactHash) const
{
    return root_ / (hexOf(artifactHash) + ".bin");
}

bool ArtifactCache::contains(std::uint64_t artifactHash) const
{
    std::error_code ec;
    return std::filesystem::exists(pathForHash(artifactHash), ec);
}

bool ArtifactCache::verify(std::uint64_t expectedHash, const void* data,
                           std::size_t size)
{
    return hashArtifactBytes(data, size) == expectedHash;
}

bool ArtifactCache::store(std::uint64_t expectedHash, const void* data,
                          std::size_t size)
{
    if (expectedHash == 0 || !data)
        return false;
    // The caller's bytes must match the expected hash (reject wrong bytes even
    // when the hash is already cached).
    if (!verify(expectedHash, data, size))
        return false;
    // Immutability + dedupe: never overwrite an existing artifact for a hash.
    if (contains(expectedHash))
        return verifyStored(expectedHash);
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    const std::filesystem::path tmp = pathForHash(expectedHash).string() + ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out.write(static_cast<const char*>(data), (std::streamsize)size);
        if (!out)
            return false;
    }
    std::filesystem::rename(tmp, pathForHash(expectedHash), ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

bool ArtifactCache::verifyStored(std::uint64_t artifactHash) const
{
    std::vector<unsigned char> bytes;
    if (!read(artifactHash, bytes))
        return false;
    return hashArtifactBytes(bytes.data(), bytes.size()) == artifactHash;
}

bool ArtifactCache::read(std::uint64_t artifactHash,
                         std::vector<unsigned char>& out) const
{
    std::ifstream in(pathForHash(artifactHash), std::ios::binary);
    if (!in)
        return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

std::size_t ArtifactCache::count() const
{
    std::error_code ec;
    if (!std::filesystem::exists(root_, ec))
        return 0;
    std::size_t n = 0;
    for (const auto& e : std::filesystem::directory_iterator(root_, ec)) {
        if (ec)
            break;
        if (e.path().extension() == ".bin")
            ++n;
    }
    return n;
}

void ArtifactAcquirer::begin(std::uint64_t logicalGenerationId,
                             std::uint64_t expectedHash)
{
    logicalGenerationId_ = logicalGenerationId;
    expectedHash_ = expectedHash;
    bytes_.clear();
    error_.clear();
    phase_ = AcquirePhase::Requesting;
}

void ArtifactAcquirer::beginFromCache(std::uint64_t logicalGenerationId,
                                      std::uint64_t expectedHash, bool cached)
{
    logicalGenerationId_ = logicalGenerationId;
    expectedHash_ = expectedHash;
    bytes_.clear();
    error_.clear();
    phase_ = cached ? AcquirePhase::Verifying : AcquirePhase::Requesting;
}

void ArtifactAcquirer::appendChunk(const void* data, std::size_t size)
{
    if (phase_ != AcquirePhase::Requesting && phase_ != AcquirePhase::Receiving)
        return;
    phase_ = AcquirePhase::Receiving;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    bytes_.insert(bytes_.end(), p, p + size);
}

bool ArtifactAcquirer::commit(std::string& error)
{
    phase_ = AcquirePhase::Verifying;
    if (expectedHash_ == 0) {
        phase_ = AcquirePhase::Failed;
        error = "no expected artifact hash";
        error_ = error;
        return false;
    }
    // Cache hit: verify the stored bytes rather than the (empty) received buffer.
    if (bytes_.empty() && ArtifactCache::instance().contains(expectedHash_)) {
        if (!ArtifactCache::instance().verifyStored(expectedHash_)) {
            phase_ = AcquirePhase::Failed;
            error = "cached artifact failed verification";
            error_ = error;
            return false;
        }
        phase_ = AcquirePhase::Complete;
        return true;
    }
    if (!ArtifactCache::instance().verify(expectedHash_, bytes_.data(), bytes_.size())) {
        phase_ = AcquirePhase::Failed;
        error = "artifact hash mismatch";
        error_ = error;
        return false;
    }
    if (!ArtifactCache::instance().store(expectedHash_, bytes_.data(), bytes_.size())) {
        phase_ = AcquirePhase::Failed;
        error = "artifact store failed";
        error_ = error;
        return false;
    }
    phase_ = AcquirePhase::Complete;
    return true;
}

void ArtifactAcquirer::fail(const std::string& reason)
{
    phase_ = AcquirePhase::Failed;
    error_ = reason;
}

} // namespace MimitaRuntime
