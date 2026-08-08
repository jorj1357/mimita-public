// 08 08 2026, 12 00
/* purpose
* Declares the pure client connection-health state machine used to drive honest
* UI. Decides the next ConnectionState from packet freshness and the reconnect
* grace window, with NO side effects (no teardown, no reconnect, no UI), so it
* is unit-testable without a full game object graph.
* Does NOT send packets, close sockets, push notifications, or touch rendering.
* Does NOT own server-side disconnect policy or peer disconnect effects.
* Does NOT modify MultiplayerContext; callers apply the returned state.
*/

#pragma once

#include "network/multiplayer-context.h"

namespace MimitaNet {

// Pure next-state decision for the client connection-health machine.
//
//   current          — the ConnectionState before this evaluation.
//   now              — current wall-clock ms.
//   lastHeardAge     — ms since the last packet was heard from the server.
//   heardSinceDisconnect — true when lastHeardServerMs > disconnectStartedMs
//                       (i.e. a packet arrived after reconnect started).
//   staleThresholdMs — last packet age at which Connected becomes WeakConnection.
//   hardTimeoutMs    — last packet age at which the session enters Reconnecting.
//   graceDeadline    — wall-clock deadline of the reconnect grace window; 0 = none.
//   graceMs          — length of the reconnect grace window (for tests; the
//                       deadline is produced by the caller, not computed here).
//
// Returns the state the machine should move to. Connected/WeakConnection is a
// single supervised pair; Reconnecting self-heals to Connected as soon as a
// fresh packet arrives, and gives up (ReconnectFailed) when the grace window
// expires. Terminal states (Disconnected, ReconnectFailed, HostClosed, Kicked,
// ServerCrashed, Connecting/... pre-connect states) are left untouched.
ConnectionState mpNextConnectionHealth(
    ConnectionState current,
    uint64_t now,
    uint64_t lastHeardAge,
    bool heardSinceDisconnect,
    uint64_t staleThresholdMs,
    uint64_t hardTimeoutMs,
    uint64_t graceDeadline,
    uint64_t graceMs);

} // namespace MimitaNet
