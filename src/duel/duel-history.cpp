// 08 10 2026, 14 34
/* purpose
* Loads and saves the recent-duels history JSON for the current profile.
* Stores up to 20 entries with opponent, score, map, and timestamp.
* Uses the same config/accounts/ location as the rest of the game's local data.
* Does NOT contain gameplay, matchmaking, or queue logic.
* Does NOT fail hard on bad JSON - keeps an empty list and logs an error.
*/

#include "duel/duel-history.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

DuelHistory& DuelHistory::instance()
{
    static DuelHistory history;
    return history;
}

void DuelHistory::load(const std::string& profileId)
{
    mProfileId = profileId.empty() ? "default" : profileId;
    std::error_code ec;
    std::filesystem::create_directories("config/accounts", ec);
    mFilePath = "config/accounts/duel-history-" + mProfileId + ".json";
    mEntries.clear();

    std::ifstream file(mFilePath);
    if (!file.is_open())
        return;

    try {
        json root;
        file >> root;
        if (!root.is_array())
            return;
        for (const auto& item : root)
        {
            if (!item.is_object()) continue;
            DuelHistoryEntry entry;
            entry.opponentName = item.value("opponent_name", std::string());
            entry.opponentProfileId = item.value("opponent_profile_id", std::string());
            entry.won = item.value("won", false);
            entry.myScore = item.value("my_score", 0);
            entry.oppScore = item.value("opp_score", 0);
            entry.map = item.value("map", std::string());
            entry.unixMs = item.value("unix_ms", (uint64_t)0);
            mEntries.push_back(std::move(entry));
        }
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Duel, "[DUEL HISTORY] parse error in %s: %s\n",
                    mFilePath.c_str(), e.what());
        mEntries.clear();
    }

    Debug::log(Debug::Category::Duel, "[DUEL HISTORY] loaded %zu entries for profile=%s\n",
               mEntries.size(), mProfileId.c_str());
}

void DuelHistory::save()
{
    if (mFilePath.empty()) return;

    json arr = json::array();
    for (const auto& e : mEntries)
    {
        json item;
        item["opponent_name"] = e.opponentName;
        item["opponent_profile_id"] = e.opponentProfileId;
        item["won"] = e.won;
        item["my_score"] = e.myScore;
        item["opp_score"] = e.oppScore;
        item["map"] = e.map;
        item["unix_ms"] = e.unixMs;
        arr.push_back(std::move(item));
    }

    std::ofstream file(mFilePath);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::Duel, "[DUEL HISTORY] could not save %s\n", mFilePath.c_str());
        return;
    }
    file << arr.dump(2) << std::endl;
    Debug::log(Debug::Category::Duel, "[DUEL HISTORY] saved %zu entries to %s\n",
               mEntries.size(), mFilePath.c_str());
}

void DuelHistory::add(DuelHistoryEntry entry)
{
    if (entry.opponentName.empty()) return;
    mEntries.insert(mEntries.begin(), std::move(entry));
    if (mEntries.size() > 20)
        mEntries.resize(20);
    save();
}

void DuelHistory::clear()
{
    mEntries.clear();
    save();
}
