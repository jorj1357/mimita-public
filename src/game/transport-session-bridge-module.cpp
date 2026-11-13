// 2026-11-13
// Game-DLL side of the resident listen-server session.

#include "game/transport-session-bridge-module.h"

#include <cstdint>
#include <string>

#include "host/abi/transport-session-capability.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "network/net_common.h"
#include "network/server.h"

namespace mimita::game {
namespace {
const mimita::host::TransportSessionCapabilityV1* gSession = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostTransportSession(const mimita::host::HostServicesV1* services) {
    gSession = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::TransportSessionCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::TransportSession),
            mimita::host::kTransportSessionCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kTransportSessionCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::TransportSessionCapabilityV1)) {
        gSession = capability;
    }
}

MimitaNet::ListenServerState* hostListenServerState() {
    if (!gSession || !gSession->server_state) return nullptr;
    return static_cast<MimitaNet::ListenServerState*>(
        gSession->server_state(gHostData));
}
}  // namespace mimita::game

namespace MimitaNet {

ListenServerState::~ListenServerState() = default;

bool startListenServer(ListenServerState& state, uint16_t port,
                       const std::string& publicIp,
                       const std::string& hostSessionId,
                       const ServerLaunchSettings* settings) {
    if (!mimita::game::gSession || !mimita::game::gSession->server_start) return false;
    return mimita::game::gSession->server_start(
               mimita::game::gHostData, &state, port, publicIp.c_str(),
               hostSessionId.c_str(), settings) != 0;
}

void stopListenServer(ListenServerState& state) {
    if (mimita::game::gSession && mimita::game::gSession->server_stop)
        mimita::game::gSession->server_stop(mimita::game::gHostData, &state);
}

void tickListenServer(ListenServerState& state, float dt) {
    if (mimita::game::gSession && mimita::game::gSession->server_tick)
        mimita::game::gSession->server_tick(mimita::game::gHostData, &state, dt);
}

bool initServerIceListener(ListenServerState& state) {
    return mimita::game::gSession && mimita::game::gSession->server_init_ice &&
           mimita::game::gSession->server_init_ice(
               mimita::game::gHostData, &state) != 0;
}

uint32_t serverHostPlayerId() {
    return mimita::game::gSession && mimita::game::gSession->server_host_player_id
        ? mimita::game::gSession->server_host_player_id(mimita::game::gHostData)
        : 0;
}

void setServerHostPlayerId(uint32_t playerId) {
    if (mimita::game::gSession && mimita::game::gSession->server_set_host_player_id)
        mimita::game::gSession->server_set_host_player_id(
            mimita::game::gHostData, playerId);
}

bool setNonBlocking(SOCKET socketHandle) {
    return mimita::game::gSession && mimita::game::gSession->set_nonblocking &&
           mimita::game::gSession->set_nonblocking(
               mimita::game::gHostData,
               static_cast<std::uint64_t>(socketHandle)) != 0;
}

bool disableUdpConnReset(SOCKET socketHandle) {
    return mimita::game::gSession && mimita::game::gSession->disable_udp_conn_reset &&
           mimita::game::gSession->disable_udp_conn_reset(
               mimita::game::gHostData,
               static_cast<std::uint64_t>(socketHandle)) != 0;
}

}  // namespace MimitaNet
