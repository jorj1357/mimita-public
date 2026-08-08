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
    uint64_t graceDeadline,
    uint64_t graceMs)
{
    (void)graceMs;

    switch (current)
    {
    case ConnectionState::Connected:
    case ConnectionState::WeakConnection:
        // The session is supervised by packet freshness alone. A hard timeout
        // beats a stale warning: the client enters the reconnect grace window.
        if (lastHeardAge > hardTimeoutMs)
            return ConnectionState::Reconnecting;
        if (lastHeardAge > staleThresholdMs)
            return ConnectionState::WeakConnection;
        return ConnectionState::Connected;

    case ConnectionState::Reconnecting:
        // Any packet received after reconnect started proves the server session
        // is alive again. ReconnectAccept restores full state; even a plain
        // snapshot means the slot survived and we can resume.
        if (heardSinceDisconnect)
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
