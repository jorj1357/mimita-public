// 09 23 2026
/* purpose
* Generation-aware logical sound-resource registry. Maps a logical sound name to
* an immutable, hash-tagged generation of file bytes; reloads decode/validate on
* a worker thread and publish at a safe boundary. A voice holds a shared_ptr to
* the generation it started with, so replacing a generation never invalidates an
* active voice and the old generation retires only when no voice references it.
* Does NOT own the miniaudio device, mixer, or the voice table.
*/
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct AudioResourceGeneration {
    std::uint64_t logicalId = 0;
    std::uint64_t contentHash = 0;
    std::uint32_t generation = 0;
    std::vector<std::uint8_t> bytes;
    bool valid = false;
    std::string lastError;
};

class AudioResourceRegistry {
public:
    static AudioResourceRegistry& instance();

    // Current generation for a logical sound name. Lazily creates generation 1
    // from the file on first use. Returns null when the sound cannot be resolved.
    std::shared_ptr<const AudioResourceGeneration> acquire(const std::string& name);

    // Queue an off-thread re-decode of the file for this logical sound.
    bool requestReload(const std::string& name);

    // Drop the generation; future acquires fall back to the legacy cache.
    void invalidate(const std::string& name);

    // Apply completed worker decodes at a safe boundary (game thread).
    void update();

    std::uint32_t generationOf(const std::string& name) const;
    std::uint32_t loadedCount() const;
    std::uint32_t pendingCount() const;
    void shutdown();

private:
    AudioResourceRegistry();
    ~AudioResourceRegistry();
    AudioResourceRegistry(const AudioResourceRegistry&) = delete;
    AudioResourceRegistry& operator=(const AudioResourceRegistry&) = delete;

    struct Impl;
    Impl* m_impl;
};
