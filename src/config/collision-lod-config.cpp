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
        j = nlohmann::json::parse(file, nullptr, true, true);

        if (j.contains("enabled"))
            mEnabled = j.value("enabled", true);
        if (j.contains("cellSize"))
        {
            const float v = j.value("cellSize", mCellSize);
            mCellSize = std::max(0.1f, std::min(v, 4.0f));
        }
        if (j.contains("static_face_removal"))
            mStaticFaceRemoval = j.value("static_face_removal", true);
        if (j.contains("static_solid_union"))
            mStaticSolidUnion = j.value("static_solid_union", true);
        if (j.contains("static_solid_union_max_triangles"))
        {
            const unsigned long long v =
                j.value("static_solid_union_max_triangles", 200000ull);
            mStaticSolidUnionMaxTriangles = static_cast<uint32_t>(
                std::clamp<unsigned long long>(v, 1000ull, 2000000ull));
        }
        if (j.contains("static_solid_union_max_solids"))
        {
            const unsigned long long v =
                j.value("static_solid_union_max_solids", 256ull);
            mStaticSolidUnionMaxSolids = static_cast<uint32_t>(
                std::clamp<unsigned long long>(v, 1ull, 10000ull));
        }
        ++mRevision;

        std::error_code ec;
        mLastWrite = std::filesystem::last_write_time(path, ec);
        mLastCheck = std::chrono::steady_clock::now();

        printf("[COLLISION LOD] loaded cellSize=%.2f enabled=%d static_face_removal=%d "
               "static_solid_union=%d max_union_tris=%u max_union_solids=%u revision=%u\n",
               mCellSize, (int)mEnabled, (int)mStaticFaceRemoval,
               (int)mStaticSolidUnion, mStaticSolidUnionMaxTriangles,
               mStaticSolidUnionMaxSolids, mRevision);
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
