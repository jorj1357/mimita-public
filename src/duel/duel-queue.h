// 08 10 2026, 14 34
/* purpose
* Owns the client-side duels queue: join/poll the coordinator matchmaker,
* drive the sandbox-practice wait, connect to a matched opponent, and present
* the live duel state to the HUD.
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
    Queuing,        // sandbox practice while waiting for a match
    MatchFound,     // "match found!!!!!!!!" banner; about to connect
    HostLaunching,  // host: launching the duel server, waiting for its room code
    Connecting,     // ICE connection to the opponent's server in progress
    InDuel,         // actively fighting
    MatchEnd,       // win/lose screen + rematch window
    Failed          // transient failure; auto re-queues
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
    // Leave the queue entirely and return to the main menu.
    void stopQueue();
    // Leave the current match and re-enter the queue (sandbox practice again).
    void returnToQueue();

    // Main-thread per-frame update.
    void update(float dt);

    // Server -> client packet handlers (called from mpTick).
    void onDuelState(const MimitaNet::DuelStatePacket& pkt);
    void onDuelEnemySpawn(const MimitaNet::DuelEnemySpawnPacket& pkt);

    // Host flow: room code read from the launched server's room file.
    void onHostRoomCodeReady(const std::string& roomCode);

    // World/NPC sandbox handoff flags consumed by engine-tick-state.
    bool wantsSandbox() const { return mWantsSandbox; }
    void clearSandboxFlag() { mWantsSandbox = false; }
    bool wantsMatchMap() const { return mWantsMatchMap; }
    void clearMatchMapFlag() { mWantsMatchMap = false; }
    std::string matchMapName() const { return mMapName; }

    // ── HUD accessors ────────────────────────────────────────────────
    bool isActive() const { return mState != DuelQueueState::Idle; }
    bool isHostLaunching() const { return mState == DuelQueueState::HostLaunching; }
    DuelQueueState state() const { return mState; }
    float queueElapsed() const { return mQueueElapsed; }
    std::string statusText() const { return mStatusText; }
    std::string opponentName() const { return mOpponentName; }
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
    std::string lastOpponentId() const { return mLastOpponentId; }

private:
    DuelQueue() = default;
    DuelQueue(const DuelQueue&) = delete;
    DuelQueue& operator=(const DuelQueue&) = delete;

    void pollThread();
    void beginPolling();
    void endPolling();
    void handleHostMatch();
    void handleClientMatch();
    void loadMatchMap();
    void enterSandbox();
    void recordHistoryOnce();
    void failToQueue(const char* reason);

    // Identity / queue
    std::string mProfileId;
    std::string mName;
    std::string mPreferOpponent;
    std::vector<std::string> mMaps;

    // Shared with the poll thread (mutex guarded)
    std::mutex mSharedMutex;
    std::string mTicketId;
    std::string mMatchId;
    std::string mRoomCode;
    bool mPendingHost = false;
    bool mPendingClient = false;
    bool mCancelled = false;
    bool mPolling = false;

    // Main-thread state
    DuelQueueState mState = DuelQueueState::Idle;
    float mQueueElapsed = 0.0f;
    std::string mStatusText;
    std::string mOpponentName;
    std::string mMapName;
    bool mMatchFoundBanner = false;
    float mMatchFoundTimer = 0.0f;
    uint64_t mPhaseStartMs = 0;
    uint64_t mLastStateMs = 0;
    float mFailedTimer = 0.0f;
    bool mHost = false;

    // Sandbox / map handoff
    bool mWantsSandbox = false;
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
