#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "hot-reload/game-api.h"

class HotReloadSystem {
public:
    static HotReloadSystem& instance();

    bool loadGameDLL();
    void unloadGameDLL();
    bool reloadGameDLLIfChanged();
    std::uint64_t getDLLWriteTime() const;

    const GameAPI* gameAPI() const;
    GameMemory& gameMemory();
    bool loaded() const;

private:
    HotReloadSystem();
    ~HotReloadSystem();
    HotReloadSystem(const HotReloadSystem&) = delete;
    HotReloadSystem& operator=(const HotReloadSystem&) = delete;

    bool loadCandidate(const std::filesystem::path& sourceDLL);
    bool rebuildIfSourcesChanged();
    std::uint64_t newestSourceWriteTime() const;
    std::filesystem::path makeUniqueTempDLLPath();
    void deleteRetiredTempDLLs();

    void* module_ = nullptr;
    GameAPI api_{};
    GameMemory memory_{};
    std::filesystem::path sourceDLL_;
    std::filesystem::path loadedTempDLL_;
    std::vector<std::filesystem::path> retiredTempDLLs_;
    std::uint64_t loadedDLLWriteTime_ = 0;
    std::uint64_t observedSourceWriteTime_ = 0;
    std::uint64_t tempGeneration_ = 0;
    bool rebuildInProgress_ = false;
};

// TODO(hot-reload): add shader source watching without touching the game DLL.
// TODO(hot-reload): add texture and animation asset replacement through renderer-owned handles.
// TODO(hot-reload): add map reload as an explicit editor transaction, never as a code reload side effect.
// TODO(hot-reload): add reloadable UI behavior while keeping GUI state and GPU resources in the EXE.
// TODO(hot-reload): add live editor integration and a non-blocking background DLL compiler.
