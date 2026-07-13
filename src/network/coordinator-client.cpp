#include "network/coordinator-client.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace MimitaNet {

namespace {

std::string gCoordinatorUrl = "http://107.191.48.226:3001";

// Simple JSON extraction helpers
std::string extractJsonStr(const std::string& json, const std::string& key)
{
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return "";
    k = json.find('"', k + key.size() + 2);
    if (k == std::string::npos) return "";
    auto s = k + 1, e = json.find('"', s);
    return (e == std::string::npos) ? "" : json.substr(s, e - s);
}

int extractJsonInt(const std::string& json, const std::string& key)
{
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return 0;
    k = json.find(':', k + key.size() + 2);
    if (k == std::string::npos) return 0;
    ++k;
    while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
    int sign = 1;
    if (k < json.size() && json[k] == '-') { sign = -1; ++k; }
    int val = 0;
    while (k < json.size() && json[k] >= '0' && json[k] <= '9') {
        val = val * 10 + (json[k] - '0');
        ++k;
    }
    return val * sign;
}

bool extractJsonBool(const std::string& json, const std::string& key)
{
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return false;
    k = json.find(':', k + key.size() + 2);
    if (k == std::string::npos) return false;
    ++k;
    while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
    return json.substr(k, 4) == "true";
}

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
    switch (err)
    {
        case ERROR_WINHTTP_OUT_OF_HANDLES: return "OUT_OF_HANDLES";
        case ERROR_WINHTTP_TIMEOUT: return "TIMEOUT";
        case ERROR_WINHTTP_INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ERROR_WINHTTP_INVALID_URL: return "INVALID_URL";
        case ERROR_WINHTTP_UNRECOGNIZED_SCHEME: return "UNRECOGNIZED_SCHEME";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED: return "DNS_FAILURE";
        case ERROR_WINHTTP_INVALID_OPTION: return "INVALID_OPTION";
        case ERROR_WINHTTP_OPTION_NOT_SETTABLE: return "OPTION_NOT_SETTABLE";
        case ERROR_WINHTTP_SHUTDOWN: return "SHUTDOWN";
        case ERROR_WINHTTP_LOGIN_FAILURE: return "LOGIN_FAILURE";
        case ERROR_WINHTTP_OPERATION_CANCELLED: return "CANCELLED";
        case ERROR_WINHTTP_INCORRECT_HANDLE_TYPE: return "INCORRECT_HANDLE_TYPE";
        case ERROR_WINHTTP_INCORRECT_HANDLE_STATE: return "INCORRECT_HANDLE_STATE";
        case ERROR_WINHTTP_CANNOT_CONNECT: return "CONNECTION_FAILED";
        case ERROR_WINHTTP_CONNECTION_ERROR: return "CONNECTION_ERROR";
        case ERROR_WINHTTP_RESEND_REQUEST: return "RESEND_REQUEST";
        case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED: return "CLIENT_AUTH_CERT_NEEDED";
        default: return "UNKNOWN";
    }
}

bool httpPostJson(const std::string& url, const std::string& body, std::string& response, int timeoutMs)
{
    UrlParts u;
    if (!parseCoordinatorUrl(url, u))
    {
        printf("[COORDINATOR HTTP] parseCoordinatorUrl failed for %s\n", url.c_str());
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"MimitaCoordinator/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession)
    {
        printf("[COORDINATOR HTTP] WinHttpOpen failed error=%lu\n", GetLastError());
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), (INTERNET_PORT)u.port, 0);
    if (!hConnect)
    {
        DWORD err = GetLastError();
        printf("[COORDINATOR HTTP] WinHttpConnect failed error=%lu (%s) for host=%S port=%d\n",
               err, winHttpErrorString(err).c_str(), u.host.c_str(), (int)u.port);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", u.path.c_str(), nullptr,
        nullptr, nullptr, flags);
    if (!hRequest)
    {
        DWORD err = GetLastError();
        printf("[COORDINATOR HTTP] WinHttpOpenRequest failed error=%lu (%s)\n",
               err, winHttpErrorString(err).c_str());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set timeout
    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    // Disable SSL cert checking for dev
    if (u.secure)
    {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    response.clear();
    if (ok)
    {
        DWORD httpCode = 0;
        DWORD httpCodeSize = sizeof(httpCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &httpCode, &httpCodeSize, nullptr);

        std::vector<char> buf;
        DWORD bytesRead = 0;
        do {
            char tmp[4096];
            if (!WinHttpReadData(hRequest, tmp, sizeof(tmp), &bytesRead)) break;
            buf.insert(buf.end(), tmp, tmp + bytesRead);
        } while (bytesRead > 0);
        response.assign(buf.data(), buf.size());
    }
    else
    {
        DWORD err = GetLastError();
        printf("[COORDINATOR HTTP] WinHttpSendRequest/ReceiveResponse failed error=%lu (%s)\n",
               err, winHttpErrorString(err).c_str());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
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

CoordinatorRoomInfo coordinatorRegister(
    const std::string& hostSessionId,
    const std::string& publicIp,
    uint16_t port,
    const std::string& serverName,
    const std::string& mapName,
    const std::string& gamemode,
    int maxPlayers)
{
    CoordinatorRoomInfo info;

    std::string body = "{"
        "\"host_session_id\":\"" + hostSessionId + "\","
        "\"public_ip\":\"" + publicIp + "\","
        "\"port\":" + std::to_string(port) + ","
        "\"server_name\":\"" + serverName + "\","
        "\"map\":\"" + mapName + "\","
        "\"gamemode\":\"" + gamemode + "\","
        "\"max_players\":" + std::to_string(maxPlayers) + ""
        "}";

    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/register";
    printf("[COORDINATOR] registering...\n");

    if (httpPostJson(url, body, response, 5000))
    {
        info.code = extractJsonStr(response, "code");
        info.joinToken = extractJsonStr(response, "join_token");
        printf("[COORDINATOR] registered code=%s token=%s\n",
               info.code.c_str(), info.joinToken.substr(0, 12).c_str());
    }
    else
    {
        printf("[COORDINATOR REGISTER FAIL] url=%s http=0 body=(empty)\n", url.c_str());
        if (!response.empty())
            printf("[COORDINATOR REGISTER FAIL] responseBody=%s\n", response.c_str());
    }

    return info;
}

bool coordinatorHeartbeat(const std::string& code, int playerCount)
{
    std::string body = "{\"code\":\"" + code + "\",\"players\":" + std::to_string(playerCount) + "}";
    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/heartbeat";
    bool ok = httpPostJson(url, body, response, 3000);
    if (!ok)
    {
        printf("[COORDINATOR HEARTBEAT FAIL] code=%s url=%s\n", code.c_str(), url.c_str());
        if (!response.empty())
            printf("[COORDINATOR HEARTBEAT FAIL] body=%s\n", response.c_str());
    }
    return ok;
}

bool coordinatorLeave(const std::string& code)
{
    std::string body = "{\"code\":\"" + code + "\"}";
    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/leave";
    bool ok = httpPostJson(url, body, response, 3000);
    printf("[COORDINATOR] leave code=%s ok=%d\n", code.c_str(), (int)ok);
    return ok;
}

CoordinatorLookupResult coordinatorLookup(const std::string& code)
{
    CoordinatorLookupResult result;
    std::string body = "{\"code\":\"" + code + "\"}";
    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/lookup";

    if (httpPostJson(url, body, response, 5000))
    {
        result.reachable = true;
        result.exists = extractJsonStr(response, "status") != "null_r" &&
                        extractJsonStr(response, "status") != "";
        if (result.exists)
        {
            result.serverName = extractJsonStr(response, "server_name");
            result.map = extractJsonStr(response, "map");
            result.gamemode = extractJsonStr(response, "gamemode");
            result.players = extractJsonInt(response, "players");
            result.maxPlayers = extractJsonInt(response, "max_players");
            result.passwordProtected = extractJsonBool(response, "password_protected");
            result.status = extractJsonStr(response, "status");
            printf("[COORDINATOR] lookup code=%s name=%s status=%s players=%d/%d\n",
                   code.c_str(), result.serverName.c_str(), result.status.c_str(),
                   result.players, result.maxPlayers);
        }
    }
    else
    {
        printf("[COORDINATOR] lookup FAILED for code=%s\n", code.c_str());
    }

    return result;
}

CoordinatorJoinResult coordinatorJoin(const std::string& code, const std::string& playerName)
{
    CoordinatorJoinResult result;
    std::string body = "{\"code\":\"" + code + "\",\"player_name\":\"" + playerName + "\"}";
    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/join";

    if (httpPostJson(url, body, response, 5000))
    {
        result.joinToken = extractJsonStr(response, "join_token");
        result.serverIp = extractJsonStr(response, "server_ip");
        result.serverPort = (uint16_t)extractJsonInt(response, "server_port");
        result.serverName = extractJsonStr(response, "server_name");
        result.ok = !result.joinToken.empty();
        if (result.ok)
        {
            printf("[COORDINATOR] join code=%s server=%s:%u token=%s\n",
                   code.c_str(), result.serverIp.c_str(),
                   result.serverPort, result.joinToken.substr(0, 12).c_str());
        }
        else
        {
            printf("[COORDINATOR JOIN FAIL] url=%s response=%s\n", url.c_str(), response.c_str());
        }
    }
    else
    {
        printf("[COORDINATOR JOIN FAIL] url=%s http=0 (timeout/connection error)\n", url.c_str());
        if (!response.empty())
            printf("[COORDINATOR JOIN FAIL] body=%s\n", response.c_str());
    }

    return result;
}

bool coordinatorValidateJoin(const std::string& code, const std::string& joinToken)
{
    std::string body = "{\"code\":\"" + code + "\",\"join_token\":\"" + joinToken + "\"}";
    std::string response;
    std::string url = gCoordinatorUrl + "/api/coordinator/join-validate";

    if (httpPostJson(url, body, response, 5000))
    {
        std::string valid = extractJsonStr(response, "valid");
        bool ok = valid == "true";
        printf("[COORDINATOR] validate-join code=%s token=%s valid=%d\n",
               code.c_str(), joinToken.substr(0, 12).c_str(), (int)ok);
        return ok;
    }

    printf("[COORDINATOR] validate-join FAILED for code=%s\n", code.c_str());
    return false;
}

bool coordinatorHttpGet(const std::string& url, std::string& response, int timeoutMs)
{
    UrlParts u;
    if (!parseCoordinatorUrl(url, u)) return false;

    HINTERNET hSession = WinHttpOpen(L"MimitaCoordinator/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), (INTERNET_PORT)u.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", u.path.c_str(), nullptr,
        nullptr, nullptr, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    response.clear();
    if (ok)
    {
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

// ── Async lookup ─────────────────────────────────────────────────────

void coordinatorLookupAsync(AsyncLookupResult& state, const std::string& code)
{
    if (state.pending) return;
    state.pending = true;
    state.done = false;
    state.failed = false;
    state.code = code;
    state.result = {};

    std::thread([&state, code]() {
        state.result = coordinatorLookup(code);
        state.done = true;
        state.failed = !state.result.exists;
        state.pending = false;
    }).detach();
}

// ── Async join ───────────────────────────────────────────────────────

void coordinatorJoinAsync(AsyncJoinResult& state, const std::string& code, const std::string& playerName)
{
    if (state.pending) return;
    state.pending = true;
    state.done = false;
    state.failed = false;
    state.code = code;
    state.playerName = playerName;
    state.result = {};

    std::thread([&state, code, playerName]() {
        state.result = coordinatorJoin(code, playerName);
        state.done = true;
        state.failed = !state.result.ok;
        state.pending = false;
    }).detach();
}

// ── ICE signaling ─────────────────────────────────────────────────────

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
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

static std::string jsonUnescape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            switch (s[i + 1])
            {
            case '"':  out += '"'; ++i; break;
            case '\\': out += '\\'; ++i; break;
            case '/':  out += '/'; ++i; break;
            case 'b':  out += '\b'; ++i; break;
            case 'f':  out += '\f'; ++i; break;
            case 'n':  out += '\n'; ++i; break;
            case 'r':  out += '\r'; ++i; break;
            case 't':  out += '\t'; ++i; break;
            case 'u':
                // Simple uXXXX unicode - skip for now, just pass through
                out += '\\';
                break;
            default: out += s[i]; break;
            }
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

IceHostResult coordinatorIceHost(const std::string& hostSessionId, const std::string& iceDescription)
{
    IceHostResult result;
    std::string escaped = jsonEscape(iceDescription);
    std::string body = "{\"host_session_id\":\"" + jsonEscape(hostSessionId)
        + "\",\"ice_description\":\"" + escaped + "\"}";
    std::string response;
    if (!httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/host", body, response, 5000))
        return result;
    result.ok = extractJsonBool(response, "ok");
    result.roomCode = extractJsonStr(response, "room_code");
    result.hostSessionId = extractJsonStr(response, "host_session_id");
    return result;
}

IceJoinResult coordinatorIceJoin(const std::string& roomCode, const std::string& clientSessionId, const std::string& iceDescription)
{
    IceJoinResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"client_session_id\":\"" + jsonEscape(clientSessionId)
        + "\",\"ice_description\":\"" + jsonEscape(iceDescription) + "\"}";
    std::string response;
    if (!httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/join", body, response, 5000))
        return result;
    result.ok = extractJsonBool(response, "ok");
    result.hostIceDescription = jsonUnescape(extractJsonStr(response, "host_ice_description"));
    result.clientSessionId = extractJsonStr(response, "client_session_id");
    return result;
}

IcePollResult coordinatorIcePoll(const std::string& roomCode, const std::string& hostSessionId)
{
    IcePollResult result;
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode)
        + "\",\"host_session_id\":\"" + jsonEscape(hostSessionId) + "\"}";
    std::string response;
    if (!httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/poll", body, response, 5000))
        return result;
    result.ok = extractJsonBool(response, "ok");
    result.status = extractJsonStr(response, "status");
    result.clientIceDescription = jsonUnescape(extractJsonStr(response, "client_ice_description"));
    result.clientSessionId = extractJsonStr(response, "client_session_id");
    return result;
}

void coordinatorIceDone(const std::string& roomCode)
{
    std::string body = "{\"room_code\":\"" + jsonEscape(roomCode) + "\"}";
    std::string response;
    httpPostJson(gCoordinatorUrl + "/api/coordinator/ice/done", body, response, 3000);
}

} // namespace MimitaNet
