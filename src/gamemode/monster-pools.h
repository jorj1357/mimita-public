// 10 19 2026
/* purpose
* Load Zombie Tower monster encounter pools from config/monster-pools.json.
* A pool is an ordered list of monster role IDs with finite counts.
* Does NOT spawn actors or own map placement; the server encounter runtime does.
*/
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct MonsterPoolEntry
{
    std::string role;
    int count = 1;
};

class MonsterPoolRegistry
{
public:
    static MonsterPoolRegistry& instance();

    bool load(const std::string& path = "config/monster-pools.json");
    bool pollReload();
    const std::vector<MonsterPoolEntry>* get(const std::string& id) const;

private:
    MonsterPoolRegistry() = default;

    std::string mPath = "config/monster-pools.json";
    std::unordered_map<std::string, std::vector<MonsterPoolEntry>> mPools;
    std::filesystem::file_time_type mLastWrite{};
};

