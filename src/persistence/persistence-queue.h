// 09 01 2026, 00 00
/* purpose
* Provide an async, non-blocking persistence queue for gameplay events.
* Enqueue kill and match events, then flush in batches on a background thread.
* Never blocks the main simulation tick on database or network latency.
* Does NOT implement reward logic, level calculation, or database schema.
*/

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "persistence/persistence-events.h"

class PersistenceQueue
{
public:
    static PersistenceQueue& instance();

    void enqueueKill(const PersistenceKillEvent& event);
    void enqueueMatchResult(const PersistenceMatchEvent& event);

    void update(float dt, const std::string& sessionToken);
    void flushBlocking(const std::string& sessionToken);

    size_t queueDepth() const;
    uint64_t totalSent() const;
    uint64_t totalRetries() const;
    uint64_t totalDuplicates() const;

    void resetStats();

private:
    PersistenceQueue() = default;

    struct QueuedEvent
    {
        nlohmann::json body;
        int retries = 0;
        uint64_t lastAttemptMs = 0;
    };

    mutable std::mutex mMutex;
    std::vector<QueuedEvent> mQueue;
    float mFlushTimer = 0.0f;
    uint64_t mTotalSent = 0;
    uint64_t mTotalRetries = 0;
    uint64_t mTotalDuplicates = 0;

    void flushBatch(const std::string& sessionToken);
};
