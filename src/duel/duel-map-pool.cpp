// 08 10 2026, 14 34
/* purpose
* Loads and hot-reloads config/duel-maps.json, the allowed duel map pool.
* Picks a random map for a player when they queue, and lists the pool so the
* server can auto-rotate between rematches.
* Does NOT own matchmaking, gamemode rules, or gameplay logic.
* Does NOT fail hard on bad JSON - keeps the last valid list and logs an error.
*/

#include "duel/duel-map-pool.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

DuelMapPool& DuelMapPool::instance()
{
    static DuelMapPool pool;
    return pool;
}

void DuelMapPool::load(const std::string& path)
{
    mPath = path;
    std::error_code ec;
    mLastWrite = std::filesystem::last_write_time(path, ec);

    std::ifstream file(path);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::Duel,
            "[DUEL MAPS] Missing %s; using defaults.\n", path.c_str());
        mMaps = {"atdm", "funworld3"};
        return;
    }

    try {
        json root;
        file >> root;
        std::vector<std::string> next;
        if (root.contains("maps") && root["maps"].is_array())
        {
            for (const auto& item : root["maps"])
            {
                if (item.is_string())
                    next.push_back(item.get<std::string>());
            }
        }
        if (next.empty())
        {
            Debug::warn(Debug::Category::Duel,
                "[DUEL MAPS] %s has no valid \"maps\" array; keeping previous list.\n",
                path.c_str());
            return;
        }
        mMaps = std::move(next);
        Debug::warn(Debug::Category::Duel, "[DUEL MAPS] Loaded %zu maps from %s\n",
                    mMaps.size(), path.c_str());
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Duel,
            "[DUEL MAPS] Parse error in %s: %s. Keeping previous list.\n",
            path.c_str(), e.what());
    }
}

void DuelMapPool::pollReload()
{
    if (mPath.empty()) return;
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(mPath, ec);
    if (ec || writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return;
    Debug::warn(Debug::Category::Duel, "[DUEL MAPS] Detected change, reloading %s\n",
                mPath.c_str());
    load(mPath);
}

std::string DuelMapPool::randomMap() const
{
    static const std::string fallback = "funworld3";
    if (mMaps.empty())
        return fallback;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, mMaps.size() - 1);
    return mMaps[dist(rng)];
}

bool DuelMapPool::has(const std::string& mapId) const
{
    for (const auto& m : mMaps)
    {
        if (m == mapId)
            return true;
    }
    return false;
}
