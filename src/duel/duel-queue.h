// 08 10 2026, 14 34
/* purpose
* Owns the client-side duels queue: launches your own duel server (online mode
* from the start), polls the coordinator matchmaker, connects to a matched
* opponent, tracks the downtime stopwatch, and presents the duel state to the
* HUD.
* Works for guests AND accounts - no auth required to queue.
* Does NOT run the authoritative duel simulation (that is server-side).
* Does NOT persist match history (see duel-history.h).
* Does NOT own the coordinator HTTP protocol (see coordinator-client.h).
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "network/packets.h"

enum class DuelQueueState
{
    Idle,
    Queuing,        // in your own online duel server, waiting for a match
    MatchFound,     // matched; host waits for the opponent to join
    Connecting,     // client connecting to the host's server
    InDuel,         // actively fighting
    MatchEnd,       // win/lose screen + rematch window
    Failed          // transient issue; visible status + auto retry
};

class DuelQueue
{
public:
    static DuelQueue& instance();

    void startQueue(const std::string& profileId, const std::string& name,
                    const std::string& preferOpponentId,
                    const std::vector<std::string>& maps);
    // Stable identity used for the queue + rematch, works for guests and accounts.
    static std::string defaultProfileId();
    // Leave the queue entirely (stops the queue server) and return to the menu.
    void stopQueue();
    // Leave the current match and re-enter the queue (fresh queue server).
    void returnToQueue();

    // Main-thread per-frame update.
    void update(float dt);

    // Server -> client packet handlers (called from mpTick).
    void onDuelState(const MimitaNet::DuelStatePacket& pkt);
    void onDuelEnemySpawn(const MimitaNet::DuelEnemySpawnPacket& pkt);

    // World/map handoff flags consumed by engine-tick-state.
    bool wantsChosenMap() const { return mWantsChosenMap; }
    void clearChosenMapFlag() { mWantsChosenMap = false; }
    bool wantsMatchMap() const { return mWantsMatchMap; }
    void clearMatchMapFlag() { mWantsMatchMap = false; }
    std::string matchMapName() const { return mMapName; }

    // ── HUD accessors ────────────────────────────────────────────────
    bool isActive() const { return mState != DuelQueueState::Idle; }
    DuelQueueState state() const { return mState; }
    // The downtime stopwatch: seconds NOT actively fighting. Resets each fight.
    float queueElapsed() const { return mDowntime; }
    float downtime() const { return mDowntime; }
    float lastDowntime() const { return mLastDowntime; }
    std::string statusText() const { return mStatusText; }
    std::string opponentName() const { return mOpponentName; }
    std::string playerName() const { return mName; }
    bool matchFoundBanner() const { return mMatchFoundBanner; }
    std::string profileId() const { return mProfileId; }

    // Match view
    bool inDuel() const { return mInDuel; }
    int myScore() const { return mMyScore; }
    int oppScore() const { return mOppScore; }
    int goal() const { return mGoal; }
    bool countdownActive() const { return mCountdownActive; }
    float countdownLeft() const { return mCountdownLeft; }
    bool matchOver() const { return mMatchOver; }
    bool won() const { return mWon; }
    float rematchLeft() const { return mRematchLeft; }
    std::string teamName(bool mine) const { return mine ? mMyTeamName : mEnemyTeamName; }

    // Spawn tracer
    bool tracerActive() const { return mTracerActive; }
    glm::vec3 tracerPos() const { return mTracerPos; }

    void requestRematch();
    void requestRematchWith(const std::string& opponentId);
    // Send PACKET_DUEL_REMATCH_REQUEST to skip the rematch timer (Space key).
    void requestRematchNow();
    // Server broadcast the map changed live (PACKET_MAP_CHANGE).
    void onMapChange(const std::string& mapId, uint32_t duelId = 0, uint32_t mapVersion = 0);
    std::string lastOpponentId() const { return mLastOpponentId; }
    // The random map picked from the pool when this player queued.
    std::string chosenMap() const { return mChosenMap; }

private:
    DuelQueue() = default;
    DuelQueue(const DuelQueue&) = delete;
    DuelQueue& operator=(const DuelQueue&) = delete;

    void pollThread();
    void beginPolling();
    void endPolling();
    void handleHostMatch();
    void handleClientMatch();
    void loadChosenMap();
    void loadMatchMap();
    void recordHistoryOnce();
    void failToQueue(const char* reason);
    // Re-join the coordinator with the still-running queue server (no relaunch).
    void rejoinQueue();
    // update() sub-steps (one per state) so each function stays small.
    void updateQueuing(float dt);
    void updateMatchFound(float dt);
    void updateConnecting(float dt);
    void updateInDuel(float dt);
    void updateFailed(float dt);

    // Identity / queue
    std::string mProfileId;
    std::string mName;
    std::string mPreferOpponent;
    std::vector<std::string> mMaps;
    std::string mChosenMap;
    // Unique per game instance so two exes (same account/guest name) are two
    // separate queue tickets on the coordinator.
    std::string mSessionId;

    // Shared with the poll thread (mutex guarded)
    std::mutex mSharedMutex;
    std::string mTicketId;
    std::string mMatchId;
    std::string mRoomCode;      // host's room code when matched as client
    bool mPendingHost = false;
    bool mPendingClient = false;
    bool mCancelled = false;
    bool mPolling = false;

    // Main-thread state
    DuelQueueState mState = DuelQueueState::Idle;
    float mDowntime = 0.0f;
    float mLastDowntime = 0.0f;
    bool mWasFighting = false;
    uint8_t mDuelPhase = MimitaNet::DUEL_PHASE_WAITING;
    std::string mStatusText;
    std::string mOpponentName;
    std::string mMapName;
    bool mMatchFoundBanner = false;
    float mMatchFoundTimer = 0.0f;
    uint64_t mPhaseStartMs = 0;
    float mFailedTimer = 0.0f;
    bool mHost = false;

    // Queue server lifecycle (started at queue time, online mode from the start)
    std::string mQueueRoomCode;
    bool mServerConnectStarted = false;
    bool mQueueServerConnected = false;
    bool mJoinedCoordinator = false;
    float mJoinRetryTimer = 0.0f;
    uint64_t mServerConnectStartMs = 0;

    // Map handoff
    bool mWantsChosenMap = false;
    bool mWantsMatchMap = false;

    // Match view
    bool mInDuel = false;
    bool mMatchOver = false;
    bool mWon = false;
    int mMyScore = 0;
    int mOppScore = 0;
    int mGoal = 20;
    uint32_t mMyPlayerId = 0;
    uint32_t mEnemyPlayerId = 0;
    uint32_t mWinnerId = 0;
    uint32_t mDuelId = 0;
    uint32_t mMapVersion = 0;
    uint32_t mSpawnAnchorVersion = 0;
    uint32_t mRespawnSequence = 0;
    uint32_t mStateVersion = 0;
    bool mCountdownActive = false;
    float mCountdownLeft = 0.0f;
    float mRematchLeft = 0.0f;
    std::string mMyTeamName = "RED";
    std::string mEnemyTeamName = "BLUE";
    bool mHistoryRecorded = false;

    // Spawn tracer
    bool mTracerActive = false;
    glm::vec3 mTracerPos{0.0f};
    float mTracerTime = 0.0f;
    float mTracerDuration = 1.5f;

    // Rematch
    std::string mLastOpponentId;

    // Thread
    std::thread mPollThread;
    std::atomic<bool> mThreadStop{false};
};
