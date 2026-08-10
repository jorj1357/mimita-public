// 08 10 2026, 14 34
/* purpose
* Implements the client-side duels queue flow: coordinator matchmaking poll,
* sandbox practice wait, match found banner, host/client connect, and the
* live duel HUD view driven by PACKET_DUEL_STATE / PACKET_DUEL_ENEMY_SPAWN.
* Works for guests AND accounts - no auth required.
* Does NOT run authoritative duel simulation (server-side).
* Does NOT persist history (duel-history.h) or talk HTTP (coordinator-client.h).
*/

#include "duel/duel-queue.h"
#include "duel/duel-history.h"

#include <chrono>
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

constexpr int kPollIntervalMs = 1000;
constexpr uint64_t kMatchConnectTimeoutMs = 60000;
constexpr uint64_t kHostLaunchTimeoutMs = 25000;
constexpr uint64_t kStateStaleTimeoutMs = 3500;

const char* practiceMapPath()
{
    return "assets/maps/mimita-aabb-only-interior-small-v4.glb";
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
        bool shouldPoll = false;
        {
            std::lock_guard<std::mutex> lock(mSharedMutex);
            ticketId = mTicketId;
            shouldPoll = mPolling;
        }

        if (shouldPoll && !ticketId.empty())
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
            else if (poll.status == "cancelled")
            {
                mCancelled = true;
                mPolling = false;
            }
            else if (poll.status == "error" || poll.status == "ticket-not-found")
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

    QueueJoinResult join = coordinatorQueueJoin(profileId, name, preferOpponentId, maps);
    if (!join.ok)
    {
        Debug::warn(Debug::Category::Duel, "[DUEL QUEUE] join failed: %s\n",
                    join.errorCode.c_str());
        mState = DuelQueueState::Failed;
        mStatusText = "Could not reach matchmaking";
        mFailedTimer = 3.0f;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        mTicketId = join.ticketId;
    }

    mState = DuelQueueState::Queuing;
    mQueueElapsed = 0.0f;
    mStatusText = "In queue - waiting for a duel";
    mMatchFoundBanner = false;
    mMatchFoundTimer = 0.0f;
    mPhaseStartMs = nowMs();
    mWantsSandbox = true;
    mInDuel = false;
    mMatchOver = false;
    mHistoryRecorded = false;
    mLastStateMs = 0;

    Debug::log(Debug::Category::Duel, "[DUEL QUEUE] queuing ticket=%s prefer=%s\n",
               join.ticketId.substr(0, 8).c_str(), preferOpponentId.c_str());
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

    mState = DuelQueueState::Idle;
    mWantsSandbox = false;
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

    // Re-queue, preferring the last opponent so an immediate rematch wins.
    startQueue(mProfileId, mName, mLastOpponentId, mMaps);
    NotificationSystem::instance().push(
        "Duel over", "Back in the queue - matchmaking...", 180, {});
}

void DuelQueue::onHostRoomCodeReady(const std::string& roomCode)
{
    std::string matchId;
    {
        std::lock_guard<std::mutex> lock(mSharedMutex);
        mRoomCode = roomCode;
        matchId = mMatchId;
    }
    if (matchId.empty())
        return;
    if (coordinatorQueueHostReady(matchId, roomCode))
        Debug::log(Debug::Category::Duel, "[DUEL HOST] posted host-ready code=%s\n",
                   roomCode.c_str());
    mStatusText = "Connecting to opponent...";
    mWantsMatchMap = true;
}

void DuelQueue::handleHostMatch()
{
    Debug::log(Debug::Category::Duel, "[DUEL HOST] launching duel server map=%s\n",
               mMapName.c_str());
    if (!launchDuelHostServer(mMapName))
    {
        failToQueue("Could not start duel server");
        return;
    }
    mState = DuelQueueState::HostLaunching;
    mPhaseStartMs = nowMs();
    mStatusText = "Starting duel server...";
    mWantsMatchMap = true;
}

void DuelQueue::handleClientMatch()
{
    Debug::log(Debug::Category::Duel, "[DUEL CLIENT] connecting to host room=%s\n",
               mRoomCode.c_str());
    mWantsMatchMap = true;
    const bool started = mpIceConnectStart(MP_CONTEXT, mRoomCode, mName);
    if (!started)
    {
        failToQueue("Could not start connection to opponent");
        return;
    }
    mState = DuelQueueState::Connecting;
    mPhaseStartMs = nowMs();
    mStatusText = "Connecting to opponent...";
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
        Debug::warn(Debug::Category::Duel, "[DUEL] failed to load match map %s\n",
                    path.c_str());
    }
}

void DuelQueue::enterSandbox()
{
    if (loadWorldFromGLB(THE_WORLD, practiceMapPath()))
    {
        ACTIVE_MAP_PATH = practiceMapPath();
        WORLD_LOADED = true;
        THE_NPC_SYSTEM.destroyAll();
        THE_PLAYER.reset();

        // A couple of practice NPCs to shoot while waiting.
        for (int i = 0; i < 2; ++i)
            spawnNpcAtSafePosition(THE_NPC_SYSTEM, THE_NPC_SYSTEM.nextNpcId(),
                                   5.0f, THE_WORLD, i);

        NotificationSystem::instance().push(
            "Duels queue", "In queue - practice while you wait", 240, {});
        Debug::log(Debug::Category::Duel, "[DUEL QUEUE] sandbox practice ready\n");
    }
}

void DuelQueue::failToQueue(const char* reason)
{
    Debug::warn(Debug::Category::Duel, "[DUEL QUEUE] %s\n", reason);
    mState = DuelQueueState::Failed;
    mStatusText = reason;
    mFailedTimer = 3.0f;
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

void DuelQueue::update(float dt)
{
    // Sandbox / match-map handoffs to engine-tick-state are flag-based.
    switch (mState)
    {
    case DuelQueueState::Idle:
        break;

    case DuelQueueState::Queuing:
        mQueueElapsed += dt;
        if (mWantsSandbox)
        {
            mWantsSandbox = false;
            enterSandbox();
        }
        {
            bool host = false, client = false, cancelled = false;
            {
                std::lock_guard<std::mutex> lock(mSharedMutex);
                host = mPendingHost;
                client = mPendingClient;
                cancelled = mCancelled;
            }
            if (host)
            {
                mHost = true;
                mLastOpponentId = mOpponentName;
                mMatchFoundBanner = true;
                mMatchFoundTimer = 0.0f;
                mState = DuelQueueState::MatchFound;
                mStatusText = "match found!!!!!!!!";
            }
            else if (client)
            {
                mHost = false;
                mLastOpponentId = mOpponentName;
                mMatchFoundBanner = true;
                mMatchFoundTimer = 0.0f;
                mState = DuelQueueState::MatchFound;
                mStatusText = "match found!!!!!!!!";
            }
            else if (cancelled)
            {
                // Match cancelled / ticket dropped - re-queue fresh.
                std::string profileId = mProfileId;
                std::string name = mName;
                std::vector<std::string> maps = mMaps;
                NotificationSystem::instance().pushCritical(
                    "Match cancelled", "Re-queuing...", 180);
                startQueue(profileId, name, mPreferOpponent, maps);
            }
        }
        break;

    case DuelQueueState::MatchFound:
        mMatchFoundTimer += dt;
        if (mHost && mMatchFoundTimer >= 2.0f)
            handleHostMatch();
        else if (!mHost && mMatchFoundTimer >= 2.0f)
            handleClientMatch();
        break;

    case DuelQueueState::HostLaunching:
    {
        std::string code;
        if (pollDuelServerRoomCode(code))
        {
            onHostRoomCodeReady(code);
            mState = DuelQueueState::Connecting;
            mPhaseStartMs = nowMs();
            const bool started = mpIceConnectStart(MP_CONTEXT, code, mName);
            if (!started)
                failToQueue("Could not connect to own duel server");
        }
        else if (nowMs() - mPhaseStartMs > kHostLaunchTimeoutMs)
        {
            failToQueue("Duel server took too long to start");
        }
        break;
    }

    case DuelQueueState::Connecting:
        if (mWantsMatchMap)
        {
            mWantsMatchMap = false;
            loadMatchMap();
        }
        if (MP_CONTEXT.active)
        {
            mState = DuelQueueState::InDuel;
            mStatusText = "";
            mLastStateMs = nowMs();
            Debug::log(Debug::Category::Duel, "[DUEL QUEUE] connected - in duel\n");
        }
        else if (nowMs() - mPhaseStartMs > kMatchConnectTimeoutMs)
        {
            failToQueue("Timed out connecting to opponent");
        }
        break;

    case DuelQueueState::InDuel:
    case DuelQueueState::MatchEnd:
    {
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
        break;
    }

    case DuelQueueState::Failed:
        mFailedTimer -= dt;
        if (mFailedTimer <= 0.0f)
        {
            std::string profileId = mProfileId;
            std::string name = mName;
            std::vector<std::string> maps = mMaps;
            startQueue(profileId, name, mPreferOpponent, maps);
        }
        break;
    }
}

void DuelQueue::onDuelState(const DuelStatePacket& pkt)
{
    if (mState == DuelQueueState::Idle)
        return;

    mLastStateMs = nowMs();
    mInDuel = true;

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
        }
        if (mState != DuelQueueState::InDuel)
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
