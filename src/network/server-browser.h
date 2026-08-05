// 08 05 2026, 09 00
/* purpose
* Owns the public server browser data model for the online menu.
* Fetches the coordinator /list and measures per-server UDP ping probes.
* Exposes a thread-safe snapshot for UI rendering without blocking the menu.
* Does NOT render UI, own the coordinator API, or manage ICE transports.
* Does NOT register rooms, join games, or mutate server state.
* Does NOT own socket lifecycle beyond short-lived probe sockets.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MimitaNet {

// Ping measurement for one server entry (reachable=false => unknown).
struct ServerBrowserPing
{
    bool reachable = false;
    uint32_t pingMs = 0;
};

// Snapshot entry shown in the server browser UI.
struct ServerBrowserEntry
{
    std::string code;
    std::string serverName;
    std::string hostPlayerName;
    std::string map;
    std::string gamemode;
    int players = 0;
    int maxPlayers = 0;
    bool passwordProtected = false;
    uint64_t uptimeSeconds = 0;
    std::string publicIp;
    int port = 0;
    ServerBrowserPing ping;
};

// One-time startup for the browser's probe sockets (WSA).
void serverBrowserInit();

// Call every frame while the browser is visible. Refreshes the list on a
// background thread at most once per REFRESH_INTERVAL_MS (or on request).
void serverBrowserTick();

// Force a refresh on the next tick (used when the menu opens).
void serverBrowserRequestRefresh();

// Tell the browser which room code the local process hosts, so its own
// server entry is pinged via 127.0.0.1 instead of the external IP.
void serverBrowserSetOwnRoomCode(const std::string& code);

// Thread-safe snapshot of the latest list (empty until first refresh).
std::vector<ServerBrowserEntry> serverBrowserEntries();

bool serverBrowserRefreshing();

} // namespace MimitaNet
