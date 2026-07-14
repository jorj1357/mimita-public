#pragma once

#include <string>
#include <functional>

namespace MimitaNet {

struct CoordinatorRoomInfo
{
    std::string code;
    std::string joinToken;
};

struct CoordinatorLookupResult
{
    bool exists = false;
    bool reachable = false;
    std::string serverName;
    std::string map;
    std::string gamemode;
    int players = 0;
    int maxPlayers = 0;
    bool passwordProtected = false;
    std::string status;
};

struct CoordinatorJoinResult
{
    bool ok = false;
    std::string joinToken;
    std::string serverIp;
    uint16_t serverPort = 0;
    std::string serverName;
};

// ── ICE test signaling ─────────────────────────────────────────────

struct IceHostResult {
    bool ok = false;
    std::string roomCode;
    std::string hostSessionId;
    std::string joinToken;
};

struct IceJoinResult {
    bool ok = false;
    std::string hostIceDescription;
    std::string clientSessionId;
    std::string joinToken;
};

struct IcePollResult {
    bool ok = false;
    std::string status; // "waiting_client" or "client_ready"
    std::string clientIceDescription;
    std::string clientSessionId;
};

IceHostResult coordinatorIceHost(const std::string& hostSessionId, const std::string& iceDescription);
IceJoinResult coordinatorIceJoin(const std::string& roomCode, const std::string& clientSessionId, const std::string& iceDescription);
IcePollResult coordinatorIcePoll(const std::string& roomCode, const std::string& hostSessionId);
bool coordinatorIceValidateJoin(const std::string& roomCode, const std::string& joinToken);
void coordinatorIceDone(const std::string& roomCode);

// Set coordinator base URL (default: http://107.191.48.226:3001)
void setCoordinatorUrl(const std::string& url);
const std::string& getCoordinatorUrl();

// Server: register with coordinator → get room code + join token
CoordinatorRoomInfo coordinatorRegister(
    const std::string& hostSessionId,
    const std::string& publicIp,
    uint16_t port,
    const std::string& serverName,
    const std::string& mapName,
    const std::string& gamemode,
    int maxPlayers);

// Server: heartbeat (keeps room alive)
bool coordinatorHeartbeat(const std::string& code, int playerCount);

// Server: deregister
bool coordinatorLeave(const std::string& code);

// Client: look up a room code → get server info
CoordinatorLookupResult coordinatorLookup(const std::string& code);

// Client: request to join a room → get join token + server address
CoordinatorJoinResult coordinatorJoin(const std::string& code, const std::string& playerName);

// Server: validate a join token
bool coordinatorValidateJoin(const std::string& code, const std::string& joinToken);

bool coordinatorHttpGet(const std::string& url, std::string& response, int timeoutMs = 5000);

// ── Async coordinator lookup (non-blocking, uses background thread) ──
struct AsyncLookupResult {
    bool pending = false;
    bool done = false;
    bool failed = false;
    std::string code;
    CoordinatorLookupResult result;
};

// Start an async lookup. Returns immediately. Check done/failed on subsequent frames.
void coordinatorLookupAsync(AsyncLookupResult& state, const std::string& code);

// ── Async coordinator join (non-blocking) ────────────────────────────
struct AsyncJoinResult {
    bool pending = false;
    bool done = false;
    bool failed = false;
    std::string code;
    std::string playerName;
    CoordinatorJoinResult result;
};

void coordinatorJoinAsync(AsyncJoinResult& state, const std::string& code, const std::string& playerName);

} // namespace MimitaNet
