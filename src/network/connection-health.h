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

#include "network/connection-state.h"

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
//   snapshotStarved  — true when authoritative snapshots stopped despite a
//                      previously received snapshot.
//   snapshotTimedOut — true when snapshot staleness reached the hard timeout.
//   graceDeadline    — wall-clock deadline of the reconnect grace window; 0 = none.
//   graceMs          — length of the reconnect grace window (for tests; the
//                       deadline is produced by the caller, not computed here).
//
// Returns the state the machine should move to. Snapshot starvation becomes
// WaitingForServer until the hard timeout, then Reconnecting. Connected stays
// connected while snapshots flow. Reconnecting self-heals to Connected as soon
// as a fresh authoritative snapshot arrives.
ConnectionState mpNextConnectionHealth(
    ConnectionState current,
    uint64_t now,
    uint64_t lastHeardAge,
    bool heardSinceDisconnect,
    uint64_t staleThresholdMs,
    uint64_t hardTimeoutMs,
    bool snapshotStarved,
    bool snapshotTimedOut,
    uint64_t graceDeadline,
    uint64_t graceMs);

} // namespace MimitaNet
