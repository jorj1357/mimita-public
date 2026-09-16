// 09 15 2026
/* purpose
* Generic logical-resource + versioned content-artifact primitive. Content bytes
* are opaque immutable hash-addressed blobs carried by the EXISTING ArtifactCache
* / ArtifactStreamer / ArtifactReceiver; this layer only adds logical identity and
* per-resource version state (active / candidate / last-good) plus a kind-specific
* validator. It is code-kind agnostic: DLL bytes are just another kind. Publication
* is atomic from resolve()'s perspective and a failed candidate leaves the active
* (last-good) mapping intact. Does NOT own transport, decoding beyond structural
* validation, GPU upload, or simulation publication.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

#include "hot-reload/artifact-cache.h"
#include "project/presentation-resource.h"

namespace MimitaRuntime {

// Kind only selects the validator/preparer. Never encodes gameplay semantics.
enum class ResourceKind : std::uint32_t {
    Unknown = 0,
    Code = 1,
    Png = 2,
    Glb = 3,
    Wav = 4,
};

inline const char* resourceKindName(ResourceKind k)
{
    switch (k) {
    case ResourceKind::Code: return "code";
    case ResourceKind::Png: return "png";
    case ResourceKind::Glb: return "glb";
    case ResourceKind::Wav: return "wav";
    default: return "unknown";
    }
}

// Stable logical identity + exact immutable version binding.
struct ContentArtifactV1 {
    std::uint64_t logicalResourceId = 0;  // resourceIdFromLogicalName("ui.menu.logo")
    std::uint32_t resourceKind = 0;       // ResourceKind
    std::uint64_t contentHash = 0;        // exact artifact bytes
    std::uint32_t byteSize = 0;
};

inline std::uint64_t resourceIdFromLogicalName(const char* name)
{
    if (!name || !*name)
        return 0;
    return hashArtifactBytes(reinterpret_cast<const unsigned char*>(name),
                             std::strlen(name));
}

using ResourceValidatorFn = bool (*)(const unsigned char* data, std::size_t size);

// Low-level structural validators (kernel content decoders). They check format
// framing, not feature semantics.
inline bool validateCodeArtifact(const unsigned char* data, std::size_t size)
{
    return data != nullptr && size > 2 && data[0] == 'M' && data[1] == 'Z';
}

inline bool validatePng(const unsigned char* data, std::size_t size)
{
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (!data || size < 8)
        return false;
    if (std::memcmp(data, sig, 8) != 0)
        return false;
    // Require a first IHDR chunk at offset 8.
    return size >= 16 && std::memcmp(data + 12, "IHDR", 4) == 0;
}

inline bool validateGlb(const unsigned char* data, std::size_t size)
{
    if (!data || size < 12)
        return false;
    // Little-endian magic "glTF" + version 2.
    const std::uint32_t magic = (std::uint32_t)data[0] | ((std::uint32_t)data[1] << 8) |
        ((std::uint32_t)data[2] << 16) | ((std::uint32_t)data[3] << 24);
    const std::uint32_t version = (std::uint32_t)data[4] |
        ((std::uint32_t)data[5] << 8) | ((std::uint32_t)data[6] << 16) |
        ((std::uint32_t)data[7] << 24);
    return magic == 0x46546C67u && version == 2;
}

inline bool validateWav(const unsigned char* data, std::size_t size)
{
    if (!data || size < 12)
        return false;
    return std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0;
}

inline ResourceValidatorFn validatorFor(std::uint32_t kind)
{
    switch (static_cast<ResourceKind>(kind)) {
    case ResourceKind::Code: return &validateCodeArtifact;
    case ResourceKind::Png: return &validatePng;
    case ResourceKind::Glb: return &validateGlb;
    case ResourceKind::Wav: return &validateWav;
    default: return nullptr;
    }
}

// Transport/acquisition metadata ONLY. The authoritative active logical mapping,
// last-good, retirement, and prepared runtime handle live in the ONE canonical
// provider: PresentationResourceProvider (already used by the real render path).
// This type must never decide the active mapping independently.
class ResourceRegistry {
public:
    static ResourceRegistry& instance()
    {
        static ResourceRegistry registry;
        return registry;
    }

    void clear()
    {
        descriptors_.clear();
        pendingByHash_.clear();
    }

    // Advertise a candidate version bound to (logicalResourceId, contentHash,
    // kind, byteSize). Replaces any pending candidate for the logical id, so a
    // newer announcement supersedes an older one.
    bool announceCandidate(const ContentArtifactV1& d)
    {
        if (d.logicalResourceId == 0 || d.contentHash == 0)
            return false;
        descriptors_[d.logicalResourceId] = d;
        pendingByHash_[d.contentHash] = d.logicalResourceId;
        return true;
    }

    // Which logical resource is currently awaiting these exact bytes (0 = none)?
    // Lets the generic artifact-receive path route completed content bytes to the
    // canonical provider instead of the code loader.
    std::uint64_t pendingLogicalIdForHash(std::uint64_t contentHash) const
    {
        const auto it = pendingByHash_.find(contentHash);
        return it == pendingByHash_.end() ? 0 : it->second;
    }

    std::uint64_t candidateHashOf(std::uint64_t logicalResourceId) const
    {
        const auto it = descriptors_.find(logicalResourceId);
        return it == descriptors_.end() ? 0 : it->second.contentHash;
    }

    const ContentArtifactV1* descriptorOf(std::uint64_t logicalResourceId) const
    {
        const auto it = descriptors_.find(logicalResourceId);
        return it == descriptors_.end() ? nullptr : &it->second;
    }

    // Resolve is a thin read of the canonical provider so there is one truth.
    std::uint64_t resolve(std::uint64_t logicalResourceId) const
    {
        const ResourceGeneration* g =
            PresentationResourceProvider::instance().current(logicalResourceId);
        return g ? g->contentHash : 0;
    }

    void acknowledgePublished(std::uint64_t logicalResourceId)
    {
        const auto it = descriptors_.find(logicalResourceId);
        if (it != descriptors_.end())
            pendingByHash_.erase(it->second.contentHash);
    }

private:
    ResourceRegistry() = default;
    std::unordered_map<std::uint64_t, ContentArtifactV1> descriptors_;
    std::unordered_map<std::uint64_t, std::uint64_t> pendingByHash_;
};

// Canonical publication bridge: validate immutable bytes, then hand the logical
// mapping to PresentationResourceProvider (single authority for active/handle/
// last-good/retirement). Provider `apply` is a no-op for the same hash and keeps
// the previous generation on loader failure.
inline bool publishContentArtifact(std::uint64_t logicalResourceId,
                                   std::uint64_t contentHash,
                                   const unsigned char* bytes, std::size_t size,
                                   std::string& error)
{
    ResourceRegistry& reg = ResourceRegistry::instance();
    const ContentArtifactV1* d = reg.descriptorOf(logicalResourceId);
    if (!d || d->contentHash != contentHash) {
        error = "not the advertised candidate (stale or unknown)";
        return false;
    }
    if (hashArtifactBytes(bytes, size) != contentHash) {
        error = "content hash mismatch";
        return false;
    }
    const ResourceValidatorFn validate = validatorFor(d->resourceKind);
    if (!validate || !validate(bytes, size)) {
        error = "validation failed";
        return false;
    }
    if (!PresentationResourceProvider::instance().apply(logicalResourceId,
                                                        contentHash, &error))
        return false;
    reg.acknowledgePublished(logicalResourceId);
    return true;
}

inline bool publishContentArtifactFromCache(std::uint64_t logicalResourceId,
                                            std::string& error)
{
    ResourceRegistry& reg = ResourceRegistry::instance();
    const ContentArtifactV1* d = reg.descriptorOf(logicalResourceId);
    if (!d) {
        error = "no candidate";
        return false;
    }
    std::vector<unsigned char> bytes;
    if (!ArtifactCache::instance().read(d->contentHash, bytes)) {
        error = "bytes not cached";
        return false;
    }
    return publishContentArtifact(logicalResourceId, d->contentHash, bytes.data(),
                                  bytes.size(), error);
}

} // namespace MimitaRuntime
