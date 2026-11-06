// 2026-11-05
/* purpose
* Own the shared per-mode generator JSON files and the reusable chunk library
* JSON files referenced by those modes.
*/
#pragma once

#include <string>
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
};

} // namespace MimitaWorldGen
