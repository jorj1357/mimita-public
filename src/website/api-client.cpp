#include "website/api-client.h"

#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::wstring widen(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

struct UrlInfo {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port;
    bool secure;
};

bool parseUrl(const std::string& url, UrlInfo& info)
{
    std::wstring w = widen(url);
    URL_COMPONENTSW p{};
    wchar_t h[256], pa[2048], e[2048];
    p.dwStructSize = sizeof(p);
    p.lpszHostName = h; p.dwHostNameLength = 256;
    p.lpszUrlPath = pa; p.dwUrlPathLength = 2048;
    p.lpszExtraInfo = e; p.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &p)) return false;
    info.secure = p.nScheme == INTERNET_SCHEME_HTTPS;
    info.host.assign(p.lpszHostName, p.dwHostNameLength);
    info.path.assign(p.lpszUrlPath, p.dwUrlPathLength);
    info.path.append(p.lpszExtraInfo, p.dwExtraInfoLength);
    if (info.path.empty()) info.path = L"/";
    info.port = p.nPort;
    return true;
}

HINTERNET hOpen() {
    return WinHttpOpen(L"MimitaGame/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
}

bool httpRequest(const std::string& method, const std::string& url,
                 const std::string& body, const std::string& authToken,
                 std::string& responseBody, int& statusCode)
{
    responseBody.clear();
    statusCode = 0;

    UrlInfo u;
    if (!parseUrl(url, u)) return false;

    HINTERNET session = hOpen();
    if (!session) return false;
    HINTERNET connect = WinHttpConnect(session, u.host.c_str(), u.port, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    std::wstring wmethod = widen(method);
    HINTERNET request = WinHttpOpenRequest(connect, wmethod.c_str(), u.path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        u.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }

    std::string headers = "Content-Type: application/json\r\n";
    if (!authToken.empty())
        headers += "Authorization: Bearer " + authToken + "\r\n";
    std::wstring wheaders = widen(headers);

    BOOL ok = WinHttpSendRequest(request, wheaders.c_str(), (DWORD)-1L,
        (LPVOID)body.data(), (DWORD)body.size(),
        (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(request, nullptr);

    DWORD st = 0, stsz = sizeof(st);
    if (ok) {
        WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &st, &stsz, WINHTTP_NO_HEADER_INDEX);
    }
    statusCode = (int)st;

    if (ok) {
        std::vector<char> buf;
        DWORD read = 0;
        do {
            char tmp[4096];
            if (!WinHttpReadData(request, tmp, sizeof(tmp), &read)) break;
            buf.insert(buf.end(), tmp, tmp + read);
        } while (read > 0);
        responseBody.assign(buf.data(), buf.size());
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

}

bool websiteReachable()
{
    std::string body;
    int code = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/debug/health",
                          "", "", body, code);
    return ok && code >= 200 && code < 400;
}

LinkCodeResult requestAuthLink()
{
    LinkCodeResult result;
    std::string body;
    int code = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/auth/link-code",
                          "", "", body, code);
    if (!ok || code != 200) return result;

    try {
        json j = json::parse(body);
        if (j.value("success", false)) {
            result.ok = true;
            result.code = j.value("code", "");
            result.grantToken = j.value("grant_token", "");
        }
    } catch (...) {}
    return result;
}

bool pollLinkStatus(const std::string& code)
{
    std::string url = "https://mimita.fun/api/auth/link-poll?code=" + code;
    std::string body;
    int httpCode = 0;
    bool ok = httpRequest("GET", url, "", "", body, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(body);
        return j.value("claimed", false);
    } catch (...) {}
    return false;
}

std::string finalizeLink(const std::string& code, const std::string& grantToken)
{
    json req;
    req["code"] = code;
    req["grant_token"] = grantToken;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/auth/link-finalize",
                          req.dump(), "", respBody, httpCode);
    if (!ok || httpCode != 200) return {};

    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("session_token", "");
    } catch (...) {}
    return {};
}

GameUserInfo validateSession(const std::string& sessionToken)
{
    GameUserInfo info;
    if (sessionToken.empty()) return info;

    std::string body;
    int code = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/auth/me",
                          "", sessionToken, body, code);
    if (!ok || code != 200) return info;

    try {
        json j = json::parse(body);
        if (!j.value("success", false)) return info;
        auto u = j["user"];
        info.valid = true;
        info.id = u.value("id", 0);
        info.username = u.value("username", "");
        info.displayName = u.value("display_name", "");
        if (info.displayName.empty())
            info.displayName = info.username;
        info.email = u.value("email", "");
        info.bio = u.value("bio", "");
        info.avatarUrl = u.value("avatar_url", "");
        info.supporterTier = u.value("supporter_tier", "free");
        info.role = u.value("role", "user");
        info.emailVerified = u.value("email_verified", false);
        info.createdAt = u.value("created_at", "");
    } catch (...) {}
    return info;
}

std::string exchangeTempToken(const std::string& exchangeToken)
{
    json req;
    req["exchange_token"] = exchangeToken;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/auth/exchange-session",
                          req.dump(), "", respBody, httpCode);
    if (!ok || httpCode != 200) return {};

    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("session_token", "");
    } catch (...) {}
    return {};
}

GameUserInfo getProfile(const std::string& sessionToken)
{
    GameUserInfo info;
    if (sessionToken.empty()) return info;

    std::string body;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/profile",
                          "", sessionToken, body, httpCode);
    if (!ok || httpCode != 200) return info;

    try {
        json j = json::parse(body);
        if (!j.value("success", false)) return info;
        auto p = j["profile"];
        info.valid = true;
        info.id = p.value("id", 0);
        info.username = p.value("username", "");
        info.displayName = p.value("display_name", p.value("username", ""));
        info.email = p.value("email", "");
        info.bio = p.value("bio", "");
        info.avatarUrl = p.value("avatar_url", "");
        if (p.contains("avatar_data") && !p["avatar_data"].is_null())
            info.avatarData = p["avatar_data"];
        info.supporterTier = p.value("supporter_tier", "free");
        info.role = p.value("role", "user");
        info.createdAt = p.value("created_at", "");
    } catch (...) {}
    return info;
}

bool updateProfile(const std::string& sessionToken, const json& updates)
{
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("PATCH", "https://mimita.fun/api/profile",
                          updates.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

json getAvatarData(const std::string& sessionToken)
{
    json nullData;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/avatar/data",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return nullData;
    try {
        json j = json::parse(respBody);
        if (j.value("success", false) && j.contains("avatar_data") && !j["avatar_data"].is_null())
            return j["avatar_data"];
    } catch (...) {}
    return nullData;
}

bool uploadAvatarData(const std::string& sessionToken, const json& avatarData)
{
    json req;
    req["avatar_data"] = avatarData;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("PUT", "https://mimita.fun/api/avatar/data",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

GameStats getStats(const std::string& sessionToken)
{
    GameStats stats;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/stats",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return stats;
    try {
        json j = json::parse(respBody);
        if (!j.value("success", false)) return stats;
        auto s = j["stats"];
        stats.wins = s.value("wins", 0);
        stats.losses = s.value("losses", 0);
        stats.kills = s.value("kills", 0);
        stats.deaths = s.value("deaths", 0);
        stats.gamesPlayed = s.value("games_played", 0);
        stats.playtimeSeconds = s.value("playtime_seconds", 0LL);
        stats.highestMmr = s.value("highest_mmr", 5000);
        stats.currentMmr = s.value("current_mmr", 5000);
        stats.accuracy = s.value("accuracy", 0.0f);
        stats.headshots = s.value("headshots", 0);
        stats.bestKillStreak = s.value("best_kill_streak", 0);
    } catch (...) {}
    return stats;
}

bool submitMatchResult(const std::string& sessionToken, const json& matchData)
{
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/stats",
                          matchData.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

std::vector<LeaderboardEntry> getLeaderboard(const std::string& type, int limit)
{
    std::vector<LeaderboardEntry> result;
    std::string url = "https://mimita.fun/api/leaderboard?type=" + type
                    + "&limit=" + std::to_string(limit);
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", url, "", "", respBody, httpCode);
    if (!ok || httpCode != 200) return result;
    try {
        json j = json::parse(respBody);
        if (!j.value("success", false)) return result;
        for (const auto& entry : j["leaderboard"])
        {
            LeaderboardEntry le;
            le.rank = entry.value("rank", 0);
            le.id = entry.value("id", 0);
            le.username = entry.value("username", "");
            le.avatarUrl = entry.value("avatar_url", "");
            le.supporterTier = entry.value("supporter_tier", "free");
            auto& s = le.stats;
            s.wins = entry.value("wins", 0);
            s.losses = entry.value("losses", 0);
            s.kills = entry.value("kills", 0);
            s.deaths = entry.value("deaths", 0);
            s.gamesPlayed = entry.value("games_played", 0);
            s.playtimeSeconds = entry.value("playtime_seconds", 0LL);
            s.highestMmr = entry.value("highest_mmr", 5000);
            s.currentMmr = entry.value("current_mmr", 5000);
            s.accuracy = entry.value("accuracy", 0.0f);
            s.headshots = entry.value("headshots", 0);
            s.bestKillStreak = entry.value("best_kill_streak", 0);
            result.push_back(le);
        }
    } catch (...) {}
    return result;
}

std::vector<MatchEntry> getMatchHistory(const std::string& sessionToken, int page, int limit)
{
    std::vector<MatchEntry> result;
    std::string url = "https://mimita.fun/api/match-history?page="
                    + std::to_string(page) + "&limit=" + std::to_string(limit);
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", url, "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return result;
    try {
        json j = json::parse(respBody);
        if (!j.value("success", false)) return result;
        for (const auto& m : j["matches"])
        {
            MatchEntry me;
            me.matchId = m.value("match_id", "");
            me.mapName = m.value("map_name", "");
            me.gameMode = m.value("game_mode", "");
            me.durationSeconds = m.value("duration_seconds", 0);
            me.createdAt = m.value("created_at", "");
            me.kills = m.value("kills", 0);
            me.deaths = m.value("deaths", 0);
            me.accuracy = m.value("accuracy", 0.0f);
            me.headshots = m.value("headshots", 0);
            me.damageDealt = m.value("damage_dealt", 0);
            me.won = m.value("won", false);
            me.mmrBefore = m.value("mmr_before", 0);
            me.mmrAfter = m.value("mmr_after", 0);
            result.push_back(me);
        }
    } catch (...) {}
    return result;
}

json getSettings(const std::string& sessionToken)
{
    json empty;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/settings",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return empty;
    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("settings", empty);
    } catch (...) {}
    return empty;
}

bool updateSettings(const std::string& sessionToken, const json& settings)
{
    json req;
    req["settings"] = settings;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("PUT", "https://mimita.fun/api/settings",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

// ── Client Login (4-letter code flow) ────────────────────────────────────────

ClientCodePreview previewClientCode(const std::string& code)
{
    ClientCodePreview result;
    json req;
    req["code"] = code;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/client-login/preview",
                          req.dump(), "", respBody, httpCode);
    if (!ok || httpCode != 200) return result;

    try {
        json j = json::parse(respBody);
        if (j.value("success", false) && j.value("valid", false))
        {
            result.valid = true;
            result.username = j.value("username", "");
            result.displayName = j.value("display_name", j.value("username", ""));
            result.avatarUrl = j.value("avatar_url", "");
            if (j.contains("avatar_data") && !j["avatar_data"].is_null())
                result.avatarData = j["avatar_data"];
            result.supporterTier = j.value("supporter_tier", "free");
        }
    } catch (...) {}
    return result;
}

ClientCodeConfirm confirmClientCode(const std::string& code)
{
    ClientCodeConfirm result;
    json req;
    req["code"] = code;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/client-login/confirm",
                          req.dump(), "", respBody, httpCode);
    if (!ok || httpCode != 200) return result;

    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
        {
            result.success = true;
            result.sessionToken = j.value("session_token", "");
            auto account = j.value("account", json::object());
            result.accountId = account.value("id", 0);
            result.username = account.value("username", "");
        }
    } catch (...) {}
    return result;
}
