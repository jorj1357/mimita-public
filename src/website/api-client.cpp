// 07 19 2026, 12 00
/* purpose
* Implement the game client's HTTP calls to mimita.fun APIs.
* Parse account, auth, profile, stats, settings, and bootstrap JSON responses.
* Keep transport details isolated from auth/gameplay systems.
* DOES NOT store local tokens or passwords.
* DOES NOT render UI or mutate player state directly.
* DOES NOT implement backend database logic.
*/

#include "website/api-client.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
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


int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

bool parseHexColor(const std::string& hex, uint8_t& r, uint8_t& g, uint8_t& b)
{
    if (hex.size() != 7 || hex[0] != '#')
        return false;
    int values[6];
    for (int i = 0; i < 6; ++i)
    {
        values[i] = hexValue(hex[(size_t)i + 1]);
        if (values[i] < 0)
            return false;
    }
    r = (uint8_t)((values[0] << 4) | values[1]);
    g = (uint8_t)((values[2] << 4) | values[3]);
    b = (uint8_t)((values[4] << 4) | values[5]);
    return true;
}

const char* staffColorForRole(const std::string& role)
{
    if (role == "owner") return "#000000";
    if (role == "admin") return "#000000";
    if (role == "moderator") return "#ff0000";
    return "";
}

MimitaVip::VipAppearance parseVipAppearanceJson(const json& owner,
                                                const std::string& fallbackTier,
                                                const std::string& role)
{
    std::string tier = fallbackTier.empty() ? "free" : fallbackTier;
    json vip = json::object();
    if (owner.contains("vip") && owner["vip"].is_object())
    {
        vip = owner["vip"];
        tier = vip.value("active_tier", tier);
    }

    MimitaVip::VipAppearance out =
        MimitaVip::tierDefaultAppearance(MimitaVip::tierFromString(tier));

    if (vip.contains("name_style") && vip["name_style"].is_object())
    {
        const json& style = vip["name_style"];
        const uint8_t styleKind = MimitaVip::styleKindFromString(style.value("kind", ""));
        if (styleKind != MimitaVip::VIP_STYLE_NONE)
            out.styleKind = styleKind;

        uint8_t r = out.colorR;
        uint8_t g = out.colorG;
        uint8_t b = out.colorB;
        bool hasColor = false;
        if (style.contains("solid_color") && style["solid_color"].is_string())
            hasColor = parseHexColor(style["solid_color"].get<std::string>(), r, g, b);
        if (!hasColor && style.contains("colors") && style["colors"].is_array() && !style["colors"].empty() && style["colors"][0].is_string())
            hasColor = parseHexColor(style["colors"][0].get<std::string>(), r, g, b);
        if (hasColor)
        {
            out.colorR = r;
            out.colorG = g;
            out.colorB = b;
        }
    }

    std::string staffHex;
    if (vip.contains("display") && vip["display"].is_object())
        staffHex = vip["display"].value("name_color_override", "");
    if (staffHex.empty())
        staffHex = staffColorForRole(role);

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    if (parseHexColor(staffHex, r, g, b))
    {
        out.colorR = r;
        out.colorG = g;
        out.colorB = b;
        out.styleKind = MimitaVip::VIP_STYLE_SOLID;
        out.flags |= MimitaVip::VIP_APPEARANCE_STAFF_OVERRIDE;
    }

    return out;
}

MimitaVip::VipStyleDetail parseVipStyleDetailJson(const json& owner,
                                                  const std::string& fallbackTier,
                                                  const std::string& role)
{
    std::string tier = fallbackTier.empty() ? "free" : fallbackTier;
    json vip = json::object();
    if (owner.contains("vip") && owner["vip"].is_object())
    {
        vip = owner["vip"];
        tier = vip.value("active_tier", tier);
    }

    MimitaVip::VipStyleDetail out;
    if (tier == "free")
        return out;

    // Staff color overrides are authoritative and handled by the compact
    // solid appearance; never let a stored style bypass staff authority.
    std::string staffHex;
    if (vip.contains("display") && vip["display"].is_object())
        staffHex = vip["display"].value("name_color_override", "");
    if (staffHex.empty())
        staffHex = staffColorForRole(role);
    if (!staffHex.empty())
        return out;

    if (!vip.contains("name_style") || !vip["name_style"].is_object())
        return out;

    const json& style = vip["name_style"];
    const uint8_t styleKind = MimitaVip::styleKindFromString(style.value("kind", ""));
    if (styleKind == MimitaVip::VIP_STYLE_NONE)
        return out;

    out.styleKind = styleKind;

    if (styleKind == MimitaVip::VIP_STYLE_SOLID ||
        styleKind == MimitaVip::VIP_STYLE_TURQUOISE)
    {
        uint8_t r = 64, g = 224, b = 208;
        if (style.contains("solid_color") && style["solid_color"].is_string())
            parseHexColor(style["solid_color"].get<std::string>(), r, g, b);
        out.colors.push_back(MimitaVip::colorFromBytes(r, g, b));
        out.solidColor = out.colors[0];
        return out;
    }

    if (style.contains("colors") && style["colors"].is_array())
    {
        for (const auto& item : style["colors"])
        {
            if (!item.is_string()) continue;
            uint8_t r = 158, g = 158, b = 158;
            if (!parseHexColor(item.get<std::string>(), r, g, b)) continue;
            out.colors.push_back(MimitaVip::colorFromBytes(r, g, b));
            if (out.colors.size() >= MimitaVip::VIP_STYLE_MAX_COLORS) break;
        }
    }
    if (out.colors.empty())
        return out;

    out.solidColor = out.colors[0];
    const double speed = style.value("rainbow_speed", 1.0);
    out.rainbowSpeed = (float)std::clamp(speed, 0.25, 4.0);
    out.direction = style.value("rainbow_direction", "ltr") == "rtl"
        ? MimitaVip::VIP_DIRECTION_RTL : MimitaVip::VIP_DIRECTION_LTR;
    const std::string animation = style.value("animation", "");
    if (animation == "cycle")
        out.animation = MimitaVip::VIP_ANIMATION_CYCLE;
    else if (animation == "pulse")
        out.animation = MimitaVip::VIP_ANIMATION_PULSE;
    else
        out.animation = MimitaVip::VIP_ANIMATION_NONE;

    out.styleEpoch = (uint32_t)vip.value("style_revision", 1);

    return out;
}
}

static void parseUserInfo(GameUserInfo& info, const json& u)
{
    info.valid = true;
    if (u.contains("id"))
    {
        if (u["id"].is_number_integer())
            info.id = u["id"].get<int>();
        else if (u["id"].is_string())
            info.id = std::atoi(u["id"].get<std::string>().c_str());
    }
    info.username = u.value("username", "");
    info.displayName = u.value("display_name", "");
    if (info.displayName.empty())
        info.displayName = info.username;
    info.email = u.value("email", "");
    info.bio = u.value("bio", "");
    info.avatarUrl = u.value("avatar_url", "");
    if (u.contains("avatar_data") && !u["avatar_data"].is_null())
        info.avatarData = u["avatar_data"];
    info.supporterTier = u.value("supporter_tier", "free");
    info.role = u.value("role", "user");
    info.vipAppearance = parseVipAppearanceJson(u, info.supporterTier, info.role);
    info.vipStyleDetail = parseVipStyleDetailJson(u, info.supporterTier, info.role);
    info.supporterTier = MimitaVip::tierToString(info.vipAppearance.tier);
    info.emailVerified = u.value("email_verified", false);
    info.createdAt = u.value("created_at", "");
}

static int jsonInt(const json& j, const std::string& key, int fallback)
{
    if (!j.contains(key)) return fallback;
    const json& v = j[key];
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_string()) return std::atoi(v.get<std::string>().c_str());
    return fallback;
}

static long long jsonLong(const json& j, const std::string& key, long long fallback)
{
    if (!j.contains(key)) return fallback;
    const json& v = j[key];
    if (v.is_number()) return v.get<long long>();
    if (v.is_string()) return std::atoll(v.get<std::string>().c_str());
    return fallback;
}

static float jsonFloat(const json& j, const std::string& key, float fallback)
{
    if (!j.contains(key)) return fallback;
    const json& v = j[key];
    if (v.is_number()) return v.get<float>();
    if (v.is_string()) return (float)std::atof(v.get<std::string>().c_str());
    return fallback;
}

static void parseStats(GameStats& stats, const json& s)
{
    stats.wins = jsonInt(s, "wins", 0);
    stats.losses = jsonInt(s, "losses", 0);
    stats.kills = jsonInt(s, "kills", 0);
    stats.deaths = jsonInt(s, "deaths", 0);
    stats.gamesPlayed = jsonInt(s, "games_played", 0);
    stats.playtimeSeconds = jsonLong(s, "playtime_seconds", 0LL);
    stats.highestMmr = jsonInt(s, "highest_mmr", 5000);
    stats.currentMmr = jsonInt(s, "current_mmr", 5000);
    stats.accuracy = jsonFloat(s, "accuracy", 0.0f);
    stats.headshots = jsonInt(s, "headshots", 0);
    stats.bestKillStreak = jsonInt(s, "best_kill_streak", 0);
    stats.totalXp = jsonLong(s, "total_xp", 0LL);
    stats.gold = jsonLong(s, "gold", 0LL);
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
        parseUserInfo(info, j["user"]);
    } catch (...) {}
    return info;
}

GameBootstrap getGameBootstrap(const std::string& sessionToken)
{
    GameBootstrap bootstrap;
    if (sessionToken.empty())
    {
        Debug::warn(Debug::Category::Auth, "BOOTSTRAP skipped: empty session token\n");
        return bootstrap;
    }

    std::string body;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/game/me",
                          "", sessionToken, body, httpCode);
    if (!ok || httpCode != 200)
    {
        Debug::warn(Debug::Category::Auth, "BOOTSTRAP failed: httpCode=%d ok=%d\n",
               httpCode, (int)ok);
        return bootstrap;
    }

    try {
        json j = json::parse(body);
        if (!j.value("success", false))
        {
            Debug::warn(Debug::Category::Auth, "BOOTSTRAP failed: server success=false body=%.200s\n",
                   body.c_str());
            return bootstrap;
        }
        parseUserInfo(bootstrap.user, j["profile"]);
        parseStats(bootstrap.stats, j.value("stats", json::object()));
        bootstrap.settings = j.value("settings", json::object());
        bootstrap.inventory = j.value("inventory", json::object());
        bootstrap.titles = j.value("titles", json::object());
        bootstrap.loadout = j.value("loadout", json::object());
        bootstrap.valid = bootstrap.user.valid && !bootstrap.user.username.empty();
        if (!bootstrap.valid)
        {
            Debug::warn(Debug::Category::Auth, "BOOTSTRAP failed: user.valid=%d username='%s'\n",
                   (int)bootstrap.user.valid, bootstrap.user.username.c_str());
        }
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::Auth, "BOOTSTRAP failed: json parse exception '%s'\n",
               e.what());
    }
    catch (...)
    {
        Debug::warn(Debug::Category::Auth, "BOOTSTRAP failed: unknown exception\n");
    }
    return bootstrap;
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
        parseUserInfo(info, j["profile"]);
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
        parseStats(stats, j["stats"]);
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

json submitPersistenceBatch(const std::string& sessionToken, const std::string& bodyJson,
                            bool registerSession)
{
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", registerSession ? "https://mimita.fun/api/progression/session"
                                                : "https://mimita.fun/api/progression/batch",
                          bodyJson, sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return json::object();
    try {
        json j = json::parse(respBody);
        return j;
    } catch (...) {}
    return json::object();
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
            le.vipAppearance = parseVipAppearanceJson(entry, le.supporterTier, entry.value("role", "user"));
            le.vipStyleDetail = parseVipStyleDetailJson(entry, le.supporterTier, entry.value("role", "user"));
            le.supporterTier = MimitaVip::tierToString(le.vipAppearance.tier);
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

json getInventory(const std::string& sessionToken)
{
    json empty;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/game/inventory",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return empty;
    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("inventory", empty);
    } catch (...) {}
    return empty;
}

bool updateInventory(const std::string& sessionToken, const json& inventory)
{
    json req;
    req["inventory"] = inventory;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("PUT", "https://mimita.fun/api/game/inventory",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

json getLoadout(const std::string& sessionToken)
{
    json empty;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/game/loadout",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return empty;
    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("loadout", empty);
    } catch (...) {}
    return empty;
}

bool updateLoadout(const std::string& sessionToken, const json& loadout)
{
    json req;
    req["loadout"] = loadout;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/game/loadout",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

json getTitles(const std::string& sessionToken)
{
    json empty;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("GET", "https://mimita.fun/api/game/titles",
                          "", sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return empty;
    try {
        json j = json::parse(respBody);
        if (j.value("success", false))
            return j.value("titles", empty);
    } catch (...) {}
    return empty;
}

bool updateTitles(const std::string& sessionToken, const json& titles)
{
    json req;
    req["titles"] = titles;
    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/game/titles",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200) return false;
    try {
        json j = json::parse(respBody);
        return j.value("success", false);
    } catch (...) {}
    return false;
}

// ── Game Auth (direct username/email + password login) ───────────────────────

GameAccountLookupResult gameLookupAccount(const std::string& identifier)
{
    GameAccountLookupResult result;
    json req;
    req["identifier"] = identifier;

    std::string respBody;
    int httpCode = 0;
    bool transportOk = httpRequest("POST", "https://mimita.fun/api/game/auth/lookup",
                                    req.dump(), "", respBody, httpCode);
    if (!transportOk)
    {
        result.errorMessage = "Could not reach the account server.";
        return result;
    }
    if (httpCode < 200 || httpCode > 299)
    {
        result.errorMessage = "Account lookup failed.";
        return result;
    }

    try {
        json j = json::parse(respBody);
        result.ok = j.value("ok", false);
        result.exists = j.value("exists", false);
        if (j.contains("account") && j["account"].is_object())
        {
            if (j["account"].contains("id"))
            {
                if (j["account"]["id"].is_number_integer())
                    result.accountId = j["account"]["id"].get<int>();
                else if (j["account"]["id"].is_string())
                    result.accountId = std::atoi(j["account"]["id"].get<std::string>().c_str());
            }
            result.username = j["account"].value("username", "");
        }
    } catch (...) {
        result.ok = false;
        result.errorMessage = "Invalid account lookup response.";
    }
    return result;
}

GameLoginResult gameLogin(const std::string& identifier, const std::string& password,
                          bool rememberMe, const std::string& deviceId,
                          const std::string& deviceName, const std::string& platform,
                          const std::string& clientBuild)
{
    GameLoginResult result;
    json req;
    req["identifier"] = identifier;
    req["password"] = password;
    req["remember_me"] = rememberMe;
    json device;
    device["device_id"] = deviceId;
    device["device_name"] = deviceName;
    device["platform"] = platform;
    device["client_build"] = clientBuild;
    req["device"] = device;

    std::string respBody;
    int httpCode = 0;
    bool transportOk = httpRequest("POST", "https://mimita.fun/api/game/auth/login",
                                    req.dump(), "", respBody, httpCode);

    if (!transportOk)
    {
        result.errorCode = "NETWORK_UNAVAILABLE";
        result.errorMessage = "Could not reach the account server.";
        return result;
    }

    if (httpCode < 200 || httpCode > 299)
    {
        try {
            json j = json::parse(respBody);
            result.errorCode = j["error"].value("code", "AUTH_SERVER_UNAVAILABLE");
            result.errorMessage = j["error"].value("message", "Authentication server error.");
        } catch (...) {
            result.errorCode = "AUTH_SERVER_UNAVAILABLE";
            result.errorMessage = "Unexpected response from server.";
        }
        return result;
    }

    try {
        json j = json::parse(respBody);
        if (!j.value("ok", false)) return result;
        auto acct = j["account"];
        auto ses = j["session"];
        result.ok = true;
        if (acct.contains("id"))
        {
            if (acct["id"].is_number_integer())
                result.accountId = acct["id"].get<int>();
            else if (acct["id"].is_string())
                result.accountId = std::atoi(acct["id"].get<std::string>().c_str());
        }
        result.username = acct.value("username", "");
        result.supporterTier = acct.value("supporter_tier", "free");
        result.vipAppearance = parseVipAppearanceJson(acct, result.supporterTier, acct.value("role", "user"));
        result.vipStyleDetail = parseVipStyleDetailJson(acct, result.supporterTier, acct.value("role", "user"));
        result.supporterTier = MimitaVip::tierToString(result.vipAppearance.tier);
        for (const auto& p : acct.value("permissions", json::array()))
            result.permissions.push_back(p.get<std::string>());
        result.accessToken = ses.value("access_token", "");
        result.accessExpiresAt = ses.value("access_expires_at", "");
        result.refreshToken = ses.value("refresh_token", "");
        result.refreshExpiresAt = ses.value("refresh_expires_at", "");
    } catch (const std::exception& e) {
        result.ok = false;
        result.errorCode = "AUTH_INVALID_RESPONSE";
        result.errorMessage = "Failed to parse server response.";
    }
    return result;
}

GameRefreshResult gameRefresh(const std::string& refreshToken, const std::string& deviceId)
{
    GameRefreshResult result;
    json req;
    req["refresh_token"] = refreshToken;
    if (!deviceId.empty()) req["device_id"] = deviceId;

    std::string respBody;
    int httpCode = 0;
    bool transportOk = httpRequest("POST", "https://mimita.fun/api/game/auth/refresh",
                                    req.dump(), "", respBody, httpCode);

    if (!transportOk)
    {
        result.errorCode = "NETWORK_UNAVAILABLE";
        return result;
    }

    if (httpCode < 200 || httpCode > 299)
    {
        try {
            json j = json::parse(respBody);
            result.errorCode = j["error"].value("code", "SESSION_EXPIRED");
            result.errorMessage = j["error"].value("message", "Session refresh failed.");
        } catch (...) {
            result.errorCode = "SESSION_EXPIRED";
        }
        return result;
    }

    try {
        json j = json::parse(respBody);
        if (!j.value("ok", false)) return result;
        result.ok = true;
        result.accessToken = j.value("access_token", "");
        result.accessExpiresAt = j.value("access_expires_at", "");
        result.refreshToken = j.value("refresh_token", refreshToken);
        result.refreshExpiresAt = j.value("refresh_expires_at", "");
        if (j.contains("account") && j["account"].is_object())
        {
            const json& acct = j["account"];
            result.vipAppearance = parseVipAppearanceJson(
                acct, acct.value("supporter_tier", "free"), acct.value("role", "user"));
            result.vipStyleDetail = parseVipStyleDetailJson(
                acct, acct.value("supporter_tier", "free"), acct.value("role", "user"));
        }
    } catch (...) {
        result.errorCode = "AUTH_INVALID_RESPONSE";
    }
    return result;
}

GameLogoutResult gameLogout(const std::string& refreshToken, const std::string& deviceId)
{
    GameLogoutResult result;
    json req;
    req["refresh_token"] = refreshToken;
    if (!deviceId.empty()) req["device_id"] = deviceId;

    std::string respBody;
    int httpCode = 0;
    httpRequest("POST", "https://mimita.fun/api/game/auth/logout",
                req.dump(), "", respBody, httpCode);

    result.ok = (httpCode >= 200 && httpCode < 300);
    return result;
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
            result.vipAppearance = parseVipAppearanceJson(j, result.supporterTier, j.value("role", "user"));
            result.vipStyleDetail = parseVipStyleDetailJson(j, result.supporterTier, j.value("role", "user"));
            result.supporterTier = MimitaVip::tierToString(result.vipAppearance.tier);
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
            result.displayName = account.value("display_name", result.username);
            result.supporterTier = account.value("supporter_tier", "free");
            result.vipAppearance = parseVipAppearanceJson(account, result.supporterTier, account.value("role", "user"));
            result.vipStyleDetail = parseVipStyleDetailJson(account, result.supporterTier, account.value("role", "user"));
            result.supporterTier = MimitaVip::tierToString(result.vipAppearance.tier);
        }
    } catch (...) {}
    return result;
}
std::string requestVipJoinTicket(const std::string& sessionToken,
                                 const std::string& roomCode,
                                 const std::string& joinToken)
{
    if (sessionToken.empty())
        return "";

    json req;
    req["room_code"] = roomCode;
    (void)joinToken;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/vip/join-ticket",
                          req.dump(), sessionToken, respBody, httpCode);
    if (!ok || httpCode != 200)
        return "";

    try {
        json j = json::parse(respBody);
        if (!j.value("success", false))
            return "";
        return j.value("join_ticket", "");
    } catch (...) {}
    return "";
}

VipJoinTicketResult verifyVipJoinTicket(const std::string& joinTicket,
                                        const std::string& roomCode,
                                        const std::string& joinToken)
{
    VipJoinTicketResult result;
    if (joinTicket.empty())
    {
        result.reason = "missing_ticket";
        return result;
    }

    json req;
    req["join_ticket"] = joinTicket;
    req["room_code"] = roomCode;
    (void)joinToken;

    std::string respBody;
    int httpCode = 0;
    bool ok = httpRequest("POST", "https://mimita.fun/api/vip/verify-join-ticket",
                          req.dump(), "", respBody, httpCode);
    result.ok = ok && httpCode == 200;
    if (!result.ok)
    {
        result.reason = "website_unavailable";
        return result;
    }

    try {
        json j = json::parse(respBody);
        if (!j.value("success", false))
        {
            result.reason = "verify_rejected";
            return result;
        }
        result.verified = j.value("verified", false);
        result.reason = j.value("reason", "");
        result.accountId = j.value("account_id", 0);
        result.username = j.value("username", "");
        result.displayName = j.value("display_name", result.username);
        result.role = j.value("role", "user");
        if (j.contains("vip") && j["vip"].is_object())
        {
            result.supporterTier = j["vip"].value("active_tier", "free");
            json owner;
            owner["supporter_tier"] = result.supporterTier;
            owner["role"] = result.role;
            owner["vip"] = j["vip"];
            result.vipAppearance = parseVipAppearanceJson(owner, result.supporterTier, result.role);
            result.vipStyleDetail = parseVipStyleDetailJson(owner, result.supporterTier, result.role);
            result.supporterTier = MimitaVip::tierToString(result.vipAppearance.tier);
        }
    } catch (...) {
        result.ok = false;
        result.verified = false;
        result.reason = "invalid_response";
    }
    return result;
}
