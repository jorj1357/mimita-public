// MimitaLauncher — auto-updater and game launcher
// Self-update, download resume, crash reporting.

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

std::string appExeName()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path = buf;
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) return path.substr(pos + 1);
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

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port;
    bool secure;
};

bool parseUrl(const std::string& url, UrlParts& out)
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

    out.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    out.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    out.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (out.path.empty()) out.path = L"/";
    out.port = parts.nPort;
    return true;
}

HINTERNET winHttpOpen()
{
    return WinHttpOpen(L"MimitaLauncher/2.0",
                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                       WINHTTP_NO_PROXY_NAME,
                       WINHTTP_NO_PROXY_BYPASS, 0);
}

HINTERNET winHttpConnect(HINTERNET session, const UrlParts& url)
{
    return WinHttpConnect(session, url.host.c_str(), url.port, 0);
}

HINTERNET winHttpRequest(HINTERNET connect, const UrlParts& url, LPCWSTR method,
                         LPCWSTR extraHeaders = nullptr)
{
    return WinHttpOpenRequest(connect, method, url.path.c_str(), nullptr,
                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                              url.secure ? WINHTTP_FLAG_SECURE : 0);
}

// GET request returning response body as string
bool httpGET(const std::string& url, std::string& out)
{
    UrlParts parts;
    if (!parseUrl(url, parts)) return false;

    HINTERNET s = winHttpOpen();
    if (!s) return false;
    HINTERNET c = winHttpConnect(s, parts);
    if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = winHttpRequest(c, parts, L"GET");
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

// Download file with resume support. Returns true if full file was obtained.
// Checks for .part file, sends Range header to resume.
bool downloadFile(const std::string& url, const std::string& dest)
{
    std::string partPath = dest + ".part";
    DWORD existingSize = 0;

    // Check for partial download
    HANDLE partFile = CreateFileA(partPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (partFile != INVALID_HANDLE_VALUE) {
        existingSize = GetFileSize(partFile, nullptr);
        SetFilePointer(partFile, 0, nullptr, FILE_END);
    }

    UrlParts up;
    if (!parseUrl(url, up)) {
        if (partFile != INVALID_HANDLE_VALUE) CloseHandle(partFile);
        return false;
    }

    HINTERNET s = winHttpOpen();
    if (!s) { if (partFile != INVALID_HANDLE_VALUE) CloseHandle(partFile); return false; }
    HINTERNET c = winHttpConnect(s, up);
    if (!c) { WinHttpCloseHandle(s); if (partFile != INVALID_HANDLE_VALUE) CloseHandle(partFile); return false; }

    // Add Range header if we have a partial download
    std::wstring rangeHeader;
    if (existingSize > 0) {
        wchar_t buf[64];
        swprintf(buf, 64, L"Range: bytes=%lu-\r\n", existingSize);
        rangeHeader = buf;
    }

    HINTERNET r = winHttpRequest(c, up, L"GET", rangeHeader.empty() ? nullptr : rangeHeader.c_str());
    if (!r) {
        if (partFile != INVALID_HANDLE_VALUE) CloseHandle(partFile);
        WinHttpCloseHandle(c); WinHttpCloseHandle(s);
        return false;
    }

    BOOL ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);

    DWORD status = 0;
    if (ok) {
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        // Accept 200 (full), 206 (partial content)
        ok = (status == 200 || status == 206);
    }

    if (ok && status == 200 && existingSize > 0) {
        // Server doesn't support Range — restart download
        existingSize = 0;
        SetFilePointer(partFile, 0, nullptr, FILE_BEGIN);
        SetEndOfFile(partFile);
    }

    if (ok) {
        char tmp[65536];
        DWORD read = 0;
        do {
            if (!WinHttpReadData(r, tmp, sizeof(tmp), &read)) break;
            if (read > 0) {
                DWORD written = 0;
                if (!WriteFile(partFile, tmp, read, &written, nullptr) || written != read) {
                    ok = FALSE;
                    break;
                }
            }
        } while (read > 0);
    }

    WinHttpCloseHandle(r);
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);

    if (partFile != INVALID_HANDLE_VALUE) {
        CloseHandle(partFile);
        partFile = INVALID_HANDLE_VALUE;
    }

    if (ok) {
        // Rename .part to final name
        DeleteFileA(dest.c_str());
        MoveFileA(partPath.c_str(), dest.c_str());
        return true;
    }

    return false;
}

// POST JSON body to URL
bool httpPOST(const std::string& url, const std::string& body)
{
    UrlParts up;
    if (!parseUrl(url, up)) return false;

    HINTERNET s = winHttpOpen();
    if (!s) return false;
    HINTERNET c = winHttpConnect(s, up);
    if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = winHttpRequest(c, up, L"POST");
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(r, headers, (DWORD)-1L,
                                 (LPVOID)body.data(), (DWORD)body.size(),
                                 (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);

    DWORD status = 0;
    if (ok) {
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        ok = (status >= 200 && status < 300);
    }

    WinHttpCloseHandle(r);
    WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

// Send crash report for abnormal game exit
void sendCrashReport(const std::string& version, DWORD exitCode, DWORD gameRunMs)
{
    std::string body = "{\"event_name\":\"crash_detected\",\"app_version\":\""
        + version + "\",\"exit_code\":" + std::to_string(exitCode)
        + ",\"uptime_ms\":" + std::to_string(gameRunMs) + "}";
    httpPOST("https://mimita.fun/api/game/analytics/events", body);
}

// Spawn a batch helper that waits, runs installer, deletes installer, and relaunches
bool spawnSelfUpdate(const std::string& installerPath, const std::string& dir)
{
    char tmpDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpDir);

    std::string batchPath = std::string(tmpDir) + "mimita-update.cmd";
    std::string exeName = appExeName();

    HANDLE h = CreateFileA(batchPath.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    // Build batch: wait for launcher to exit, run installer, clean up, relaunch
    std::string batch =
        "@echo off\r\n"
        "ping -n 3 127.0.0.1 > nul\r\n"
        "\"" + installerPath + "\" /VERYSILENT /NORESTART /DIR=\"" + dir + "\"\r\n"
        "del \"" + installerPath + "\"\r\n"
        "start \"\" \"" + dir + "\\" + exeName + "\"\r\n"
        "del \"%~f0\"\r\n";

    DWORD written = 0;
    WriteFile(h, batch.data(), (DWORD)batch.size(), &written, nullptr);
    CloseHandle(h);

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = batchPath.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteExA(&sei);
}

}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::string dir = appDir();
    std::string localVer = readFile(dir + "\\version.txt");
    if (localVer.empty()) localVer = "0.0.0";

    std::string gameExe = dir + "\\mimita.exe";
    bool gameExists = (GetFileAttributesA(gameExe.c_str()) != INVALID_FILE_ATTRIBUTES);

    // Clean up old .old files from previous self-update
    std::string oldLauncher = dir + "\\MimitaLauncher.old.exe";
    DeleteFileA(oldLauncher.c_str());

    // Check for updates
    std::string versionJson;
    if (httpGET("https://mimita.fun/api/game/version", versionJson)) {
        std::string latestVer = extractJsonStr(versionJson, "version");
        if (!latestVer.empty() && latestVer != localVer) {
            char tmpDir[MAX_PATH];
            GetTempPathA(MAX_PATH, tmpDir);
            std::string installerPath = std::string(tmpDir)
                + "MimitaSetup-" + latestVer + ".exe";

            // Download with resume support (.part files, Range headers)
            if (downloadFile("https://mimita.fun/api/download/latest", installerPath)) {
                // Self-update: spawn batch helper and exit
                if (spawnSelfUpdate(installerPath, dir)) {
                    Sleep(500); // brief wait for batch to start
                    return 0;  // exit — batch will run installer and relaunch
                }
            }
        }
    }

    // Launch game and wait (for crash reporting)
    if (!gameExists) {
        MessageBoxA(nullptr, "Mimita not found. Please run MimitaSetup.exe to install.",
                    "Mimita Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD gameStart = GetTickCount();

    if (!CreateProcessA(nullptr, &gameExe[0], nullptr, nullptr, FALSE, 0,
                        nullptr, dir.c_str(), &si, &pi)) {
        return 1;
    }

    // Wait for game to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    DWORD gameRunMs = GetTickCount() - gameStart;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Report crashes (non-zero exit or run time < 30s)
    if (exitCode != 0 && gameRunMs > 1000) {
        sendCrashReport(localVer, exitCode, gameRunMs);
    }

    return 0;
}
