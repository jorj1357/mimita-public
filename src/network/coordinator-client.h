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

// Async check to populate room code (non-blocking string response)
bool coordinatorHttpGet(const std::string& url, std::string& response, int timeoutMs = 5000);

} // namespace MimitaNet
