#pragma once

#include <string>
#include <vector>

enum class IceAgentState
{
    Idle,
    Gathering,
    GatheringComplete,
    Connecting,
    Connected,
    Completed,
    Failed,
    Closed
};

enum class IceCandidateType
{
    Host,
    ServerReflexive,
    Relay,
    Unknown
};

struct IceCandidateInfo
{
    IceCandidateType type = IceCandidateType::Unknown;
    std::string protocol;
    std::string address;
    uint16_t port = 0;
};

inline const char* iceCandidateTypeToString(IceCandidateType type)
{
    switch (type)
    {
    case IceCandidateType::Host:            return "host";
    case IceCandidateType::ServerReflexive:  return "srflx";
    case IceCandidateType::Relay:           return "relay";
    default:                                return "unknown";
    }
}

inline IceCandidateType iceCandidateTypeFromSdp(const std::string& sdp)
{
    if (sdp.find("typ host") != std::string::npos)
        return IceCandidateType::Host;
    if (sdp.find("typ srflx") != std::string::npos)
        return IceCandidateType::ServerReflexive;
    if (sdp.find("typ relay") != std::string::npos)
        return IceCandidateType::Relay;
    return IceCandidateType::Unknown;
}

inline void parseCandidateSdp(const std::string& sdp, IceCandidateInfo& info)
{
    // SDP format: candidate foundation component protocol priority address port type ...
    // Example: candidate 1 1 UDP 2130706432 192.168.1.1 53524 typ host
    info.type = iceCandidateTypeFromSdp(sdp);

    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = sdp.find(' ');
    while (end != std::string::npos)
    {
        parts.push_back(sdp.substr(start, end - start));
        start = end + 1;
        end = sdp.find(' ', start);
    }
    parts.push_back(sdp.substr(start));

    // parts[0] = "candidate"
    // parts[1] = foundation
    // parts[2] = component
    // parts[3] = protocol (UDP/TCP)
    // parts[4] = priority
    // parts[5] = address
    // parts[6] = port
    if (parts.size() >= 7)
    {
        info.protocol = parts[3];
        info.address = parts[5];
        info.port = static_cast<uint16_t>(std::stoul(parts[6]));
    }
}
