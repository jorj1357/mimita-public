#include "gamemode/monster-pools.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

namespace {
using json = nlohmann::json;

std::filesystem::file_time_type writeTimeOf(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::last_write_time(path, ec);
}
} // namespace

MonsterPoolRegistry& MonsterPoolRegistry::instance()
{
    static MonsterPoolRegistry registry;
    return registry;
}

bool MonsterPoolRegistry::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel,
            "[MONSTER POOLS] missing %s; encounter pools are unavailable\n", path.c_str());
        return false;
    }

    try {
        const json root = parseJsonConfig(file);
        std::unordered_map<std::string, std::vector<MonsterPoolEntry>> next;
        if (root.contains("pools") && root["pools"].is_object()) {
            for (auto poolIt = root["pools"].begin(); poolIt != root["pools"].end(); ++poolIt) {
                if (!poolIt.value().is_array()) continue;
                std::vector<MonsterPoolEntry> entries;
                for (const auto& item : poolIt.value()) {
                    if (!item.is_object()) continue;
                    MonsterPoolEntry entry;
                    entry.role = item.value("role", std::string{});
                    entry.count = std::max(1, item.value("count", 1));
                    if (!entry.role.empty())
                        entries.push_back(std::move(entry));
                }
                if (!entries.empty())
                    next[poolIt.key()] = std::move(entries);
            }
        }
        mPools = std::move(next);
        mPath = path;
        mLastWrite = writeTimeOf(path);
        Debug::warn(Debug::Category::Duel,
            "[MONSTER POOLS] loaded %zu pools from %s\n", mPools.size(), path.c_str());
        return true;
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Duel,
            "[MONSTER POOLS] parse error in %s: %s; keeping last valid data\n",
            path.c_str(), e.what());
        return false;
    }
}

bool MonsterPoolRegistry::pollReload()
{
    if (mPath.empty()) return false;
    const auto write = writeTimeOf(mPath);
    if (write == std::filesystem::file_time_type{} || write == mLastWrite)
        return false;
    return load(mPath);
}

const std::vector<MonsterPoolEntry>* MonsterPoolRegistry::get(const std::string& id) const
{
    const auto it = mPools.find(id);
    return it == mPools.end() ? nullptr : &it->second;
}

