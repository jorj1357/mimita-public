// 08 08 2026, 12 00
/* purpose
* Declares the client connection state enum and per-remote-player disconnect
* state, kept header-light so pure state-machine logic and unit tests can use
* it without pulling in the full MultiplayerContext / player object graph.
* Does NOT own session teardown, reconnect sends, notifications, or rendering.
* Does NOT depend on sockets, transports, glm, or entity headers.
*/

#pragma once

#include <cstdint>

namespace MimitaNet {

// Honest client-side connection health. `WeakConnection` means game packets
// are stale but the session is still usable (input still flows). `Reconnecting`
// means reconnect attempts are running inside the grace window. The terminal
// states (ReconnectFailed / HostClosed / Kicked / ServerCrashed) describe why
// the session ended. UI derives "what the player should believe" from these.
enum class ConnectionState : uint8_t
{
    Disconnected,
    ResolvingCode,
    RequestingJoin,
    WaitJoinAccept,
    NatNegotiating,
    Connecting,
    Connected,
    Reconnecting,
    DisconnectPending,
    // ── Connection-health states (honest UI) ──────────────────────────
    WeakConnection,     // packets stale but session still alive; input still sent
    ReconnectFailed,    // 60s grace expired, gave up (fully disconnected)
    HostClosed,         // server/host explicitly closed the session
    Kicked,             // server kicked this player
    ServerCrashed       // server unreachable / process died
};

// ── Per-remote-player disconnect state (observers see red effects) ────
struct RemoteConnectionState
{
    uint64_t disconnectedSinceMs = 0; // 0 = not disconnected
    bool reconnectedNotified = false; // green effect already shown for this episode
};

const char* connectionStateName(ConnectionState state);

} // namespace MimitaNet
