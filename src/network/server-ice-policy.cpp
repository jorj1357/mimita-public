#include "network/server-ice-policy.h"

#include "config/networking-config.h"

namespace MimitaNet {

void serverIcePolicyDecide(mimita::host::ServerIcePolicyV1& policy) {
    const auto& config = NetworkingConfig::instance().data().icePolicy;
    policy.gatherTimeoutMs =
        policy.stage == mimita::host::kIcePolicyListenerConfig
            ? config.listenerGatherTimeoutMs
            : config.peerGatherTimeoutMs;
    policy.connectTimeoutMs = config.peerConnectTimeoutMs;
    policy.pollIntervalMs = config.coordinatorPollIntervalMs;
    policy.maxPendingPeers = config.maxPendingPeers;
    policy.maxPacketsPerPoll = config.maxPacketsPerPoll;
    policy.acceptPeer = 1;
    policy.expirePeer = 0;
    policy.relayOnly = policy.forceRelay ? 1u : 0u;

    switch (policy.stage) {
    case mimita::host::kIcePolicyCoordinatorPoll:
        policy.acceptPeer =
            policy.pendingPeers < config.maxPendingPeers ? 1u : 0u;
        break;
    case mimita::host::kIcePolicyPeerGather:
        policy.expirePeer =
            policy.elapsedMs > config.peerGatherTimeoutMs ? 1u : 0u;
        break;
    case mimita::host::kIcePolicyPeerConnect:
        policy.expirePeer =
            policy.elapsedMs > config.peerConnectTimeoutMs ? 1u : 0u;
        break;
    default:
        break;
    }
}

}  // namespace MimitaNet
