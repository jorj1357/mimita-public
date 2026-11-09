// 08 08 2026, 12 00
/* purpose
* Implements the pure client connection-health state machine.
* Maps packet freshness + reconnect grace window to the honest ConnectionState
* that drives UI. Side-effect free by design for unit testing.
* Does NOT tear down sessions, send reconnect packets, or render anything.
* Does NOT know about the server's peer disconnect lifecycle.
* Does NOT reference the NotificationSystem, sockets, or transports.
*/

#include "network/connection-health.h"

namespace MimitaNet {

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
    uint64_t graceMs)
{
    (void)graceMs;

    switch (current)
    {
    case ConnectionState::Connected:
    case ConnectionState::WeakConnection:
    case ConnectionState::WaitingForServer:
        // A snapshot gap is not proof that the transport is dead. Keep the
        // session and wait for the server until the hard timeout; reconnecting
        // immediately was causing a loop while the server was still working.
        if (snapshotTimedOut || lastHeardAge > hardTimeoutMs)
            return ConnectionState::Reconnecting;
        if (snapshotStarved)
            return ConnectionState::WaitingForServer;
        if (lastHeardAge > staleThresholdMs)
            return ConnectionState::WeakConnection;
        return ConnectionState::Connected;

    case ConnectionState::Reconnecting:
        // Do not treat a reconnect response or unrelated packet as recovered
        // gameplay state. Recovery requires a fresh authoritative snapshot;
        // otherwise the stale-snapshot check immediately starts the same
        // reconnect again.
        if (heardSinceDisconnect && !snapshotStarved)
            return ConnectionState::Connected;
        // Give up once the 60-second grace window expires.
        if (graceDeadline != 0 && now >= graceDeadline)
            return ConnectionState::ReconnectFailed;
        return ConnectionState::Reconnecting;

    default:
        // Terminal and pre-connect states are not moved by the health machine.
        return current;
    }
}

} // namespace MimitaNet
