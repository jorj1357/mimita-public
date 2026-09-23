// 09 23 2026
/* purpose
* LEGACY / STABLE-KERNEL: thin libjuice transport wrapper. This is deliberately
* COLD and is never hot-reloaded: libjuice runs its own thread and its callbacks
* must never enter a replaceable module. The one safety contract for hot reload
* is the generation-safe callback token below: callbacks are invalidated and
* their queued events drained before any module swap or transport recreation, so
* a late libjuice callback can only mutate cold state or be dropped.
* Does NOT own ICE policy, signaling, packet schemas, or gameplay.
*/
#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <functional>
#include "network/ice/ice-types.h"
#include "network/ice/ice-config.h"

#include <juice/juice.h>

enum class IceEventType
{
    StateChanged,
    Candidate,
    GatheringDone,
    Recv
};

struct IceEvent
{
    IceEventType type;

    // For Recv events
    std::vector<char> data;

    // For StateChanged
    IceAgentState newState;
};

class IceAgent
{
public:
    IceAgent();
    ~IceAgent();

    bool initialize(const IceConfiguration& config);
    void shutdown();
    bool gatherCandidates();

    std::string localSdp() const;

    bool setRemoteDescription(const std::string& sdp);
    bool send(const void* data, size_t size);

    void tick();

    IceAgentState state() const;
    const std::vector<IceCandidateInfo>& candidates() const;

    // Log the selected candidate pair after connection
    void logSelectedPath();

    // Drain pending events (thread-safe, call from main thread)
    void pollEvents(std::vector<IceEvent>& out);

    // ── Hot-reload safety (legacy kernel contract) ──────────────────────
    // Invalidate callbacks for the current generation and drop any queued
    // events, so a module swap can never observe a callback that outlives its
    // generation. Bumps the callback token; a late libjuice callback still
    // queued is discarded. The libjuice object itself is never unloaded.
    void quiesceForReload();
    // Current generation-safe callback token (0 while invalidated).
    std::uint64_t callbackToken() const { return mCallbackToken.load(); }

private:
    static void onStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void onCandidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void onGatheringDone(juice_agent_t* agent, void* user_ptr);
    static void onRecv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

    void handleStateChanged(juice_state_t state);
    void handleCandidate(const char* sdp);
    void handleGatheringDone();
    void handleRecv(const char* data, size_t size);

    mutable std::mutex mMutex;
    juice_agent_t* mAgent = nullptr;
    IceAgentState mState = IceAgentState::Idle;
    std::vector<IceCandidateInfo> mCandidates;
    std::deque<IceEvent> mEvents;
    std::string mLocalSdp;
    int mHostCount = 0;
    int mSrflxCount = 0;
    int mRelayCount = 0;
    bool mInitialized = false;
    // Generation-safe callback token. Callbacks capture the token at entry and
    // drop if it changed; quiesceForReload bumps/zeroes it before a swap.
    std::atomic<std::uint64_t> mCallbackToken{1};
};
