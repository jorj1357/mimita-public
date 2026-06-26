// MimitaLauncher — auto-updater and game launcher
// Minimal Win32 app. No game dependencies. No GLFW. No OpenGL.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

namespace {

std::wstring widen(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

std::string appDir()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path = buf;
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) path = path.substr(0, pos);
    return path;
}

std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return "";
    std::string text, line;
    while (std::getline(f, line)) {
        text += line + "\n";
    }
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.pop_back();
    return text;
}

std::string extractJsonStr(const std::string& json, const std::string& key)
{
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return "";
    k = json.find('"', k + key.size() + 2);
    if (k == std::string::npos) return "";
    auto s = k + 1;
    auto e = json.find('"', s);
    if (e == std::string::npos) return "";
    return json.substr(s, e - s);
}

bool httpGET(const std::string& url, std::string& out)
{
    std::wstring wurl = widen(url);
    URL_COMPONENTSW parts{};
    wchar_t host[256]{}, path[2048]{}, extra[2048]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = sizeof(extra) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
        return false;

    bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring h(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring p(parts.lpszUrlPath, parts.dwUrlPathLength);
    p.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (p.empty()) p = L"/";

    HINTERNET s = WinHttpOpen(L"MimitaLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;

    HINTERNET c = WinHttpConnect(s, h.c_str(), parts.nPort, 0);
    if (!c) { WinHttpCloseHandle(s); return false; }

    HINTERNET r = WinHttpOpenRequest(c, L"GET", p.c_str(), nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     secure ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }

    BOOL ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);

    if (ok) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        ok = (status >= 200 && status < 300);
    }

    if (ok) {
        std::vector<char> buf;
        DWORD read = 0;
        do {
            char tmp[4096];
            if (!WinHttpReadData(r, tmp, sizeof(tmp), &read)) break;
            buf.insert(buf.end(), tmp, tmp + read);
        } while (read > 0);
        out.assign(buf.data(), buf.size());
    }

    WinHttpCloseHandle(r);
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

bool downloadFile(const std::string& url, const std::string& dest)
{
    std::wstring wurl = widen(url);
    std::wstring wdest = widen(dest);

    URL_COMPONENTSW parts{};
    wchar_t host[256]{}, path[2048]{}, extra[2048]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = sizeof(extra) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
        return false;

    bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring h(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring p(parts.lpszUrlPath, parts.dwUrlPathLength);
    p.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (p.empty()) p = L"/";

    HINTERNET s = WinHttpOpen(L"MimitaLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;

    HINTERNET c = WinHttpConnect(s, h.c_str(), parts.nPort, 0);
    if (!c) { WinHttpCloseHandle(s); return false; }

    HINTERNET r = WinHttpOpenRequest(c, L"GET", p.c_str(), nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     secure ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }

    BOOL ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);

    if (ok) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        ok = (status >= 200 && status < 300);
    }

    if (ok) {
        HANDLE f = CreateFileW(wdest.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
            return false;
        }
        DWORD read = 0;
        do {
            char tmp[65536];
            if (!WinHttpReadData(r, tmp, sizeof(tmp), &read)) break;
            if (read > 0) {
                DWORD written = 0;
                if (!WriteFile(f, tmp, read, &written, nullptr) || written != read) {
                    ok = FALSE;
                    break;
                }
            }
        } while (read > 0);
        CloseHandle(f);
    }

    WinHttpCloseHandle(r);
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::string dir = appDir();
    std::string localVer = readFile(dir + "\\version.txt");
    if (localVer.empty()) localVer = "0.0.0";

    std::string gameExe = dir + "\\mimita.exe";
    bool gameExists = (GetFileAttributesA(gameExe.c_str()) != INVALID_FILE_ATTRIBUTES);

    std::string versionJson;
    if (httpGET("https://mimita.fun/api/game/version", versionJson)) {
        std::string latestVer = extractJsonStr(versionJson, "version");
        if (!latestVer.empty() && latestVer != localVer && gameExists) {
            char tmpDir[MAX_PATH];
            GetTempPathA(MAX_PATH, tmpDir);
            std::string installerPath = std::string(tmpDir) + "MimitaSetup-" + latestVer + ".exe";

            if (downloadFile("https://mimita.fun/api/download/latest", installerPath)) {
                std::string cmd = "\"" + installerPath + "\" /VERYSILENT /NORESTART /DIR=\"" + dir + "\"";
                STARTUPINFOA si = { sizeof(si) };
                PROCESS_INFORMATION pi;
                if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
                DeleteFileA(installerPath.c_str());
            }
        }
    }

    if (gameExists) {
        ShellExecuteA(nullptr, "open", gameExe.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
    } else {
        MessageBoxA(nullptr, "Mimita not found. Please run MimitaSetup.exe to install the game.",
                    "Mimita Launcher", MB_OK | MB_ICONERROR);
    }

    return 0;
}
