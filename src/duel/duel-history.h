// 08 10 2026, 14 34
/* purpose
* Persists the player's most recent duels to a local JSON file per profile.
* Works for guests AND accounts - no server dependency, no account gate.
* Used by the recent-duels panel to show past matches and rematch opponents.
* Does NOT send data to the website or the coordinator.
* Does NOT contain matchmaking, queue, or match state logic.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct DuelHistoryEntry
{
    std::string opponentName;
    std::string opponentProfileId;
    bool won = false;
    int myScore = 0;
    int oppScore = 0;
    std::string map;
    std::string roomCode;
    uint64_t unixMs = 0;
};

class DuelHistory
{
public:
    static DuelHistory& instance();

    void load(const std::string& profileId);
    void save();
    void add(DuelHistoryEntry entry);
    void clear();

    const std::vector<DuelHistoryEntry>& entries() const { return mEntries; }
    const std::string& profileId() const { return mProfileId; }

private:
    DuelHistory() = default;

    std::vector<DuelHistoryEntry> mEntries;
    std::string mProfileId;
    std::string mFilePath;
};
