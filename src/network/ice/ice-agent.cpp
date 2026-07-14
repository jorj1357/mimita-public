#include "network/ice/ice-agent.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <cstring>

IceAgent::IceAgent() = default;

IceAgent::~IceAgent()
{
    shutdown();
}

bool IceAgent::initialize(const IceConfiguration& config)
{
    if (mInitialized) shutdown();

    Debug::warn(Debug::Category::Networking, "ICE INIT libjuiceVersion=%s\n", "1.7.2");
    Debug::warn(Debug::Category::Networking, "ICE INIT stunHost=%s stunPort=%u\n",
           config.stun.host.c_str(), config.stun.port);

    juice_turn_server_t turnServer;
    bool turnConfigured = !config.turn.password.empty();

    if (turnConfigured)
    {
        Debug::warn(Debug::Category::Networking, "ICE INIT turnConfigured=1 turnHost=%s turnPort=%u\n",
               config.turn.host.c_str(), config.turn.port);

        turnServer.host = config.turn.host.c_str();
        turnServer.port = config.turn.port;
        turnServer.username = config.turn.username.c_str();
        turnServer.password = config.turn.password.c_str();
    }

    juice_config_t juiceCfg = {};
    juiceCfg.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;

    juiceCfg.stun_server_host = config.stun.host.c_str();
    juiceCfg.stun_server_port = config.stun.port;

    if (turnConfigured)
    {
        juiceCfg.turn_servers = &turnServer;
        juiceCfg.turn_servers_count = 1;
    }

    juiceCfg.cb_state_changed = onStateChanged;
    juiceCfg.cb_candidate = onCandidate;
    juiceCfg.cb_gathering_done = onGatheringDone;
    juiceCfg.cb_recv = onRecv;
    juiceCfg.user_ptr = this;

    mAgent = juice_create(&juiceCfg);
    if (!mAgent)
    {
        Debug::warn(Debug::Category::Networking, "ICE INIT failed: juice_create returned null\n");
        return false;
    }

    mInitialized = true;
    mState = IceAgentState::Idle;
    mCandidates.clear();
    mEvents.clear();
    mHostCount = 0;
    mSrflxCount = 0;
    mRelayCount = 0;
    mLocalSdp.clear();

    Debug::log(Debug::Category::Networking, "ICE INIT success\n");
    return true;
}

bool IceAgent::gatherCandidates()
{
    if (!mAgent || !mInitialized) return false;

    mState = IceAgentState::Gathering;
    int ret = juice_gather_candidates(mAgent);
    if (ret != JUICE_ERR_SUCCESS)
    {
        Debug::warn(Debug::Category::Networking, "ICE GATHER FAILED: juice_gather_candidates returned %d\n", ret);
        return false;
    }

    Debug::log(Debug::Category::Networking, "ICE GATHER started\n");
    return true;
}

std::string IceAgent::localSdp() const
{
    return mLocalSdp;
}

bool IceAgent::setRemoteDescription(const std::string& sdp)
{
    if (!mAgent || !mInitialized) return false;

    int ret = juice_set_remote_description(mAgent, sdp.c_str());
    if (ret != JUICE_ERR_SUCCESS)
    {
        Debug::warn(Debug::Category::Networking, "ICE SET REMOTE DESC failed: %d\n", ret);
        return false;
    }

    Debug::log(Debug::Category::Networking, "ICE SET REMOTE DESC (bytes=%zu)\n", sdp.size());
    return true;
}

bool IceAgent::send(const void* data, size_t size)
{
    if (!mAgent || !mInitialized) return false;

    int ret = juice_send(mAgent, static_cast<const char*>(data), size);
    if (ret != JUICE_ERR_SUCCESS)
    {
        Debug::warn(Debug::Category::Networking, "ICE SEND failed: %d\n", ret);
        return false;
    }

    Debug::log(Debug::Category::Networking, "ICE SEND bytes=%zu\n", size);
    return true;
}

void IceAgent::logSelectedPath()
{
    if (!mAgent) return;
    char localBuf[JUICE_MAX_CANDIDATE_SDP_STRING_LEN] = {};
    char remoteBuf[JUICE_MAX_CANDIDATE_SDP_STRING_LEN] = {};
    int ret = juice_get_selected_candidates(mAgent, localBuf, sizeof(localBuf),
                                              remoteBuf, sizeof(remoteBuf));
    if (ret != JUICE_ERR_SUCCESS) return;

    std::string local(localBuf);
    std::string remote(remoteBuf);

    auto localType = iceCandidateTypeFromSdp(local);
    auto remoteType = iceCandidateTypeFromSdp(remote);

    bool direct = (localType == IceCandidateType::Host || localType == IceCandidateType::ServerReflexive) &&
                  (remoteType == IceCandidateType::Host || remoteType == IceCandidateType::ServerReflexive);
    bool relay = (localType == IceCandidateType::Relay || remoteType == IceCandidateType::Relay);

    Debug::warn(Debug::Category::Networking,
           "ICE SELECTED PATH localCandidate=%s localType=%s remoteCandidate=%s remoteType=%s direct=%d relay=%d\n",
           local.c_str(), iceCandidateTypeToString(localType),
           remote.c_str(), iceCandidateTypeToString(remoteType),
           (int)direct, (int)relay);
}

void IceAgent::shutdown()
{
    if (mAgent)
    {
        juice_destroy(mAgent);
        mAgent = nullptr;
    }
    mState = IceAgentState::Closed;
    mInitialized = false;

    std::lock_guard<std::mutex> lock(mMutex);
    mCandidates.clear();
    mEvents.clear();
}

void IceAgent::tick()
{
}

IceAgentState IceAgent::state() const
{
    return mState;
}

const std::vector<IceCandidateInfo>& IceAgent::candidates() const
{
    return mCandidates;
}

void IceAgent::pollEvents(std::vector<IceEvent>& out)
{
    std::lock_guard<std::mutex> lock(mMutex);
    out.clear();
    out.reserve(mEvents.size());
    for (auto& ev : mEvents)
        out.push_back(std::move(ev));
    mEvents.clear();
}

void IceAgent::onStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr)
{
    auto* self = static_cast<IceAgent*>(user_ptr);
    if (self) self->handleStateChanged(state);
}

void IceAgent::onCandidate(juice_agent_t* agent, const char* sdp, void* user_ptr)
{
    auto* self = static_cast<IceAgent*>(user_ptr);
    if (self) self->handleCandidate(sdp);
}

void IceAgent::onGatheringDone(juice_agent_t* agent, void* user_ptr)
{
    auto* self = static_cast<IceAgent*>(user_ptr);
    if (self) self->handleGatheringDone();
}

void IceAgent::onRecv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr)
{
    auto* self = static_cast<IceAgent*>(user_ptr);
    if (self) self->handleRecv(data, size);
}

void IceAgent::handleStateChanged(juice_state_t state)
{
    IceAgentState old = mState;

    IceEvent ev;
    ev.type = IceEventType::StateChanged;

    switch (state)
    {
    case JUICE_STATE_DISCONNECTED: mState = IceAgentState::Idle; break;
    case JUICE_STATE_GATHERING:    mState = IceAgentState::Gathering; break;
    case JUICE_STATE_CONNECTING:   mState = IceAgentState::Connecting; break;
    case JUICE_STATE_CONNECTED:    mState = IceAgentState::Connected; break;
    case JUICE_STATE_COMPLETED:    mState = IceAgentState::Completed; break;
    case JUICE_STATE_FAILED:       mState = IceAgentState::Failed; break;
    }

    ev.newState = mState;

    std::lock_guard<std::mutex> lock(mMutex);
    mEvents.push_back(ev);

    Debug::log(Debug::Category::Networking, "ICE STATE old=%s new=%s\n",
           juice_state_to_string(static_cast<juice_state_t>(old)),
           juice_state_to_string(state));
}

void IceAgent::handleCandidate(const char* sdp)
{
    if (!sdp) return;

    IceCandidateInfo info;
    std::string sdpStr(sdp);
    info.type = iceCandidateTypeFromSdp(sdpStr);

    std::vector<std::string> parts;
    size_t start = 0, end = sdpStr.find(' ');
    while (end != std::string::npos)
    {
        parts.push_back(sdpStr.substr(start, end - start));
        start = end + 1;
        end = sdpStr.find(' ', start);
    }
    parts.push_back(sdpStr.substr(start));

    int offset = 0;
    if (parts.size() >= 1 && parts[0] == "candidate")
        offset = 1;

    if (parts.size() >= (size_t)(6 + offset))
    {
        info.protocol = parts[2 + offset];
        info.address = parts[4 + offset];
        try {
            info.port = static_cast<uint16_t>(std::stoul(parts[5 + offset]));
        } catch (...) {
            info.port = 0;
        }
    }
    else
    {
        info.protocol = "?";
        info.address = sdpStr.substr(0, 64);
        info.port = 0;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCandidates.push_back(info);

        IceEvent ev;
        ev.type = IceEventType::Candidate;
        mEvents.push_back(ev);

        switch (info.type)
        {
        case IceCandidateType::Host:            mHostCount++; break;
        case IceCandidateType::ServerReflexive:  mSrflxCount++; break;
        case IceCandidateType::Relay:           mRelayCount++; break;
        default: break;
        }
    }

    Debug::log(Debug::Category::Networking, "ICE CANDIDATE type=%s protocol=%s address=%s port=%u\n",
           iceCandidateTypeToString(info.type),
           info.protocol.c_str(),
           info.address.c_str(),
           info.port);
}

void IceAgent::handleGatheringDone()
{
    mState = IceAgentState::GatheringComplete;

    char sdpBuf[JUICE_MAX_SDP_STRING_LEN] = {};
    if (juice_get_local_description(mAgent, sdpBuf, sizeof(sdpBuf)) == JUICE_ERR_SUCCESS)
    {
        mLocalSdp = sdpBuf;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        IceEvent ev;
        ev.type = IceEventType::GatheringDone;
        mEvents.push_back(ev);
    }

    Debug::warn(Debug::Category::Networking,
           "ICE GATHER COMPLETE hostCandidates=%d srflxCandidates=%d relayCandidates=%d\n",
           mHostCount, mSrflxCount, mRelayCount);
}

void IceAgent::handleRecv(const char* data, size_t size)
{
    if (!data || size == 0) return;

    printf("[ICE RECV CB] bytes=%zu data=%.*s\n", size, (int)(size > 64 ? 64 : size), data);
    fflush(stdout);

    IceEvent ev;
    ev.type = IceEventType::Recv;
    ev.data.assign(data, data + size);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEvents.push_back(ev);
    }

    Debug::warn(Debug::Category::Networking, "ICE RECV bytes=%zu\n", size);
}
