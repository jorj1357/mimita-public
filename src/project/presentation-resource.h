// 09 14 2026
/* purpose
* Generic generation-aware presentation resource provider. One mechanism maps a
* logical resource id -> content hash -> immutable resource generation -> opaque
* handle, with atomic swap and last-good preservation on failure. It is
* type-agnostic: shaders, textures, models, fonts, and audio all register the
* same loader/retire callbacks. Hot presentation systems resolve a logical id
* and read the current handle plus generation; they never cache a raw GPU handle
* forever.
* Does NOT own GPU calls, file IO, or rendering policy; loaders do.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace MimitaRuntime {

// Returns true and writes the new opaque handle on success. On failure the
// provider keeps the previous generation active.
using ResourceLoadFn = bool (*)(void* user, void** outHandle);
// Frees a retired generation's handle. Called once the swap is committed.
using ResourceRetireFn = void (*)(void* user, void* handle);

struct ResourceGeneration {
    std::uint64_t logicalId = 0;
    std::uint64_t contentHash = 0;
    std::uint32_t generation = 0;
    void* handle = nullptr;
    bool valid = false;
    std::string lastError;
    std::uint32_t failureCount = 0;
};

class PresentationResourceProvider {
public:
    static PresentationResourceProvider& instance();

    void clear();

    void setLoader(std::uint64_t logicalId, ResourceLoadFn load,
                   ResourceRetireFn retire, void* user);

    // Adopts an already-created handle as generation 1 (initial load).
    void adopt(std::uint64_t logicalId, std::uint64_t contentHash, void* handle);

    // Applies a candidate with a given content hash. Same hash is a no-op.
    // A failed loader preserves the last-good generation and records the error.
    // On success the previous generation is retired and the generation advances.
    bool apply(std::uint64_t logicalId, std::uint64_t contentHash,
               std::string* error = nullptr);

    const ResourceGeneration* current(std::uint64_t logicalId) const;
    void* handleOf(std::uint64_t logicalId) const;
    std::uint32_t generationOf(std::uint64_t logicalId) const;

private:
    PresentationResourceProvider() = default;

    struct Entry {
        ResourceGeneration state;
        ResourceLoadFn load = nullptr;
        ResourceRetireFn retire = nullptr;
        void* user = nullptr;
    };

    std::unordered_map<std::uint64_t, Entry> entries_;
};

} // namespace MimitaRuntime
