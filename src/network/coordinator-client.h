#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace MimitaNet {

// ── Room registration ────────────────────────────────────────────────
struct CoordinatorRoomInfo
{
    std::string code;
    std::string joinToken;
};

// ── Room lookup (non-mutating) ───────────────────────────────────────
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
    bool isIce = false;       // room was created via ice/host
};

// ── Normal join ──────────────────────────────────────────────────────
struct CoordinatorJoinResult
{
    bool ok = false;
    std::string joinToken;
    std::string serverIp;
    uint16_t serverPort = 0;
    std::string serverName;
};

// ── ICE: host registration ───────────────────────────────────────────
struct IceHostResult {
    bool ok = false;
    std::string roomCode;
    std::string hostSessionId;
    std::string joinToken;
};

// ── ICE: client begins join (sends real SDP, gets requestId) ─────────
struct IceBeginJoinResult {
    bool ok = false;
    std::string errorCode;    // "room-not-found", "invalid-sdp", etc.
    std::string requestId;
    std::string joinToken;
    std::string hostIceDescription; // empty initially; filled if host answered already
};

// ── ICE: host polls for pending requests ─────────────────────────────
struct IceHostPendingRequest {
    bool hasRequest = false;
    std::string requestId;
    std::string clientSessionId;
    std::string clientIceDescription;
};

// ── ICE: host posts answer for a request ─────────────────────────────
struct IceHostAnswerResult {
    bool ok = false;
};

// ── ICE: client polls for host answer ────────────────────────────────
struct IceClientPollResult {
    bool ok = false;
    std::string status;       // "pending", "answered", "failed", "expired"
    std::string hostIceDescription;
    std::string errorCode;
};

// ── ICE: request complete notification ───────────────────────────────
struct IceRequestCompleteResult {
    bool ok = false;
};

// ── TURN temporary credentials ───────────────────────────────────────
struct TurnCredentials {
    bool ok = false;
    std::string host;
    uint16_t port = 3478;
    std::string username;
    std::string credential;
    uint32_t expiresAt = 0;   // unix timestamp
};

// ── API functions ────────────────────────────────────────────────────

// Set coordinator base URL (default: http://107.191.48.226:3001)
void setCoordinatorUrl(const std::string& url);
const std::string& getCoordinatorUrl();

// ── ICE signaling (two-phase offer/answer) ───────────────────────────

// Host: register an ICE room
IceHostResult coordinatorIceHost(const std::string& hostSessionId, const std::string& iceDescription);
IceHostResult coordinatorIceHostPeer(const std::string& roomCode, const std::string& hostSessionId, const std::string& iceDescription);

// Legacy ICE API (deprecated, kept for test compatibility)
struct IceJoinResult {
    bool ok = false;
    std::string hostIceDescription;
    std::string clientSessionId;
    std::string joinToken;
};
struct IcePollResult {
    bool ok = false;
    std::string status;
    std::string clientIceDescription;
    std::string clientSessionId;
};
IceJoinResult coordinatorIceJoin(const std::string& roomCode, const std::string& clientSessionId, const std::string& iceDescription);
IcePollResult coordinatorIcePoll(const std::string& roomCode, const std::string& hostSessionId);

// Client: non-mutating lookup — checks if room exists AND is ICE type
CoordinatorLookupResult coordinatorIceLookup(const std::string& roomCode);

// Client: begin join — sends real SDP, gets requestId + one-time token
IceBeginJoinResult coordinatorIceBeginJoin(const std::string& roomCode,
    const std::string& clientSessionId, const std::string& iceDescription);

// Host: poll for pending connection requests
IceHostPendingRequest coordinatorIceHostPoll(const std::string& roomCode,
    const std::string& hostSessionId);

// Host: post answer SDP for a specific request
IceHostAnswerResult coordinatorIceHostAnswer(const std::string& roomCode,
    const std::string& hostSessionId, const std::string& requestId,
    const std::string& hostPeerSdp);

// Client: poll for host's answer
IceClientPollResult coordinatorIceClientPoll(const std::string& roomCode,
    const std::string& requestId);

// Host/client: mark request complete
IceRequestCompleteResult coordinatorIceRequestComplete(const std::string& roomCode,
    const std::string& requestId);

// Validate ICE join token (server-side)
bool coordinatorIceValidateJoin(const std::string& roomCode, const std::string& joinToken);

// Close room
void coordinatorIceDone(const std::string& roomCode);

// ── TURN credentials ─────────────────────────────────────────────────
TurnCredentials coordinatorRequestTurnCredentials();

// ── Standard room operations ─────────────────────────────────────────
CoordinatorRoomInfo coordinatorRegister(
    const std::string& hostSessionId,
    const std::string& publicIp,
    uint16_t port,
    const std::string& serverName,
    const std::string& mapName,
    const std::string& gamemode,
    int maxPlayers);

bool coordinatorHeartbeat(const std::string& code, int playerCount);
bool coordinatorLeave(const std::string& code);
CoordinatorLookupResult coordinatorLookup(const std::string& code);
CoordinatorJoinResult coordinatorJoin(const std::string& code, const std::string& playerName);
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
void coordinatorLookupAsync(AsyncLookupResult& state, const std::string& code);

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
