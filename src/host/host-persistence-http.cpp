// Resident persistence HTTP transport.
//
// The game generation owns the strong api-client implementation. The weak copy
// below keeps the strict cold EXE capable of submitting progression batches.

#include "website/api-client.h"

#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

std::wstring widen(const std::string& text) {
    if (text.empty()) return L"";
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), count);
    return result;
}

bool postJson(const std::string& url, const std::string& body,
              const std::string& token, std::string& response) {
    response.clear();
    const std::wstring wideUrl = widen(url);
    URL_COMPONENTSW parts{};
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[2048]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) return false;
    std::wstring requestPath(path, parts.dwUrlPathLength);
    requestPath.append(extra, parts.dwExtraInfoLength);
    if (requestPath.empty()) requestPath = L"/";

    HINTERNET session = WinHttpOpen(L"MimitaHost/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", requestPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!token.empty()) {
        headers += L"Authorization: Bearer " + widen(token) + L"\r\n";
    }
    BOOL ok = WinHttpSendRequest(request, headers.c_str(),
        static_cast<DWORD>(-1L), const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (ok) ok = WinHttpReceiveResponse(request, nullptr);
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (ok) {
        WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
            WINHTTP_NO_HEADER_INDEX);
    }
    if (ok) {
        DWORD read = 0;
        char chunk[4096];
        do {
            if (!WinHttpReadData(request, chunk, sizeof(chunk), &read)) break;
            response.append(chunk, read);
        } while (read > 0);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok && status == 200;
}

}  // namespace

__attribute__((weak)) json submitPersistenceBatch(
    const std::string& sessionToken, const std::string& bodyJson,
    bool registerSession) {
    std::string response;
    const char* endpoint = registerSession
        ? "https://mimita.fun/api/progression/session"
        : "https://mimita.fun/api/progression/batch";
    if (!postJson(endpoint, bodyJson, sessionToken, response)) {
        return json::object();
    }
    try {
        return json::parse(response);
    } catch (...) {
        return json::object();
    }
}
