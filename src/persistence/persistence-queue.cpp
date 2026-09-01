// 09 01 2026, 00 00
/* purpose
* Implement an async, non-blocking persistence queue for gameplay events.
* Batch-flush kill and match events to mimita.fun on a background thread.
* Never blocks the server simulation tick on network or database latency.
* Does NOT implement reward logic or database schema.
*/

#include "persistence/persistence-queue.h"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "network/net_common.h"
#include "website/api-client.h"
#include "notifications/notifications.h"

using json = nlohmann::json;

PersistenceQueue& PersistenceQueue::instance()
{
    static PersistenceQueue q;
    return q;
}

static json killEventToJson(const PersistenceKillEvent& e)
{
    json j;
    j["type"] = "kill";
    j["eventId"] = e.eventId;
    j["matchId"] = e.matchId;
    j["serverTick"] = e.serverTick;
    j["attackerType"] = e.attackerType;
    j["attackerId"] = e.attackerId;
    j["attackerName"] = e.attackerName;
    j["victimType"] = e.victimType;
    j["victimId"] = e.victimId;
    j["victimName"] = e.victimName;
    j["weaponId"] = e.weaponId;
    j["distanceMeters"] = e.distanceMeters;
    return j;
}

static json matchEventToJson(const PersistenceMatchEvent& e)
{
    json j;
    j["type"] = "match_result";
    j["eventId"] = e.eventId;
    j["matchId"] = e.matchId;
    j["mode"] = e.mode;
    j["victoryType"] = e.victoryType;
    j["redScore"] = e.redScore;
    j["blueScore"] = e.blueScore;
    j["winnerTeam"] = e.winnerTeam;
    j["winnerPlayerId"] = e.winnerPlayerId;

    json parts = json::array();
    for (const auto& p : e.participants)
    {
        json pj;
        pj["userId"] = p.userId;
        pj["username"] = p.username;
        pj["team"] = p.team;
        pj["kills"] = p.kills;
        pj["deaths"] = p.deaths;
        pj["won"] = p.won;
        parts.push_back(std::move(pj));
    }
    j["participants"] = std::move(parts);
    return j;
}

void PersistenceQueue::enqueueKill(const PersistenceKillEvent& event)
{
    std::lock_guard<std::mutex> lock(mMutex);
    QueuedEvent qe;
    qe.body = killEventToJson(event);
    mQueue.push_back(std::move(qe));
    if (mQueue.size() > 512)
        mQueue.erase(mQueue.begin());
}

void PersistenceQueue::enqueueMatchResult(const PersistenceMatchEvent& event)
{
    std::lock_guard<std::mutex> lock(mMutex);
    QueuedEvent qe;
    qe.body = matchEventToJson(event);
    mQueue.push_back(std::move(qe));
    if (mQueue.size() > 512)
        mQueue.erase(mQueue.begin());
}

void PersistenceQueue::update(float dt, const std::string& sessionToken)
{
    mFlushTimer += dt;
    if (mFlushTimer < 60.0f)
        return;
    mFlushTimer = 0.0f;
    flushBatch(sessionToken);
}

void PersistenceQueue::flushBlocking(const std::string& sessionToken)
{
    flushBatch(sessionToken);
}

void PersistenceQueue::flushBatch(const std::string& sessionToken)
{
    std::vector<QueuedEvent> batch;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mQueue.empty())
            return;
        const size_t count = std::min(mQueue.size(), (size_t)50);
        batch.insert(batch.begin(), mQueue.begin(), mQueue.begin() + count);
        mQueue.erase(mQueue.begin(), mQueue.begin() + count);
    }

    auto send = [this, batchCopy = batch, sessionToken]() mutable {
        json body;
        json events = json::array();
        for (auto& qe : batchCopy)
            events.push_back(std::move(qe.body));
        body["events"] = std::move(events);

        const bool ok = submitPersistenceBatch(sessionToken, body.dump());

        std::lock_guard<std::mutex> lock(mMutex);
        if (ok)
        {
            mTotalSent += batchCopy.size();
            mAutoSaveNotificationPending.store(true);
        }
        else
        {
            for (auto& qe : batchCopy)
            {
                qe.retries++;
                if (qe.retries < 3)
                    mQueue.insert(mQueue.begin(), std::move(qe));
                else
                    mTotalDuplicates++;
            }
        }
    };

    std::thread(std::move(send)).detach();
}

size_t PersistenceQueue::queueDepth() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mQueue.size();
}

uint64_t PersistenceQueue::totalSent() const { return mTotalSent; }
uint64_t PersistenceQueue::totalRetries() const { return mTotalRetries; }
uint64_t PersistenceQueue::totalDuplicates() const { return mTotalDuplicates; }

void PersistenceQueue::resetStats()
{
    mTotalSent = 0;
    mTotalRetries = 0;
    mTotalDuplicates = 0;
}

bool PersistenceQueue::consumeAutoSaveNotification()
{
    return mAutoSaveNotificationPending.exchange(false);
}
