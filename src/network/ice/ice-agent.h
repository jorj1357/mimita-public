#pragma once

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

    // Drain pending events (thread-safe, call from main thread)
    void pollEvents(std::vector<IceEvent>& out);

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
};
