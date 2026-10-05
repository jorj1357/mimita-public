// Generic JSON-defined gamemode runtime - deterministic selection helpers.
//
// Purpose: one stable, dependency-free hash/mix used by every data-driven
// gamemode decision that must agree between the server and every client
// (disaster selection, per-actor random weapon assignment, timeout winner).
//
// It is NOT a general-purpose RNG and does NOT replace the simulation RNG.
// It exposes only pure functions so a test can prove byte-identical output
// for the same (seed, salt) on any platform.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MimitaGamemode {

// FNV-1a over a logical string id (capability id, weapon id, mode id ...).
inline uint32_t hashString(const std::string& text)
{
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= (uint32_t)c;
        hash *= 16777619u;
    }
    return hash;
}

// Stable 32-bit mix of a match seed and a 64-bit salt. Uses 64-bit integer
// math only, so the result is identical on every supported platform.
inline uint32_t mixSeed(uint32_t seed, uint64_t salt)
{
    uint64_t h = 1469598103934665603ull ^ (uint64_t)seed;
    h ^= salt + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h *= 1099511628211ull;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;
    return (uint32_t)h;
}

// Deterministic index in [0, count). Returns 0 when count is empty/zero.
inline uint32_t pickIndex(uint32_t seed, uint64_t salt, uint32_t count)
{
    if (count == 0)
        return 0;
    return mixSeed(seed, salt) % count;
}

// Deterministic selection from a list of stable string ids. Each candidate is
// scored from (seed, salt, its own id) and the highest score wins, with the id
// as a stable tie-breaker. The result therefore depends only on the set of ids
// and never on their order in the source list.
inline const std::string* pickStable(const std::vector<std::string>& candidates,
                                     uint32_t seed, uint64_t salt)
{
    const std::string* best = nullptr;
    uint32_t bestScore = 0;
    for (const std::string& id : candidates) {
        const uint32_t score =
            mixSeed(seed, salt ^ ((uint64_t)hashString(id) * 1099511628211ull));
        if (!best || score > bestScore || (score == bestScore && id < *best)) {
            best = &id;
            bestScore = score;
        }
    }
    return best;
}

} // namespace MimitaGamemode
