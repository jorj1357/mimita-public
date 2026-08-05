// 07 19 2026, 10 45
/* purpose
* Owns HTTP communication with the MiMITA coordinator service.
* Provides room registration, lookup, ICE signaling, and token validation APIs.
* Applies the process-level coordinator URL override for automated tests.
* Does NOT own ICE candidate gathering, gameplay packet handling, or UI state.
* Does NOT implement server simulation or local harness assertions.
* Does NOT store persistent room, auth, or player state.
*/

#include "network/coordinator-client.h"
#include "network/net_common.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <winhttp.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace MimitaNet {

namespace {

std::string defaultCoordinatorUrl()
{
    const char* envUrl = std::getenv("MIMITA_COORDINATOR_URL");
    return envUrl && *envUrl ? std::string(envUrl) : "http://107.191.48.226:3001";
}

std::string gCoordinatorUrl = defaultCoordinatorUrl();

struct UrlParts {
    std::wstring host;
    int port;
    std::wstring path;
    bool secure;
};

bool parseCoordinatorUrl(const std::string& url, UrlParts& out)
{
    out.secure = url.compare(0, 8, "https://") == 0;
    size_t start = out.secure ? 8 : 7;
    size_t colon = url.find(':', start);
    size_t slash = url.find('/', start);
    if (colon != std::string::npos && colon < slash)
    {
        out.host = std::wstring(url.begin() + start, url.begin() + colon);
        std::string portStr = url.substr(colon + 1, slash - colon - 1);
        out.port = std::stoi(portStr);
        out.path = slash != std::string::npos
            ? std::wstring(url.begin() + slash, url.end())
            : L"/";
    }
    else
    {
        out.host = std::wstring(url.begin() + start, url.begin() + slash);
        out.port = out.secure ? 443 : 80;
        out.path = slash != std::string::npos
            ? std::wstring(url.begin() + slash, url.end())
            : L"/";
    }
    return true;
}

static std::string winHttpErrorString(DWORD err)
{
    switch (err) {
        case ERROR_WINHTTP_OUT_OF_HANDLES: return "OUT_OF_HANDLES";
        case ERROR_WINHTTP_TIMEOUT: return "TIMEOUT";
        case ERROR_WINHTTP_INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ERROR_WINHTTP_INVALID_URL: return "INVALID_URL";
        case ERROR_WINHTTP_UNRECOGNIZED_SCHEME: return "UNRECOGNIZED_SCHEME";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED: return "DNS_FAILURE";
        case ERROR_WINHTTP_CANNOT_CONNECT: return "CONNECTION_FAILED";
        case ERROR_WINHTTP_CONNECTION_ERROR: return "CONNECTION_ERROR";
        default: return "UNKNOWN";
    }
}

static bool httpPostJsonInner(const std::string& url, const std::string& body,
                               std::string& response, int timeoutMs,
                               long& outHttpCode)
{
    outHttpCode = 0;
    response.clear();
    UrlParts u;
    if (!parseCoordinatorUrl(url, u)) return false;

    HINTERNET hSession = WinHttpOpen(L"MimitaCoordinator/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), (INTERNET_PORT)u.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", u.path.c_str(), nullptr,
        nullptr, nullptr, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (u.secure) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        DWORD codeSize = sizeof(outHttpCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &outHttpCode, &codeSize, nullptr);
        std::vector<char> buf;
        DWORD bytesRead = 0;
        do {
            char tmp[4096];
            if (!WinHttpReadData(hRequest, tmp, sizeof(tmp), &bytesRead)) break;
            buf.insert(buf.end(), tmp, tmp + bytesRead);
        } while (bytesRead > 0);
        response.assign(buf.data(), buf.size());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

bool httpPostJson(const std::string& url, const std::string& body, std::string& response, int timeoutMs)
{
    long code = 0;
    return httpPostJsonInner(url, body, response, timeoutMs, code);
}

// Safely extract string from json, handling missing keys
static std::string jsonStr(const json& j, const char* key, const char* fallback = "")
{
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : fallback;
}

static int jsonInt(const json& j, const char* key, int fallback = 0)
{
    return j.contains(key) && j[key].is_number() ? j[key].get<int>() : fallback;
}

static bool jsonBool(const json& j, const char* key, bool fallback = false)
{
    return j.contains(key) && j[key].is_boolean() ? j[key].get<bool>() : fallback;
}

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

// Validate SDP has required ICE ufrag/pwd
static bool sdpHasUfragPwd(const std::string& sdp)
{
    return sdp.find("a=ice-ufrag:") != std::string::npos &&
           sdp.find("a=ice-pwd:") != std::string::npos;
}

static std::string iceLogSdpSummary(const std::string& sdp)
{
    size_t ufragPos = sdp.find("a=ice-ufrag:");
    size_t pwdPos = sdp.find("a=ice-pwd:");
    int hostCandidates = 0, srflxCandidates = 0, relayCandidates = 0;
    size_t pos = 0;
    while ((pos = sdp.find("a=candidate:", pos)) != std::string::npos) {
        if (sdp.find(" typ host ", pos) < sdp.find('\n', pos)) ++hostCandidates;
        else if (sdp.find(" typ srflx ", pos) < sdp.find('\n', pos)) ++srflxCandidates;
        else if (sdp.find(" typ relay ", pos) < sdp.find('\n', pos)) ++relayCandidates;
        pos += 12;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "%zu bytes, hasUfrag=%d hasPwd=%d host=%d srflx=%d relay=%d",
             sdp.size(), (int)(ufragPos != std::string::npos),
             (int)(pwdPos != std::string::npos),
             hostCandidates, srflxCandidates, relayCandidates);
    return buf;
}

} // anonymous namespace

void setCoordinatorUrl(const std::string& url)
{
    gCoordinatorUrl = url;
    printf("[COORDINATOR] URL set to %s\n", url.c_str());
}

const std::string& getCoordinatorUrl()
{
    return gCoordinatorUrl;
}

// ── IceHost ──────────────────────────────────────────────────────────

IceHostResult coordinatorIceHost(const std::string& hostSessionId, const std::string& iceDescription,
    const IceHostMetadata& metadata)
{
    IceHostResult result;
    std::string body = "{\"host_session_id\":\"" + jsonEscape(hostSessionId)
        + "\",\"ice_description\":\"" + jsonEscape(iceDescription)
        + "\",\"server_name\":\"" + jsonEscape(metadata.serverName)
        + "\",\"map\":\"" + jsonEscape(metadata.map)
        + "\",\"gamemode\":\"" + jsonEscape(metadata.gamemode)
        + "\",\"max_players\":" + std::to_string(metadata.maxPlayers)
        + ",\"password_protected\":" + (metadata.passwordProtected ? "true" : "false")
        + ",\"host_player_name\":\"" + jsonEscape(metadata.hostPlayerName)
        + "\",\"port\":" + std::to_string(metadata.port) + "}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/host", body, response, 5000, httpCode))
    {
        printf("[ICE ROOM REGISTER] status=%ld duration=%llums FAILED\n", httpCode, nowMs() - t0);
        return result;
    }
    uint64_t dt = nowMs() - t0;
    try {
        auto j = json::parse(response);
        result.ok = jsonBool(j, "ok");
        result.roomCode = jsonStr(j, "room_code");
        result.hostSessionId = jsonStr(j, "host_session_id");
        result.joinToken = jsonStr(j, "join_token");
        printf("[ICE ROOM REGISTER] code=%s name=%s maxPlayers=%d host=%s sdp=%s status=%ld duration=%llums\n",
               result.roomCode.c_str(), metadata.serverName.c_str(), metadata.maxPlayers,
               metadata.hostPlayerName.c_str(),
               iceLogSdpSummary(iceDescription).c_str(), httpCode, dt);
    } catch (const std::exception& e) {
        printf("[ICE ROOM REGISTER] parse error: %s\n", e.what());
    }
    return result;
}

IceHostResult coordinatorIceHostPeer(const std::string& roomCode,
    const std::string& hostSessionId, const std::string& iceDescription)
{
    IceHostResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"host_session_id\":\"" + jsonEscape(hostSessionId)
        + "\",\"ice_description\":\"" + jsonEscape(iceDescription) + "\"}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/host-peer", body, response, 5000, httpCode))
    {
        printf("[ICE HOST PEER REGISTER] code=%s status=%ld duration=%llums FAILED\n",
               roomCode.c_str(), httpCode, nowMs() - t0);
        return result;
    }
    try {
        auto j = json::parse(response);
        result.ok = jsonBool(j, "ok");
        result.roomCode = jsonStr(j, "room_code");
        result.hostSessionId = hostSessionId;
        result.joinToken = jsonStr(j, "join_token");
        printf("[ICE HOST PEER REGISTER] code=%s session=%s ok=%d sdp=%s duration=%llums\n",
               roomCode.c_str(), hostSessionId.substr(0, 12).c_str(),
               (int)result.ok, iceLogSdpSummary(iceDescription).c_str(), nowMs() - t0);
    } catch (const std::exception& e) {
        printf("[ICE HOST PEER REGISTER] parse error: %s\n", e.what());
    }
    return result;
}

// ── IceLookup (non-mutating) ─────────────────────────────────────────

CoordinatorLookupResult coordinatorIceLookup(const std::string& roomCode)
{
    CoordinatorLookupResult result;
    std::string body = "{\"code\":\"" + jsonEscape(roomCode) + "\"}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/lookup", body, response, 5000, httpCode))
    {
        printf("[ICE ROOM LOOKUP] code=%s status=%ld duration=%llums FAILED\n",
               roomCode.c_str(), httpCode, nowMs() - t0);
        return result;
    }
    result.reachable = true;
    try {
        auto j = json::parse(response);
        result.exists = jsonBool(j, "exists");
        result.isIce = jsonBool(j, "is_ice");
        result.status = jsonStr(j, "status");
        result.serverName = jsonStr(j, "server_name");
        result.map = jsonStr(j, "map");
        result.gamemode = jsonStr(j, "gamemode");
        result.players = jsonInt(j, "players");
        result.maxPlayers = jsonInt(j, "max_players");
        result.passwordProtected = jsonBool(j, "password_protected");
    } catch (const std::exception& e) {
        printf("[ICE ROOM LOOKUP] parse error: %s response=%.200s\n", e.what(), response.c_str());
    }
    printf("[ICE ROOM LOOKUP] code=%s exists=%d isIce=%d status=%s httpCode=%ld duration=%llums\n",
           roomCode.c_str(), (int)result.exists, (int)result.isIce,
           result.status.c_str(), httpCode, nowMs() - t0);
    return result;
}

// ── ServerList (public server browser) ───────────────────────────────

std::vector<ServerListEntry> coordinatorServerList()
{
    std::vector<ServerListEntry> result;
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/list", "{}", response, 5000, httpCode))
    {
        printf("[SERVER LIST] status=%ld duration=%llums FAILED\n", httpCode, nowMs() - t0);
        return result;
    }
    try {
        auto j = json::parse(response);
        if (!j.contains("servers") || !j["servers"].is_array())
        {
            printf("[SERVER LIST] status=%ld malformed response (no servers array)\n", httpCode);
            return result;
        }
        for (const auto& it : j["servers"])
        {
            ServerListEntry e;
            e.code = jsonStr(it, "code");
            e.serverName = jsonStr(it, "server_name");
            e.hostPlayerName = jsonStr(it, "host_player_name");
            e.map = jsonStr(it, "map");
            e.gamemode = jsonStr(it, "gamemode");
            e.players = jsonInt(it, "players");
            e.maxPlayers = jsonInt(it, "max_players");
            e.passwordProtected = jsonBool(it, "password_protected");
            e.uptimeSeconds = (uint64_t)jsonInt(it, "uptime_seconds");
            e.publicIp = jsonStr(it, "public_ip");
            e.port = jsonInt(it, "port");
            e.isIce = jsonBool(it, "is_ice");
            result.push_back(std::move(e));
        }
        printf("[SERVER LIST] rooms=%zu duration=%llums status=%ld\n",
               result.size(), nowMs() - t0, httpCode);
    } catch (const std::exception& e) {
        printf("[SERVER LIST] parse error: %s response=%.200s\n", e.what(), response.c_str());
    }
    return result;
}

// ── IceBeginJoin ─────────────────────────────────────────────────────

IceBeginJoinResult coordinatorIceBeginJoin(const std::string& roomCode,
    const std::string& clientSessionId, const std::string& iceDescription)
{
    IceBeginJoinResult result;
    if (!sdpHasUfragPwd(iceDescription))
    {
        result.errorCode = "invalid-sdp";
        printf("[ICE JOIN BEGIN] code=%s sdp=%s REJECTED: invalid-sdp (missing ufrag/pwd)\n",
               roomCode.c_str(), iceLogSdpSummary(iceDescription).c_str());
        return result;
    }
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"client_session_id\":\"" + jsonEscape(clientSessionId)
        + "\",\"ice_description\":\"" + jsonEscape(iceDescription) + "\"}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/begin-join", body, response, 5000, httpCode))
    {
        printf("[ICE JOIN BEGIN] code=%s status=%ld duration=%llums HTTP-FAIL\n",
               roomCode.c_str(), httpCode, nowMs() - t0);
        return result;
    }
    try {
        auto j = json::parse(response);
        result.ok = jsonBool(j, "ok");
        result.errorCode = jsonStr(j, "error");
        result.requestId = jsonStr(j, "request_id");
        result.joinToken = jsonStr(j, "join_token");
        result.hostIceDescription = jsonStr(j, "host_ice_description");
        printf("[ICE JOIN BEGIN] code=%s req=%s ok=%d error=%s sdp=%s duration=%llums\n",
               roomCode.c_str(), result.requestId.substr(0, 12).c_str(),
               (int)result.ok, result.errorCode.c_str(),
               iceLogSdpSummary(iceDescription).c_str(), nowMs() - t0);
    } catch (const std::exception& e) {
        printf("[ICE JOIN BEGIN] parse error: %s\n", e.what());
    }
    return result;
}

// ── IceHostPoll ──────────────────────────────────────────────────────

IceHostPendingRequest coordinatorIceHostPoll(const std::string& roomCode,
    const std::string& hostSessionId, int players)
{
    IceHostPendingRequest result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"host_session_id\":\"" + jsonEscape(hostSessionId) + "\"";
    if (players >= 0)
        body += ",\"players\":" + std::to_string(players);
    body += "}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/host-poll", body, response, 5000, httpCode))
    {
        printf("[ICE HOST POLL] code=%s status=%ld duration=%llums HTTP-FAIL\n",
               roomCode.c_str(), httpCode, nowMs() - t0);
        return result;
    }
    try {
        auto j = json::parse(response);
        result.hasRequest = jsonBool(j, "has_request");
        if (result.hasRequest) {
            result.requestId = jsonStr(j, "request_id");
            result.clientSessionId = jsonStr(j, "client_session_id");
            result.clientIceDescription = jsonStr(j, "client_ice_description");
        }
        printf("[ICE HOST POLL] code=%s players=%d hasRequest=%d duration=%llums status=%ld\n",
               roomCode.c_str(), players, (int)result.hasRequest, nowMs() - t0, httpCode);
        if (result.hasRequest) {
            printf("[ICE HOST REQUEST] code=%s req=%s client=%s sdp=%s duration=%llums\n",
                   roomCode.c_str(), result.requestId.substr(0, 12).c_str(),
                   result.clientSessionId.substr(0, 12).c_str(),
                   iceLogSdpSummary(result.clientIceDescription).c_str(), nowMs() - t0);
        }
    } catch (const std::exception& e) {
        printf("[ICE HOST POLL] parse error: %s\n", e.what());
    }
    return result;
}

// ── IceHostAnswer ────────────────────────────────────────────────────

IceHostAnswerResult coordinatorIceHostAnswer(const std::string& roomCode,
    const std::string& hostSessionId, const std::string& requestId,
    const std::string& hostPeerSdp)
{
    IceHostAnswerResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"host_session_id\":\"" + jsonEscape(hostSessionId)
        + "\",\"request_id\":\"" + jsonEscape(requestId)
        + "\",\"host_peer_sdp\":\"" + jsonEscape(hostPeerSdp) + "\"}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/host-answer", body, response, 5000, httpCode))
        return result;
    try {
        auto j = json::parse(response);
        result.ok = jsonBool(j, "ok");
        printf("[ICE HOST ANSWER] code=%s req=%s ok=%d sdp=%s duration=%llums\n",
               roomCode.c_str(), requestId.substr(0, 12).c_str(),
               (int)result.ok, iceLogSdpSummary(hostPeerSdp).c_str(), nowMs() - t0);
    } catch (const std::exception& e) {
        printf("[ICE HOST ANSWER] parse error: %s\n", e.what());
    }
    return result;
}

// ── IceClientPoll ────────────────────────────────────────────────────

IceClientPollResult coordinatorIceClientPoll(const std::string& roomCode,
    const std::string& requestId)
{
    IceClientPollResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"request_id\":\"" + jsonEscape(requestId) + "\"}";
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/client-poll", body, response, 5000, httpCode))
        return result;
    result.ok = true;
    try {
        auto j = json::parse(response);
        result.status = jsonStr(j, "status");
        result.hostIceDescription = jsonStr(j, "host_ice_description");
        result.errorCode = jsonStr(j, "error");
        printf("[ICE CLIENT POLL] req=%s status=%s duration=%llums\n",
               requestId.substr(0, 12).c_str(), result.status.c_str(), nowMs() - t0);
    } catch (const std::exception& e) {
        printf("[ICE CLIENT POLL] parse error: %s\n", e.what());
    }
    return result;
}

// ── IceRequestComplete ───────────────────────────────────────────────

IceRequestCompleteResult coordinatorIceRequestComplete(const std::string& roomCode,
    const std::string& requestId)
{
    IceRequestCompleteResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"request_id\":\"" + jsonEscape(requestId) + "\"}";
    std::string response;
    uint64_t t0 = nowMs();
    if (!httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/request-complete", body, response, 3000))
        return result;
    try {
        auto j = json::parse(response);
        result.ok = jsonBool(j, "ok");
        printf("[ICE REQUEST COMPLETE] req=%s ok=%d duration=%llums\n",
               requestId.substr(0, 12).c_str(), (int)result.ok, nowMs() - t0);
    } catch (...) {}
    return result;
}

// ── IceValidateJoin ──────────────────────────────────────────────────

bool coordinatorIceValidateJoin(const std::string& roomCode, const std::string& joinToken)
{
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"join_token\":\"" + jsonEscape(joinToken) + "\"}";
    std::string response;
    long httpCode = 0;
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/ice/validate-join", body, response, 5000, httpCode))
    {
        printf("[ICE VALIDATE JOIN] code=%s HTTP=%ld FAILED\n", roomCode.c_str(), httpCode);
        return false;
    }
    try {
        auto j = json::parse(response);
        bool valid = jsonBool(j, "valid");
        printf("[ICE VALIDATE JOIN] code=%s valid=%d\n", roomCode.c_str(), (int)valid);
        return valid;
    } catch (...) {}
    return false;
}

// ── IceDone ──────────────────────────────────────────────────────────

void coordinatorIceDone(const std::string& roomCode)
{
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode) + "\"}";
    std::string response;
    httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/done", body, response, 3000);
}

// ── TURN credentials ─────────────────────────────────────────────────

TurnCredentials coordinatorRequestTurnCredentials()
{
    TurnCredentials result;
    std::string response;
    long httpCode = 0;
    uint64_t t0 = nowMs();
    if (!httpPostJsonInner(gCoordinatorUrl + "/api/coordinator/turn-credentials", "{}", response, 5000, httpCode))
    {
        printf("[TURN CREDENTIALS] status=%ld duration=%llums HTTP-FAIL\n", httpCode, nowMs() - t0);
        return result;
    }
    try {
        auto j = json::parse(response);
        result.ok = true;
        result.host = jsonStr(j, "host", "107.191.48.226");
        result.port = (uint16_t)jsonInt(j, "port", 3478);
        result.username = jsonStr(j, "username");
        result.credential = jsonStr(j, "credential");
        result.expiresAt = (uint32_t)jsonInt(j, "expires_at");
        printf("[TURN CREDENTIALS] host=%s:%u expires_at=%u duration=%llums\n",
               result.host.c_str(), result.port, result.expiresAt, nowMs() - t0);
    } catch (const std::exception& e) {
        printf("[TURN CREDENTIALS] parse error: %s\n", e.what());
    }
    return result;
}

// ── Legacy ICE API (deprecated wrappers) ─────────────────────────────

IceJoinResult coordinatorIceJoin(const std::string& roomCode, const std::string& clientSessionId, const std::string& iceDescription)
{
    IceJoinResult result;
    auto r = coordinatorIceBeginJoin(roomCode, clientSessionId, iceDescription);
    result.ok = r.ok;
    result.hostIceDescription = r.hostIceDescription;
    result.clientSessionId = clientSessionId;
    result.joinToken = r.joinToken;
    return result;
}

IcePollResult coordinatorIcePoll(const std::string& roomCode, const std::string& hostSessionId)
{
    IcePollResult result;
    auto r = coordinatorIceHostPoll(roomCode, hostSessionId);
    result.ok = r.hasRequest;
    result.status = r.hasRequest ? "client_ready" : "waiting_client";
    result.clientIceDescription = r.clientIceDescription;
    result.clientSessionId = r.clientSessionId;
    return result;
}

} // namespace MimitaNet
