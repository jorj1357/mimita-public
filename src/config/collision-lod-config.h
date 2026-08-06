// 08 06 2026, 12 30
/* purpose
* Live-tunable collision LOD settings (vertex-cluster decimation cell size).
* The game renders at full fidelity while the invisible collision proxy is
* decimated so dense objects cost ~0.1ms to collide with.
* Reserved fields for future per-mesh collision overrides and render LOD.
* Does NOT build collision meshes or own the world; a caller re-decimates when
* pollHotReload() reports a change.
*/

#pragma once

#include <chrono>
#include <filesystem>
#include <string>

class CollisionLodConfig {
public:
    static CollisionLodConfig& instance();

    bool load(const std::string& path = "config/collision-lod.json");
    // Returns true when the file changed and settings were re-loaded.
    bool pollHotReload();

    bool enabled() const { return mEnabled; }
    float cellSize() const { return mCellSize; }
    unsigned revision() const { return mRevision; }

private:
    CollisionLodConfig();

    bool mEnabled = true;
    float mCellSize = 0.5f;
    unsigned mRevision = 0;

    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
    std::chrono::steady_clock::time_point mLastCheck{};
};
