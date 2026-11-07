// 2026-11-05
/* purpose
* Own the shared per-mode generator JSON files and the reusable chunk library
* JSON files referenced by those modes.
*/
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gamemode/map-config.h"
#include "worldgen/worldgen-types.h"

namespace MimitaWorldGen {

class WorldGenLibrary
{
public:
    static WorldGenLibrary& instance();

    bool ensureDefaults(std::string* error = nullptr);
    bool listModes(std::vector<std::string>& out, std::string* error = nullptr);
    bool loadMode(const std::string& id, WorldGenMode& out,
                  std::string* error = nullptr);
    bool saveMode(const WorldGenMode& mode, std::string* error = nullptr);

    // Watch the active profile and its referenced chunk files. A successful
    // validated change updates `out`/`chunks`; a parse/validation failure keeps
    // the last committed values and reports why. Returns true when new data was
    // applied, with `changed` set to true.
    bool pollReload(const std::string& id, WorldGenMode& out,
                    std::vector<AuthoredChunk>& chunks, bool* changed = nullptr,
                    std::string* error = nullptr);

    bool loadChunksForMode(const WorldGenMode& mode,
                           std::vector<AuthoredChunk>& out,
                           std::string* error = nullptr);
    bool loadChunkFile(const std::string& path, AuthoredChunk& out,
                       std::string* error = nullptr);
    bool saveChunk(const AuthoredChunk& chunk, std::string* error = nullptr);
    bool listChunkFiles(std::vector<std::string>& out,
                        std::string* error = nullptr);

    static std::string modePath(const std::string& id);
    static std::string chunkPath(const std::string& id);

private:
    WorldGenLibrary() = default;

    struct WatchEntry
    {
        std::string id;
        std::uint64_t modeWrite = 0;
        std::vector<std::pair<std::string, std::uint64_t>> chunkWrites;
    };

    WatchEntry* findWatch(const std::string& id);
    std::vector<WatchEntry> mWatches;
};

} // namespace MimitaWorldGen
