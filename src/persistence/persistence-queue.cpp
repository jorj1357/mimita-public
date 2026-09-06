// 09 06 2026, 16 00
/* purpose
* Maintain loaded account totals plus cumulative gains for each host session.
* Keep immutable retry bodies and revision-specific backend acknowledgments.
* Join the owned worker during clean shutdown, retaining unconfirmed state.
* Does NOT overwrite global backend totals or perform HTTP on simulation ticks.
*/
#include "persistence/persistence-queue.h"
#include "persistence/persistence-rewards.h"
#include "website/api-client.h"
#include "debug/debug-log.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <limits>
using json = nlohmann::json;
namespace {
uint64_t integer(const json& j, const char* key)
{
    const auto& v = j.at(key);
    if (v.is_number_unsigned() && v.get<uint64_t>() <= uint64_t(INT64_MAX)) return v.get<uint64_t>();
    if (v.is_number_integer() && v.get<int64_t>() >= 0) return v.get<uint64_t>();
    if (!v.is_string()) throw std::runtime_error("invalid_integer");
    const std::string s = v.get<std::string>();
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid_integer");
    const auto result = std::stoull(s);
    if (result > uint64_t(INT64_MAX)) throw std::runtime_error("integer_overflow");
    return result;
}
ProgressionCounters counters(const json& j)
{
    return {integer(j,"gold"), integer(j,"totalXp"), integer(j,"playerKills"),
            integer(j,"npcKills"), integer(j,"deaths"), integer(j,"playtimeTicks")};
}
json snapshot(uint64_t id, uint64_t revision, const ProgressionCounters& c)
{
    return {{"userId",std::to_string(id)}, {"revision",std::to_string(revision)},
        {"gold",std::to_string(c.gold)}, {"totalXp",std::to_string(c.totalXp)},
        {"playerKills",std::to_string(c.playerKills)}, {"npcKills",std::to_string(c.npcKills)},
        {"deaths",std::to_string(c.deaths)}, {"playtimeTicks",std::to_string(c.playtimeTicks)}};
}
}
PersistenceQueue& PersistenceQueue::instance() { static PersistenceQueue q; return q; }
PersistenceQueue::~PersistenceQueue() { if (mWorker.valid()) mWorker.wait(); }
void PersistenceQueue::beginSession(const std::string& room, const std::string& token, uint64_t hostId)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto session = std::make_shared<Session>();
    std::random_device random;
    std::ostringstream id;
    for (int i = 0; i < 4; ++i) id << std::hex << std::setw(8) << std::setfill('0') << random();
    session->id = id.str(); session->room = room; session->token = token;
    session->hostId = hostId;
    mSessions.push_back(session); // Older unconfirmed sessions survive rehosting.
    mCurrent = session; mRetryTicks = 0;
}
void PersistenceQueue::setHostToken(const std::string& token, uint64_t hostId)
{
    std::lock_guard<std::mutex> lock(mMutex);
    // Refresh only sessions belonging to this login; logout never changes ownership.
    if (hostId && !token.empty())
        for (const auto& s : mSessions) if (s->hostId == hostId) s->token = token;
}
std::string PersistenceQueue::sessionId() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrent ? mCurrent->id : std::string();
}
void PersistenceQueue::observePlayer(uint32_t playerId, uint64_t userId,
    const std::string& name, const std::string& ticket, bool active)
{
    if (!userId) return;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mCurrent) return;
    auto& p = mCurrent->players[userId];
    p.playerId = playerId; p.name = name;
    if (!ticket.empty()) p.ticket = ticket;
    p.observed = true; p.active = p.active || active;
}
void PersistenceQueue::tick()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mCurrent) return;
        auto& s = *mCurrent;
        for (auto& item : s.players)
        {
            auto& p = item.second;
            if (p.observed && p.active) { ++p.gains.playtimeTicks; ++p.revision; }
            if (p.wasActive && (!p.observed || !p.active)) s.reason = "PLAYER_DISCONNECT";
            p.wasActive = p.observed && p.active;
            p.observed = p.active = false;
        }
        if (++s.ticks % 3600 == 0) s.reason = "AUTOSAVE";
        if (mRetryTicks) { --mRetryTicks; return; }
    }
    flushBatch();
}
void PersistenceQueue::enqueueKill(const PersistenceKillEvent& e)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mCurrent) return;
    auto& s = *mCurrent;
    auto old = s.lastDeath.find(e.victimIdentity);
    if (old != s.lastDeath.end() && old->second >= e.deathGeneration)
        { ++mTotalDuplicates; return; }
    s.lastDeath[e.victimIdentity] = e.deathGeneration;
    const bool self = e.attackerPlayerId != 0 && e.attackerPlayerId == e.victimPlayerId;
    const bool npc = e.victimType == "npc";
    const bool playerKiller = e.attackerType == "player" && !self;
    if (playerKiller && e.attackerPlayerId)
        mNotices.push_back({e.attackerPlayerId, static_cast<uint8_t>(npc ? 1 : 0), e.victimName, {}});
    if (!npc && e.victimPlayerId)
        mNotices.push_back({e.victimPlayerId, 2, self ? "yourself" :
            (e.attackerName.empty() ? "environment" : e.attackerName), {}});
    const auto& reward = getDefaultRewards();
    if (playerKiller && e.attackerId > 0)
    {
        auto& p = s.players[static_cast<uint64_t>(e.attackerId)];
        p.playerId = e.attackerPlayerId; p.name = e.attackerName;
        p.gains.totalXp += npc ? reward.npcKillXp : reward.playerKillXp;
        p.gains.gold += npc ? reward.npcKillGold : reward.playerKillGold;
        if (npc) ++p.gains.npcKills; else ++p.gains.playerKills;
        ++p.revision;
    }
    if (!npc && e.victimId > 0)
    {
        auto& p = s.players[static_cast<uint64_t>(e.victimId)];
        p.playerId = e.victimPlayerId; p.name = e.victimName;
        ++p.gains.deaths; ++p.revision;
    }
}
void PersistenceQueue::enqueueMatchResult(const PersistenceMatchEvent&) { requestSave("MATCH_END"); }
void PersistenceQueue::requestSave(const char* reason)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCurrent) mCurrent->reason = reason;
}
void PersistenceQueue::flushBatch()
{
    if (mWorker.valid())
    {
        if (mWorker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        mWorker.get();
    }
    std::shared_ptr<Session> selected;
    json registration;
    std::string token;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (size_t visited = 0; visited < mSessions.size(); ++visited)
        {
            const auto& s = mSessions[mNextSession++ % mSessions.size()];
            if (s->token.empty()) continue;
            json joining = json::array();
            // Existing immutable saves take priority over new ticket admission.
            if (s->pending.is_null())
                for (const auto& item : s->players)
                    if (!item.second.loaded)
                        joining.push_back({{"userId", std::to_string(item.first)}, {"joinTicket", item.second.ticket}});
            if (!joining.empty() || !s->pending.is_null() || !s->reason.empty())
            {
                selected = s; token = s->token;
                registration = {{"sessionId",s->id},{"roomCode",s->room},{"players",joining}};
                break;
            }
        }
        if (!selected) return;
        mRetryTicks = 300;
    }
    mWorker = std::async(std::launch::async, [this, selected, registration, token]() {
        try
        {
            if (!registration["players"].empty())
            {
                try {
                const json response = submitPersistenceBatch(token, registration.dump(), true);
                if (!response.value("success", false) || response.value("sessionId", "") != selected->id)
                    throw std::runtime_error("bootstrap_rejected_or_unavailable");
                std::lock_guard<std::mutex> lock(mMutex);
                for (const auto& result : response.at("players"))
                {
                    auto it = selected->players.find(integer(result,"userId"));
                    if (it == selected->players.end() || it->second.loaded) continue;
                    auto& p = it->second;
                    const auto initial = counters(result);
                    const auto accepted = counters(result.at("cumulative"));
                    const uint64_t revision = integer(result,"revision");
                    p.initial = initial;
                    p.gains.gold += accepted.gold; p.gains.totalXp += accepted.totalXp;
                    p.gains.playerKills += accepted.playerKills; p.gains.npcKills += accepted.npcKills;
                    p.gains.deaths += accepted.deaths; p.gains.playtimeTicks += accepted.playtimeTicks;
                    p.revision += revision; p.savedRevision = revision; p.loaded = true;
                    Debug::warn(Debug::Category::Networking,
                        "[PERSISTENCE] loaded session=%s user=%llu revision=%llu localRevision=%llu\n",
                        selected->id.c_str(), (unsigned long long)it->first,
                        (unsigned long long)revision, (unsigned long long)p.revision);
                }
                } catch (const std::exception&) {
                    std::lock_guard<std::mutex> lock(mMutex);
                    ++mTotalRetries;
                    Debug::warn(Debug::Category::Networking,
                        "[PERSISTENCE] bootstrap failed session=%s pendingAccounts=%zu retry=retained loadedAccountsMaySave=1\n",
                        selected->id.c_str(),registration["players"].size());
                }
            }
            json body;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (selected->pending.is_null() && !selected->reason.empty())
                {
                    json players = json::array();
                    for (const auto& item : selected->players)
                        if (item.second.loaded && item.second.revision > item.second.savedRevision)
                            players.push_back(snapshot(item.first, item.second.revision, item.second.gains));
                    if (!players.empty())
                        selected->pending = {{"sessionId",selected->id},
                            {"batchId",selected->id + "_" + std::to_string(++selected->batchSequence)},
                            {"reason",selected->reason},{"players",players}};
                    bool awaitingLoad = false;
                    for (const auto& item : selected->players) awaitingLoad |= !item.second.loaded;
                    if (!awaitingLoad) selected->reason.clear();
                }
                if (selected->pending.is_null()) return;
                body = selected->pending;
                if (selected == mCurrent)
                    for (const auto& row : body["players"])
                    {
                        const auto& p = selected->players.at(integer(row,"userId"));
                        mNotices.push_back({p.playerId,3,p.name,{}});
                    }
            }
            Debug::warn(Debug::Category::Networking,"[PERSISTENCE] attempt batch=%s reason=%s players=%zu\n",
                body["batchId"].get<std::string>().c_str(), body["reason"].get<std::string>().c_str(),body["players"].size());
            const json response = submitPersistenceBatch(token, body.dump());
            if (!response.value("success", false) || response.value("batchId", "") != body["batchId"] ||
                response.value("sessionId", "") != selected->id || response.value("confirmedAt", "").empty())
                throw std::runtime_error("save_unconfirmed");
            std::unordered_map<uint64_t,uint64_t> acknowledgements;
            for (const auto& row : response.at("players"))
                acknowledgements.emplace(integer(row,"userId"),integer(row,"revision"));
            for (const auto& row : body["players"])
            {
                auto ack = acknowledgements.find(integer(row,"userId"));
                if (ack == acknowledgements.end() || ack->second != integer(row,"revision"))
                    throw std::runtime_error("save_revision_mismatch");
            }
            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto& row : body["players"])
            {
                auto& p = selected->players.at(integer(row,"userId"));
                p.savedRevision = integer(row,"revision");
                if (selected == mCurrent)
                    mNotices.push_back({p.playerId,4,p.name,response["confirmedAt"].get<std::string>()});
            }
            mTotalSent += body["players"].size(); selected->pending = nullptr;
            Debug::warn(Debug::Category::Networking,"[PERSISTENCE] confirmed batch=%s at=%s newerRevisionsRetained=1\n",
                body["batchId"].get<std::string>().c_str(),response["confirmedAt"].get<std::string>().c_str());
        }
        catch (const std::exception& error)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            ++mTotalRetries;
            Debug::warn(Debug::Category::Networking,
                "[PERSISTENCE] failed session=%s batch=%s retry=retained dirty=1 totalRetries=%llu reason=%s\n",
                selected->id.c_str(), selected->pending.is_null() ? "bootstrap" :
                selected->pending["batchId"].get<std::string>().c_str(),(unsigned long long)mTotalRetries,
                error.what());
        }
    });
}
void PersistenceQueue::flushBlocking()
{
    if (mWorker.valid()) mWorker.get();
    requestSave("SERVER_SHUTDOWN");
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        flushBatch();
        if (mWorker.valid()) mWorker.get();
    }
    if (queueDepth()) Debug::warn(Debug::Category::Networking,
        "[PERSISTENCE] shutdown unconfirmed=%zu retainedInMemory=1 processExitMayLoseUnsavedGains=1\n",queueDepth());
}
std::vector<ProgressionNotice> PersistenceQueue::takeNotices()
{
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<ProgressionNotice> notices; notices.swap(mNotices); return notices;
}
size_t PersistenceQueue::queueDepth() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    size_t count = 0;
    for (const auto& s : mSessions) for (const auto& item : s->players)
        count += item.second.revision > item.second.savedRevision;
    return count;
}
uint64_t PersistenceQueue::totalSent() const { std::lock_guard<std::mutex> l(mMutex); return mTotalSent; }
uint64_t PersistenceQueue::totalRetries() const { std::lock_guard<std::mutex> l(mMutex); return mTotalRetries; }
uint64_t PersistenceQueue::totalDuplicates() const { std::lock_guard<std::mutex> l(mMutex); return mTotalDuplicates; }
void PersistenceQueue::resetStats() { std::lock_guard<std::mutex> l(mMutex); mTotalSent = mTotalRetries = mTotalDuplicates = 0; }
