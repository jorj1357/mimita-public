// 08 06 2026, 12 30
/* purpose
* Live-tunable collision LOD settings (vertex-cluster decimation cell size).
* Reloads config/collision-lod.json when the file changes so the collision proxy
* can be re-decimated at runtime without restarting.
* Does NOT build collision meshes or own the world.
*/

#include "config/collision-lod-config.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

CollisionLodConfig& CollisionLodConfig::instance()
{
    static CollisionLodConfig cfg;
    return cfg;
}

CollisionLodConfig::CollisionLodConfig()
{
    load();
}

bool CollisionLodConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("[COLLISION LOD] No config at %s (using defaults: cellSize=%.2f)\n",
               path.c_str(), mCellSize);
        return false;
    }

    try
    {
        json j;
        file >> j;

        if (j.contains("enabled"))
            mEnabled = j.value("enabled", true);
        if (j.contains("cellSize"))
        {
            const float v = j.value("cellSize", mCellSize);
            mCellSize = std::max(0.1f, std::min(v, 4.0f));
        }
        ++mRevision;

        std::error_code ec;
        mLastWrite = std::filesystem::last_write_time(path, ec);
        mLastCheck = std::chrono::steady_clock::now();

        printf("[COLLISION LOD] loaded cellSize=%.2f enabled=%d revision=%u\n",
               mCellSize, (int)mEnabled, mRevision);
        return true;
    }
    catch (const std::exception& e)
    {
        printf("[COLLISION LOD ERROR] Failed to parse %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool CollisionLodConfig::pollHotReload()
{
    // Only check at most every 250ms.
    const auto now = std::chrono::steady_clock::now();
    if (mLastCheck.time_since_epoch().count() != 0 &&
        now - mLastCheck < std::chrono::milliseconds(250))
        return false;
    mLastCheck = now;

    std::error_code ec;
    if (!std::filesystem::exists(mPath, ec) || ec)
        return false;
    const auto writeTime = std::filesystem::last_write_time(mPath, ec);
    if (ec || writeTime == mLastWrite)
        return false;

    mLastWrite = writeTime;
    return load(mPath);
}
