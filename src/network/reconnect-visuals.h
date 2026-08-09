// 08 08 2026, 12 00
/* purpose
* Owns the observer-facing disconnect/reconnect visuals for remote players.
* Shows a red pulsing beam + ticking "connection lost" label above a frozen
* body while a peer reconnects, and a green "reconnected" effect on recovery.
* Tracks per-remote-player disconnect state in MultiplayerContext.
* Does NOT manage the local player's own connection state or notifications.
* Does NOT run on the server, send packets, or mutate player simulation.
* Does NOT render nameplates/healthbars (uses the pooled EffectPart system).
*/

#pragma once

#include "network/multiplayer-context.h"

namespace MimitaNet {

// Server notified that a peer player's connection was lost. Marks the remote
// player disconnected and spawns the initial red effect. Call when a
// PACKET_PLAYER_CONNECTION_STATE (connected=0) arrives.
void mpNoteRemotePlayerDisconnected(MultiplayerContext& ctx, uint32_t playerId,
                                    uint64_t serverDisconnectedAtMs);

// Server notified that a peer player reconnected. Spawns a green effect and
// clears the disconnected indicator. Call when a PACKET_PLAYER_CONNECTION_STATE
// (connected=1) arrives.
void mpNoteRemotePlayerReconnected(MultiplayerContext& ctx, uint32_t playerId);

// Per-frame update: keeps red beams and the fast-ticking elapsed label alive
// above every disconnected remote player. Call from engineTickNet every frame.
void mpUpdateReconnectVisuals(MultiplayerContext& ctx, float dt);

// Clear any pending indicator for a remote player (missing-entity removal).
void mpClearRemoteReconnectVisual(MultiplayerContext& ctx, uint32_t playerId);

} // namespace MimitaNet
