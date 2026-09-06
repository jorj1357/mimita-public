// 09 06 2026, 16 00
/* purpose
* Own authoritative account totals, session gains and immutable save snapshots.
* Serialize background backend operations without blocking gameplay ticks.
* Retain failed saves and newer revisions until explicitly acknowledged.
* Does NOT trust client account claims, simulate combat or render UI.
*/
#pragma once
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>
#include "persistence/persistence-events.h"

struct ProgressionCounters
{
    uint64_t gold = 0, totalXp = 0, playerKills = 0, npcKills = 0, deaths = 0, playtimeTicks = 0;
};
struct ProgressionNotice
{
    uint32_t playerId = 0;
    uint8_t kind = 0; // 0=kill, 1=NPC, 2=death, 3=attempt, 4=confirmed
    std::string name, confirmedAt;
};
class PersistenceQueue
{
public:
    static PersistenceQueue& instance();
    ~PersistenceQueue();
    void beginSession(const std::string& room, const std::string& token, uint64_t hostId);
    void setHostToken(const std::string& token, uint64_t hostId);
    void observePlayer(uint32_t playerId, uint64_t userId, const std::string& name,
                       const std::string& ticket, bool active);
    void tick(); // exactly once per authoritative 60 Hz tick
    void enqueueKill(const PersistenceKillEvent& event);
    void enqueueMatchResult(const PersistenceMatchEvent& event);
    void requestSave(const char* reason);
    void flushBlocking();
    std::vector<ProgressionNotice> takeNotices();
    std::string sessionId() const;
    size_t queueDepth() const;
    uint64_t totalSent() const;
    uint64_t totalRetries() const;
    uint64_t totalDuplicates() const;
    void resetStats();
private:
    PersistenceQueue() = default;
    struct PlayerState
    {
        uint32_t playerId = 0;
        std::string name, ticket;
        ProgressionCounters initial, gains;
        uint64_t revision = 0, savedRevision = 0;
        bool loaded = false, observed = false, active = false, wasActive = false;
    };
    struct Session
    {
        std::string id, room, token, reason;
        uint64_t batchSequence = 0, ticks = 0, hostId = 0;
        std::unordered_map<uint64_t, PlayerState> players;
        std::unordered_map<uint64_t, uint64_t> lastDeath; // One watermark per victim, not per kill.
        nlohmann::json pending; // Retained verbatim until acknowledgment.
    };
    mutable std::mutex mMutex;
    std::vector<std::shared_ptr<Session>> mSessions;
    std::shared_ptr<Session> mCurrent;
    std::future<void> mWorker;
    uint64_t mRetryTicks = 0;
    size_t mNextSession = 0;
    uint64_t mTotalSent = 0, mTotalRetries = 0, mTotalDuplicates = 0;
    std::vector<ProgressionNotice> mNotices;
    void flushBatch();
};
