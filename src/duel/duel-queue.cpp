// 08 10 2026, 14 34
/* purpose
* Implements the client-side duels queue: launches your own duel server (online
* mode from the start, mirroring the community browser), connects to it, polls
* the coordinator every few seconds, and on a match either hosts (opponent
* joins you) or abandons your server and joins the opponent's.
* Tracks a downtime stopwatch (seconds not fighting) that resets each fight.
* Does NOT run authoritative duel simulation or talk HTTP directly.
* Does NOT persist history (duel-history.h) or own the coordinator protocol.
*/

#include "duel/duel-queue.h"
#include "duel/duel-history.h"
#include "duel/duel-map-pool.h"

#include <chrono>
#include <random>
#include <thread>

#include "terminal/terminal-state.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "network/net_common.h"
#include "gui/gui-main.h"
#include "world/world-gltf-loader.h"
#include "game/spawn-utils.h"
#include "notifications/notifications.h"
#include "auth/auth-system.h"
#include "debug/debug-log.h"

using namespace MimitaNet;

namespace {

constexpr int kPollIntervalMs = 5000;
constexpr uint64_t kQueueServerStartTimeoutMs = 15000;
constexpr uint64_t kServerConnectTimeoutMs = 20000;
constexpr uint64_t kMatchConnectTimeoutMs = 60000;
constexpr uint64_t kOpponentJoinTimeoutMs = 30000;
constexpr uint64_t kStateStaleTimeoutMs = 3500;

// Unique per game instance: two exes on one PC get different session ids, so
// the coordinator treats them as two players even with the same account/name.
std::string makeSessionId()
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    return std::to_string(MimitaNet::nowMs()) + "_" + std::to_string(dist(rng));
}

} // namespace

std::string DuelQueue::defaultProfileId()
{
    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() == AuthState::Authenticated && auth.user().id > 0)
        return "acct_" + std::to_string(auth.user().id);
    return "guest_" + auth.displayName();
}

DuelQueue& DuelQueue::instance()
{
    static DuelQueue queue;
    return queue;
}

void DuelQueue::beginPolling()
{
    endPolling();
    mThreadStop.store(false);
    mPolling = true;
    mPollThread = std::thread(&DuelQueue::pollThread, this);
}

void DuelQueue::endPolling()
{
    mThreadStop.store(true);
    if (mPollThread.joinable())
        mPollThread.join();
    mPolling = false;
}

void DuelQueue::pollThread()
{
    while (!mThreadStop.load())
    {
        std::string ticketId;
        bool doPoll = false;
        {
            std::lock_guard<std::mutex> lock(mSharedMutex);
            ticketId = mTicketId;
            doPoll = mPolling;
        }

        if (doPoll && !ticketId.empty())
        {
            QueuePollResult poll = coordinatorQueuePoll(ticketId);
            std::lock_guard<std::mutex> lock(mSharedMutex);
            if (poll.status == "matched_host")
            {
                mPendingHost = true;
                mMatchId = poll.matchId;
                mMapName = poll.map;
                mOpponentName = poll.opponentName;
                mPolling = false;
            }
            else if (poll.status == "match_ready")
            {
                mPendingClient = true;
                mMatchId = poll.matchId;
                mRoomCode = poll.roomCode;
                mMapName = poll.map;
                mOpponentName = poll.opponentName;
                mPolling = false;
            }
            else if (poll.status == "waiting_for_host")
            {
                mMatchId = poll.matchId;
                mMapName = poll.map;
                mOpponentName = poll.opponentName;
            }
            else if (poll.status == "cancelled" || poll.status == "error" ||
                     poll.status == "ticket-not-found")
            {
                mCancelled = true;
                mPolling = false;
            }
        }

        // Sleep in small slices so stop requests land within ~100ms.
        for (int i = 0; i < kPollIntervalMs / 100 && !mThreadStop.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void DuelQueue::startQueue(const std::string& profileId, const std::string& name,
                           const std::string& preferOpponentId,
                           const std::vector<std::string>& maps)
{
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        mProfileId = profileId;
        mName = name;
        mPreferOpponent = preferOpponentId;
        mMaps = maps;
        mTicketId.clear();
        mMatchId.clear();
        mRoomCode.clear();
        mPendingHost = false;
        mPendingClient = false;
        mCancelled = false;
        mPolling = true;
    }

    if (mMaps.empty())
        mMaps = DuelMapPool::instance().list();
    mChosenMap = DuelMapPool::instance().randomMap();
    // One stable session id per game instance (used as the queue ticket key).
    if (mSessionId.empty())
        mSessionId = makeSessionId();

    // Tear down any leftover session/server before launching a fresh one.
    MimitaNet::mpShutdown(MP_CONTEXT);
    stopExternalServerProcess();
    THE_NPC_SYSTEM.destroyAll();

    mState = DuelQueueState::Queuing;
    mDowntime = 0.0f;
    mLastDowntime = 0.0f;
    mWasFighting = false;
    mDuelPhase = DUEL_PHASE_WAITING;
    mStatusText = "Starting your duel server...";
    mMatchFoundBanner = false;
    mMatchFoundTimer = 0.0f;
    mPhaseStartMs = nowMs();
    mQueueRoomCode.clear();
    mServerConnectStarted = false;
    mQueueServerConnected = false;
    mJoinedCoordinator = false;
    mJoinRetryTimer = 0.0f;
    mServerConnectStartMs = 0;
    mWantsChosenMap = true;
    mInDuel = false;
    mMatchOver = false;
    mHistoryRecorded = false;
    mLastStateMs = 0;

    // Online mode from the start: launch your own duel server on your map.
    if (!launchDuelHostServer(mChosenMap))
        mStatusText = "Could not start duel server - retrying";

    NotificationSystem::instance().push(
        "Duels queue", "Now queued - matchmaking...", 240, {});
    Debug::log(Debug::Category::Duel, "[DUEL QUEUE] queuing map=%s prefer=%s\n",
               mChosenMap.c_str(), preferOpponentId.c_str());
    beginPolling();
}

void DuelQueue::stopQueue()
{
    std::string ticketId;
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        ticketId = mTicketId;
        mTicketId.clear();
        mPendingHost = false;
        mPendingClient = false;
        mPolling = false;
    }
    if (!ticketId.empty())
        coordinatorQueueLeave(ticketId);
    endPolling();

    MimitaNet::mpShutdown(MP_CONTEXT);
    stopExternalServerProcess();
    THE_NPC_SYSTEM.destroyAll();

    mState = DuelQueueState::Idle;
    mWantsChosenMap = false;
    mWantsMatchMap = false;
    mMatchFoundBanner = false;
    mInDuel = false;
    mMatchOver = false;
    mCountdownActive = false;
    mTracerActive = false;
}

void DuelQueue::returnToQueue()
{
    std::string ticketId;
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        ticketId = mTicketId;
        mTicketId.clear();
        mPendingHost = false;
        mPendingClient = false;
        mPolling = false;
    }
    if (!ticketId.empty())
        coordinatorQueueLeave(ticketId);
    endPolling();

    // Tear down the server session and the host's launched server process.
    MimitaNet::mpShutdown(MP_CONTEXT);
    stopExternalServerProcess();
    THE_NPC_SYSTEM.destroyAll();

    // Capture the duel context BEFORE startQueue resets it.
    const std::string opponent = mOpponentName;
    const std::string roomCode = mRoomCode.empty() ? mQueueRoomCode : mRoomCode;
    std::string msg = "Joining duels queue - left duel with " +
        (opponent.empty() ? std::string("opponent") : opponent) +
        " in room " + (roomCode.empty() ? std::string("?") : roomCode);

    // Re-queue, preferring the last opponent so an immediate rematch wins.
    startQueue(mProfileId, mName, mLastOpponentId, mMaps);
    NotificationSystem::instance().push("Duels queue", msg, 240, {});
}

void DuelQueue::rejoinQueue()
{
    std::string ticketId;
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        ticketId = mTicketId;
        mTicketId.clear();
        mPendingHost = false;
        mPendingClient = false;
        mCancelled = false;
        mPolling = true;
    }
    if (!ticketId.empty())
        coordinatorQueueLeave(ticketId);

    mJoinedCoordinator = false;
    mState = DuelQueueState::Queuing;
    mStatusText = "Re-queuing...";
    NotificationSystem::instance().push(
        "Duels queue", "Re-queuing...", 180, {});
    beginPolling();
}

void DuelQueue::loadChosenMap()
{
    if (mChosenMap.empty()) return;
    const std::string path = "assets/maps/" + mChosenMap + ".glb";
    if (loadWorldFromGLB(THE_WORLD, path.c_str()))
    {
        ACTIVE_MAP_PATH = path;
        WORLD_LOADED = true;
        THE_NPC_SYSTEM.destroyAll();
        Debug::log(Debug::Category::Duel, "[DUEL QUEUE] loaded chosen map %s\n", path.c_str());
    }
}

void DuelQueue::loadMatchMap()
{
    if (mMapName.empty()) return;
    const std::string path = "assets/maps/" + mMapName + ".glb";
    if (loadWorldFromGLB(THE_WORLD, path.c_str()))
    {
        ACTIVE_MAP_PATH = path;
        WORLD_LOADED = true;
        THE_NPC_SYSTEM.destroyAll();
        Debug::log(Debug::Category::Duel, "[DUEL] loaded match map %s\n", path.c_str());
    }
    else
    {
        Debug::warn(Debug::Category::Duel, "[DUEL] failed to load match map %s\n", path.c_str());
    }
}

void DuelQueue::handleHostMatch()
{
    mHost = true;
    mMatchFoundBanner = true;
    mMatchFoundTimer = 0.0f;
    mState = DuelQueueState::MatchFound;
    mStatusText = "opponent joining your room...";
    mPhaseStartMs = nowMs();
    NotificationSystem::instance().push(
        "match found!", "vs " + mOpponentName, 300, {});
    Debug::log(Debug::Category::Duel, "[DUEL HOST] matched - opponent joins my room\n");
}

void DuelQueue::handleClientMatch()
{
    mHost = false;
    mLastOpponentId = mOpponentName;
    mMatchFoundBanner = true;
    mMatchFoundTimer = 0.0f;

    // Leave our own queue server; join the host's room.
    MimitaNet::mpShutdown(MP_CONTEXT);
    stopExternalServerProcess();
    THE_NPC_SYSTEM.destroyAll();
    mQueueServerConnected = false;
    mJoinedCoordinator = false;

    mWantsMatchMap = true;
    mState = DuelQueueState::Connecting;
    mPhaseStartMs = nowMs();
    mConnectedSinceMs = 0;
    mStatusText = "Joining...";
    NotificationSystem::instance().push(
        "match found!", "joining " + mOpponentName + "'s room", 300, {});
    Debug::log(Debug::Category::Duel, "[DUEL CLIENT] matched - joining host room=%s\n",
               mRoomCode.c_str());

    if (!mpIceConnectStart(MP_CONTEXT, mRoomCode, mName))
        failToQueue("Could not connect to opponent's room");
}

void DuelQueue::updateQueuing(float dt)
{
    mDowntime += dt;

    if (mWantsChosenMap)
    {
        mWantsChosenMap = false;
        loadChosenMap();
    }

    // 1) Start / connect to our own queue server (online mode from the start).
    if (!mQueueServerConnected)
    {
        if (mQueueRoomCode.empty())
        {
            std::string code;
            if (pollDuelServerRoomCode(code))
            {
                mQueueRoomCode = code;
                mServerConnectStartMs = nowMs();
                mServerConnectStarted = false;
            }
        }
        if (!mQueueRoomCode.empty() && !mServerConnectStarted)
        {
            mServerConnectStarted = mpIceConnectStart(MP_CONTEXT, mQueueRoomCode, mName);
            mServerConnectStartMs = nowMs();
        }
        if (!mQueueRoomCode.empty() && MP_CONTEXT.active)
        {
            mQueueServerConnected = true;
            mStatusText = "In queue - waiting for a duel";
            NotificationSystem::instance().push(
                "Duels queue",
                mChosenMap.empty() ? "In queue - practice while you wait"
                                   : "In queue - map: " + mChosenMap,
                240, {});
            Debug::log(Debug::Category::Duel, "[DUEL QUEUE] connected to own server code=%s\n",
                       mQueueRoomCode.c_str());
        }
        else if (!mQueueRoomCode.empty() && mServerConnectStartMs != 0 &&
                 nowMs() - mServerConnectStartMs > kServerConnectTimeoutMs)
        {
            // ICE connect to own server stalled - retry the server launch.
            mStatusText = "Starting server... (retrying)";
            Debug::warn(Debug::Category::Duel, "[DUEL QUEUE] own server connect stalled; relaunching\n");
            stopExternalServerProcess();
            mQueueRoomCode.clear();
            mServerConnectStartMs = 0;
            mServerConnectStarted = false;
            if (launchDuelHostServer(mChosenMap))
                mStatusText = "Starting your duel server...";
        }
        else if (nowMs() - mPhaseStartMs > kQueueServerStartTimeoutMs &&
                 mQueueRoomCode.empty())
        {
            mStatusText = "Starting server... (retrying)";
            mPhaseStartMs = nowMs();
            stopExternalServerProcess();
            if (launchDuelHostServer(mChosenMap))
                mStatusText = "Starting your duel server...";
        }
        return; // no matchmaking until connected + joined
    }

    // 2) Join the coordinator with our live room code.
    if (!mJoinedCoordinator)
    {
        if (mJoinRetryTimer > 0.0f)
        {
            mJoinRetryTimer -= dt;
            return;
        }
        QueueJoinResult join = coordinatorQueueJoin(
            mProfileId, mName, mPreferOpponent, mMaps, mChosenMap, mQueueRoomCode, mSessionId);
        if (join.ok)
        {
            {
                std::lock_guard<std::mutex> lock(mSharedMutex);
                mTicketId = join.ticketId;
            }
            mJoinedCoordinator = true;
            mStatusText = "In queue - waiting for a duel";
            Debug::log(Debug::Category::Duel, "[DUEL QUEUE] joined coordinator ticket=%s\n",
                       join.ticketId.substr(0, 8).c_str());
        }
        else
        {
            mStatusText = "Can't reach matchmaking - retrying";
            mJoinRetryTimer = 3.0f;
        }
        return;
    }

    // 3) Act on matchmaker results.
    bool host = false, client = false, cancelled = false;
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        host = mPendingHost;
        client = mPendingClient;
        cancelled = mCancelled;
    }
    if (host)
        handleHostMatch();
    else if (client)
        handleClientMatch();
    else if (cancelled)
    {
        NotificationSystem::instance().pushCritical(
            "Match cancelled", "Re-queuing...", 180);
        rejoinQueue();
    }
}

void DuelQueue::updateMatchFound(float dt)
{
    mDowntime += dt;
    mMatchFoundTimer += dt;

    // Host is waiting for the opponent to connect to our room.
    if (mHost && nowMs() - mPhaseStartMs > kOpponentJoinTimeoutMs)
    {
        NotificationSystem::instance().pushCritical(
            "Opponent left", "Re-queuing...", 180);
        rejoinQueue();
    }
}

void DuelQueue::updateConnecting(float dt)
{
    mDowntime += dt;

    if (mWantsMatchMap)
    {
        mWantsMatchMap = false;
        loadMatchMap();
    }
    if (MP_CONTEXT.active)
    {
        if (mConnectedSinceMs == 0)
            mConnectedSinceMs = nowMs();
        // Connected to the host's server; the countdown arrives via DuelState.
        mStatusText = "Connected - waiting for countdown...";
        NotificationSystem::instance().push(
            "Duels", "Connected to " + mOpponentName + "'s room", 240, {});
        Debug::log(Debug::Category::Duel, "[DUEL QUEUE] connected to host - waiting countdown\n");
        if (nowMs() - mConnectedSinceMs > 15000)
        {
            NotificationSystem::instance().pushCritical(
                "Opponent left", "Returning to the queue...", 180);
            returnToQueue();
        }
    }
    else if (nowMs() - mPhaseStartMs > kMatchConnectTimeoutMs)
    {
        failToQueue("Timed out connecting to opponent");
    }
}

void DuelQueue::updateInDuel(float dt)
{
    // Downtime stopwatch: counts while NOT actively fighting; resets on fight.
    const bool fighting = (mDuelPhase == DUEL_PHASE_ACTIVE);
    if (fighting && !mWasFighting)
    {
        mLastDowntime = mDowntime;
        mDowntime = 0.0f;
    }
    mWasFighting = fighting;
    if (!fighting)
        mDowntime += dt;

    // Spawn tracer timer.
    if (mTracerActive)
    {
        mTracerTime += dt;
        if (mTracerTime >= mTracerDuration)
            mTracerActive = false;
    }
    // Detect a dead server / lost opponent via missing DuelState packets
    // (the server broadcasts every ~1s during Active and MatchEnd).
    if (mLastStateMs != 0 && nowMs() - mLastStateMs > kStateStaleTimeoutMs)
    {
        NotificationSystem::instance().pushCritical(
            "Opponent left", "Returning to the queue...", 180);
        returnToQueue();
    }
}

void DuelQueue::updateFailed(float dt)
{
    mDowntime += dt;
    mFailedTimer -= dt;
    if (mFailedTimer <= 0.0f)
        rejoinQueue();
}

void DuelQueue::update(float dt)
{
    switch (mState)
    {
    case DuelQueueState::Idle:
        break;
    case DuelQueueState::Queuing:
        updateQueuing(dt);
        break;
    case DuelQueueState::MatchFound:
        updateMatchFound(dt);
        break;
    case DuelQueueState::Connecting:
        updateConnecting(dt);
        break;
    case DuelQueueState::InDuel:
    case DuelQueueState::MatchEnd:
        updateInDuel(dt);
        break;
    case DuelQueueState::Failed:
        updateFailed(dt);
        break;
    }
}

void DuelQueue::failToQueue(const char* reason)
{
    Debug::warn(Debug::Category::Duel, "[DUEL QUEUE] %s\n", reason);
    mState = DuelQueueState::Failed;
    mStatusText = reason;
    mFailedTimer = 3.0f;
}

void DuelQueue::onDuelState(const DuelStatePacket& pkt)
{
    if (mState == DuelQueueState::Idle)
        return;

    mLastStateMs = nowMs();

    // WAITING just means "your queue server is up but no opponent yet".
    if (pkt.phase == DUEL_PHASE_WAITING)
    {
        mDuelPhase = DUEL_PHASE_WAITING;
        mCountdownActive = false;
        return;
    }

    mDuelPhase = pkt.phase;
    mInDuel = true;
    mConnectedSinceMs = 0;

    // Determine which side is ours.
    const uint32_t myId = MP_CONTEXT.localPlayerId;
    const bool amA = (pkt.playerAId != 0 && myId == pkt.playerAId) ||
                     (pkt.playerAId == 0);
    mMyPlayerId = myId;
    mEnemyPlayerId = amA ? pkt.playerBId : pkt.playerAId;
    mGoal = pkt.goalValue;
    mMyTeamName = amA ? pkt.teamAName : pkt.teamBName;
    mEnemyTeamName = amA ? pkt.teamBName : pkt.teamAName;
    mMyScore = amA ? pkt.scoreA : pkt.scoreB;
    mOppScore = amA ? pkt.scoreB : pkt.scoreA;
    mWinnerId = pkt.winnerPlayerId;

    switch (static_cast<DuelStatePhase>(pkt.phase))
    {
    case DUEL_PHASE_COUNTDOWN:
        mCountdownActive = true;
        mCountdownLeft = pkt.countdownLeft;
        if (mState == DuelQueueState::MatchEnd)
        {
            // A rematch began: clear the end-of-match view.
            mMatchOver = false;
            mWon = false;
            mHistoryRecorded = false;
            NotificationSystem::instance().push(
                "Rematch", "vs " + mOpponentName, 240, {});
        }
        mState = DuelQueueState::InDuel;
        break;

    case DUEL_PHASE_ACTIVE:
        mCountdownActive = false;
        mMatchOver = false;
        if (mState == DuelQueueState::MatchEnd)
            mHistoryRecorded = false;
        mState = DuelQueueState::InDuel;
        break;

    case DUEL_PHASE_MATCH_END:
        mCountdownActive = false;
        mMatchOver = true;
        mWon = (mWinnerId != 0 && mWinnerId == mMyPlayerId);
        mRematchLeft = pkt.rematchLeft;
        mState = DuelQueueState::MatchEnd;
        recordHistoryOnce();
        break;

    case DUEL_PHASE_WAITING:
    default:
        break;
    }
}

void DuelQueue::onDuelEnemySpawn(const DuelEnemySpawnPacket& pkt)
{
    if (pkt.enemyPlayerId == 0 || pkt.enemyPlayerId == mMyPlayerId)
        return;
    mTracerActive = true;
    mTracerPos = glm::vec3(pkt.posX, pkt.posY, pkt.posZ);
    mTracerTime = 0.0f;
}

void DuelQueue::recordHistoryOnce()
{
    if (mHistoryRecorded || !mMatchOver)
        return;
    mHistoryRecorded = true;

    DuelHistoryEntry entry;
    entry.opponentName = mOpponentName;
    entry.opponentProfileId = mLastOpponentId;
    entry.won = mWon;
    entry.myScore = mMyScore;
    entry.oppScore = mOppScore;
    entry.map = mMapName;
    entry.unixMs = nowMs();
    DuelHistory::instance().add(std::move(entry));
}

void DuelQueue::requestRematch()
{
    requestRematchWith(mLastOpponentId);
}

void DuelQueue::requestRematchWith(const std::string& opponentId)
{
    if (opponentId.empty())
    {
        NotificationSystem::instance().push(
            "Rematch", "No recent opponent to rematch", 180, {});
        return;
    }
    if (mState == DuelQueueState::MatchEnd)
    {
        // Still connected - the server auto-rematches; just wait.
        return;
    }
    // Re-queue with the opponent as highest priority.
    mPreferOpponent = opponentId;
    startQueue(mProfileId, mName, opponentId, mMaps);
    NotificationSystem::instance().push(
        "Rematch", "Looking for " + opponentId + " again...", 240, {});
}

void DuelQueue::requestRematchNow()
{
    if (mState != DuelQueueState::MatchEnd)
        return;
    DuelRematchRequestPacket pkt{};
    pkt.header.type = PACKET_DUEL_REMATCH_REQUEST;
    pkt.header.tick = 0;
    pkt.header.playerId = MP_CONTEXT.localPlayerId;
    mpSendPacket(MP_CONTEXT, &pkt, sizeof(pkt));
    Debug::log(Debug::Category::Duel, "[DUEL] rematch now requested (space)\n");
}

void DuelQueue::onMapChange(const std::string& mapId)
{
    mMapName = mapId;
    NotificationSystem::instance().push(
        "Map changed", "Now fighting on " + mapId, 200, {});
}
