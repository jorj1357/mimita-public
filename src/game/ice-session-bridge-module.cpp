// 2026-11-13
// Game-DLL-side proxy for the resident ICE agent.
//
// The game object carries only the host handle. Juice, sockets, and callback
// threads stay resident; events/SDP cross as bounded values for this call.

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/ice-capability.h"
#include "network/ice/ice-agent.h"

namespace mimita::game {
void initializeHostIce(const mimita::host::HostServicesV1* services);
namespace {
const mimita::host::IceCapabilityV1* gIce = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostIce(const mimita::host::HostServicesV1* services) {
    gIce = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::IceCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Ice),
            mimita::host::kIceCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kIceCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::IceCapabilityV1)) {
        gIce = capability;
    }
}
}  // namespace mimita::game

IceAgent::IceAgent() {
    if (mimita::game::gIce && mimita::game::gIce->create)
        mHostHandle = mimita::game::gIce->create(mimita::game::gHostData);
}

IceAgent::~IceAgent() {
    if (mimita::game::gIce && mimita::game::gIce->destroy && mHostHandle)
        mimita::game::gIce->destroy(mimita::game::gHostData, mHostHandle);
    mHostHandle = nullptr;
}

bool IceAgent::initialize(const IceConfiguration& config) {
    return mimita::game::gIce && mimita::game::gIce->initialize &&
           mimita::game::gIce->initialize(mimita::game::gHostData, mHostHandle, &config) != 0;
}

void IceAgent::shutdown() {
    if (mimita::game::gIce && mimita::game::gIce->shutdown)
        mimita::game::gIce->shutdown(mimita::game::gHostData, mHostHandle);
}

bool IceAgent::gatherCandidates() {
    return mimita::game::gIce && mimita::game::gIce->gather_candidates &&
           mimita::game::gIce->gather_candidates(mimita::game::gHostData, mHostHandle) != 0;
}

std::string IceAgent::localSdp() const {
    if (!mimita::game::gIce || !mimita::game::gIce->local_sdp) return {};
    std::array<char, 8192> buffer{};
    if (mimita::game::gIce->local_sdp(mimita::game::gHostData, mHostHandle,
                                     buffer.data(),
                                     static_cast<std::uint32_t>(buffer.size())) == 0) {
        return {};
    }
    return std::string(buffer.data());
}

bool IceAgent::setRemoteDescription(const std::string& sdp) {
    return mimita::game::gIce && mimita::game::gIce->set_remote_description &&
           mimita::game::gIce->set_remote_description(
               mimita::game::gHostData, mHostHandle, sdp.c_str()) != 0;
}

bool IceAgent::send(const void* data, size_t size) {
    if (!data || size > 0xFFFFFFFFull || !mimita::game::gIce || !mimita::game::gIce->send)
        return false;
    return mimita::game::gIce->send(mimita::game::gHostData, mHostHandle, data,
                                    static_cast<std::uint32_t>(size)) != 0;
}

void IceAgent::tick() {
    if (mimita::game::gIce && mimita::game::gIce->tick)
        mimita::game::gIce->tick(mimita::game::gHostData, mHostHandle);
}

IceAgentState IceAgent::state() const {
    if (!mimita::game::gIce || !mimita::game::gIce->state) return IceAgentState::Idle;
    return static_cast<IceAgentState>(
        mimita::game::gIce->state(mimita::game::gHostData, mHostHandle));
}

void IceAgent::pollEvents(std::vector<IceEvent>& out) {
    out.clear();
    if (!mimita::game::gIce || !mimita::game::gIce->poll_events) return;
    std::array<mimita::host::IceEventV1, 256> events{};
    std::uint32_t count = 0;
    if (mimita::game::gIce->poll_events(
            mimita::game::gHostData, mHostHandle, events.data(),
            static_cast<std::uint32_t>(events.size()), &count) == 0) {
        return;
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        IceEvent event{};
        event.type = static_cast<IceEventType>(events[i].kind);
        event.newState = static_cast<IceAgentState>(events[i].newState);
        if (events[i].data && events[i].dataSize) {
            const auto* begin = reinterpret_cast<const char*>(events[i].data);
            event.data.assign(begin, begin + events[i].dataSize);
        }
        out.push_back(std::move(event));
    }
}

void IceAgent::logSelectedPath() {
    if (mimita::game::gIce && mimita::game::gIce->log_selected_path)
        mimita::game::gIce->log_selected_path(mimita::game::gHostData, mHostHandle);
}
