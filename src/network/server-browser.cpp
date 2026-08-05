// 08 05 2026, 09 05
/* purpose
* Implements the public server browser data model for the online menu.
* Fetches the coordinator /list and runs short UDP ping probes per server.
* Uses one background refresh thread so the menu never blocks on HTTP/UDP.
* Does NOT render UI, own the coordinator API, or manage ICE transports.
* Does NOT register rooms, join games, or mutate server state.
* Does NOT own socket lifecycle beyond short-lived probe sockets.
*/

#include "network/server-browser.h"
#include "network/net_common.h"
#include "network/packets.h"
#include "network/coordinator-client.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace MimitaNet {

namespace {

constexpr uint64_t REFRESH_INTERVAL_MS = 3000;
constexpr uint64_t PING_TIMEOUT_MS = 800;

std::vector<ServerBrowserEntry> gEntries;
std::mutex gMutex;
std::atomic<bool> gRefreshRunning{false};
uint64_t gLastRefreshMs = 0;
bool gRefreshRequested = false;
bool gWsaStarted = false;
std::string gOwnRoomCode;

// Probe one server with a raw PING packet; unreachable hosts time out.
ServerBrowserPing probeServerPing(const ServerListEntry& e)
{
    ServerBrowserPing out;
    std::string host = e.publicIp;
    if (host.empty() || e.port <= 0)
        return out;
    if (!gOwnRoomCode.empty() && gOwnRoomCode == e.code)
        host = "127.0.0.1";

    sockaddr_in addr{};
    if (!parseAddress(host + ":" + std::to_string(e.port), addr))
        return out;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
        return out;
    disableUdpConnReset(s);

    DWORD timeoutMs = (DWORD)PING_TIMEOUT_MS;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    PingPacket ping{};
    ping.header.magic = PROTOCOL_MAGIC;
    ping.header.version = PROTOCOL_VERSION;
    ping.header.type = PACKET_PING;
    ping.header.tick = 0;
    ping.header.playerId = 0;
    ping.clientTimeMs = nowMs();

    sendto(s, (const char*)&ping, (int)sizeof(ping), 0,
           (sockaddr*)&addr, sizeof(addr));

    char buf[256];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
    closesocket(s);

    if (n >= (int)sizeof(PingPacket))
    {
        const PingPacket* pong = reinterpret_cast<const PingPacket*>(buf);
        if (pong->header.magic == PROTOCOL_MAGIC &&
            pong->header.version == PROTOCOL_VERSION &&
            pong->header.type == PACKET_PING)
        {
            uint64_t rtt = nowMs() - pong->clientTimeMs;
            out.reachable = true;
            out.pingMs = (uint32_t)(rtt > 9999 ? 9999 : rtt);
        }
    }
    return out;
}

// Background worker: fetch the coordinator list and ping each server.
void refreshWorker()
{
    std::vector<ServerListEntry> raw = coordinatorServerList();
    std::vector<ServerBrowserEntry> fresh;
    fresh.reserve(raw.size());
    for (const ServerListEntry& e : raw)
    {
        ServerBrowserEntry be;
        be.code = e.code;
        be.serverName = e.serverName;
        be.hostPlayerName = e.hostPlayerName;
        be.map = e.map;
        be.gamemode = e.gamemode;
        be.players = e.players;
        be.maxPlayers = e.maxPlayers;
        be.passwordProtected = e.passwordProtected;
        be.uptimeSeconds = e.uptimeSeconds;
        be.publicIp = e.publicIp;
        be.port = e.port;
        be.ping = probeServerPing(e);
        fresh.push_back(std::move(be));
    }
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gEntries = std::move(fresh);
    }
    gRefreshRunning = false;
}

} // anonymous namespace

void serverBrowserInit()
{
    if (!gWsaStarted)
    {
        netStartup();
        gWsaStarted = true;
    }
}

void serverBrowserTick()
{
    serverBrowserInit();
    if (gRefreshRunning)
        return;
    uint64_t now = nowMs();
    bool due = (now - gLastRefreshMs >= REFRESH_INTERVAL_MS);
    if (!due && !gRefreshRequested)
        return;
    gLastRefreshMs = now;
    gRefreshRequested = false;
    gRefreshRunning = true;
    std::thread(refreshWorker).detach();
}

void serverBrowserRequestRefresh()
{
    gRefreshRequested = true;
}

void serverBrowserSetOwnRoomCode(const std::string& code)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gOwnRoomCode = code;
}

std::vector<ServerBrowserEntry> serverBrowserEntries()
{
    std::lock_guard<std::mutex> lock(gMutex);
    return gEntries;
}

bool serverBrowserRefreshing()
{
    return gRefreshRunning.load();
}

} // namespace MimitaNet
