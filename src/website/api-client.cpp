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
        info.valid = true;
        info.id = j.value("id", 0);
        info.username = j.value("username", "");
        info.avatarUrl = j.value("avatar_url", "");
    } catch (...) {}
    return info;
}
