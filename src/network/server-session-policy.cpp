// 2026-11-17
// Hot room/session publication policy decisions.
//
// The resident pump owns bind/listen/thread/socket mechanics and the room-file
// stream. The game generation decides local-only, coordinator registration,
// publication, and duplicate-room policy. Editing this file activates in the
// same PID through a game-generation swap.
#include "network/server-session-policy.h"

namespace MimitaNet {

void serverSessionPolicyDecide(mimita::host::ServerSessionPolicyV1& policy) {
    policy.localOnly = policy.startLocalServer ? 1u : 0u;
    // A local-only room never reaches the coordinator. An externally launched
    // server process owns its own registration.
    policy.registerWithCoordinator =
        (!policy.localOnly && !policy.externalProcessLaunched) ? 1u : 0u;
    policy.publishRoom =
        (policy.localOnly || policy.registerWithCoordinator) ? 1u : 0u;
    policy.writeRoomFile = policy.registerWithCoordinator ? 1u : 0u;
    // A duplicate room on the same process is a bug, not an admitted state.
    policy.allowDuplicateRoom = 0;
}

}  // namespace MimitaNet
