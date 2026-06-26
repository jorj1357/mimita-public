// MimitaLauncher v2 — differential updates, SHA-256 verify, minidump

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "dbghelp.lib")

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
    auto p = path.find_last_of("\\/");
    return (p != std::string::npos) ? path.substr(0, p) : path;
}

std::string appExeName()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path = buf;
    auto p = path.find_last_of("\\/");
    return (p != std::string::npos) ? path.substr(p + 1) : path;
}

std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return "";
    std::string text, line;
    while (std::getline(f, line)) text += line + "\n";
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.pop_back();
    return text;
}

void writeFile(const std::string& path, const std::string& content)
{
    std::ofstream f(path);
    f << content;
}

std::string extractJsonStr(const std::string& json, const std::string& key)
{
    auto k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return "";
    k = json.find('"', k + key.size() + 2);
    if (k == std::string::npos) return "";
    auto s = k + 1, e = json.find('"', s);
    return (e == std::string::npos) ? "" : json.substr(s, e - s);
}

struct UrlParts {
    std::wstring host, path;
    INTERNET_PORT port;
    bool secure;
};

bool parseUrl(const std::string& url, UrlParts& u)
{
    std::wstring w = widen(url);
    URL_COMPONENTSW p{};
    wchar_t h[256], pa[2048], e[2048];
    p.dwStructSize = sizeof(p);
    p.lpszHostName = h; p.dwHostNameLength = 256;
    p.lpszUrlPath = pa; p.dwUrlPathLength = 2048;
    p.lpszExtraInfo = e; p.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &p)) return false;
    u.secure = p.nScheme == INTERNET_SCHEME_HTTPS;
    u.host.assign(p.lpszHostName, p.dwHostNameLength);
    u.path.assign(p.lpszUrlPath, p.dwUrlPathLength);
    u.path.append(p.lpszExtraInfo, p.dwExtraInfoLength);
    if (u.path.empty()) u.path = L"/";
    u.port = p.nPort;
    return true;
}

HINTERNET hOpen() { return WinHttpOpen(L"MimitaLauncher/3.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS, 0); }
HINTERNET hConnect(HINTERNET s, const UrlParts& u) { return WinHttpConnect(s, u.host.c_str(), u.port, 0); }
HINTERNET hRequest(HINTERNET c, const UrlParts& u, LPCWSTR method, LPCWSTR hdrs = nullptr) {
    return WinHttpOpenRequest(c, method, u.path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        u.secure ? WINHTTP_FLAG_SECURE : 0);
}

bool httpGET(const std::string& url, std::string& out)
{
    UrlParts u;
    if (!parseUrl(url, u)) return false;
    HINTERNET s = hOpen(); if (!s) return false;
    HINTERNET c = hConnect(s, u); if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = hRequest(c, u, L"GET"); if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    BOOL ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);
    DWORD st = 0, sz = sizeof(st);
    if (ok) { WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &st, &sz, WINHTTP_NO_HEADER_INDEX); ok = (st >= 200 && st < 300); }
    if (ok) {
        std::vector<char> b;
        DWORD rd = 0;
        do { char t[4096]; if (!WinHttpReadData(r, t, sizeof(t), &rd)) break; b.insert(b.end(), t, t + rd); } while (rd > 0);
        out.assign(b.data(), b.size());
    }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return ok;
}

bool downloadFileTo(const std::string& url, const std::string& dest, DWORD resumeAt = 0)
{
    std::string partPath = dest + ".part";
    HANDLE f = CreateFileA(partPath.c_str(), GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    if (resumeAt > 0) SetFilePointer(f, resumeAt, nullptr, FILE_BEGIN);
    else SetFilePointer(f, 0, nullptr, FILE_BEGIN);

    UrlParts u;
    if (!parseUrl(url, u)) { CloseHandle(f); return false; }
    HINTERNET s = hOpen(); if (!s) { CloseHandle(f); return false; }
    HINTERNET c = hConnect(s, u); if (!c) { WinHttpCloseHandle(s); CloseHandle(f); return false; }

    std::wstring rh;
    if (resumeAt > 0) { wchar_t b[64]; swprintf(b, 64, L"Range: bytes=%lu-\r\n", resumeAt); rh = b; }
    HINTERNET r = hRequest(c, u, L"GET", rh.empty() ? nullptr : rh.c_str());
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); CloseHandle(f); return false; }

    BOOL ok = WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);
    DWORD st = 0, sz = sizeof(st);
    if (ok) { WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &st, &sz, WINHTTP_NO_HEADER_INDEX);
        ok = (st == 200 || st == 206); }
    if (ok && st == 200 && resumeAt > 0) { SetFilePointer(f, 0, nullptr, FILE_BEGIN); SetEndOfFile(f); }

    if (ok) {
        char tmp[65536]; DWORD rd = 0;
        do {
            if (!WinHttpReadData(r, tmp, sizeof(tmp), &rd)) break;
            if (rd > 0) { DWORD wr = 0; if (!WriteFile(f, tmp, rd, &wr, nullptr) || wr != rd) { ok = FALSE; break; } }
        } while (rd > 0);
    }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    CloseHandle(f);
    if (ok) { DeleteFileA(dest.c_str()); MoveFileA(partPath.c_str(), dest.c_str()); return true; }
    return false;
}

bool sha256File(const std::string& path, std::string& hex)
{
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;

    BCRYPT_ALG_HANDLE a = nullptr;
    if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) { CloseHandle(f); return false; }
    DWORD hl = 0, dl = 0;
    BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PBYTE)&hl, sizeof(hl), &dl, 0);
    DWORD hl2 = 0;
    BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PBYTE)&hl2, sizeof(hl2), &dl, 0);
    std::vector<BYTE> obj(hl), hash(hl2);
    BCRYPT_HASH_HANDLE hh = nullptr;
    if (BCryptCreateHash(a, &hh, obj.data(), (ULONG)obj.size(), nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(a, 0); CloseHandle(f); return false; }

    char buf[65536]; DWORD rd = 0;
    while (ReadFile(f, buf, sizeof(buf), &rd, nullptr) && rd > 0)
        BCryptHashData(hh, (PBYTE)buf, rd, 0);

    BCryptFinishHash(hh, hash.data(), (ULONG)hash.size(), 0);
    BCryptDestroyHash(hh);
    BCryptCloseAlgorithmProvider(a, 0);
    CloseHandle(f);

    const char* x = "0123456789abcdef";
    for (BYTE b : hash) { hex += x[b >> 4]; hex += x[b & 0xf]; }
    return true;
}

void sendCrashReport(const std::string& ver, DWORD code, DWORD ms)
{
    std::string b = "{\"event_name\":\"crash_detected\",\"app_version\":\""
        + ver + "\",\"exit_code\":" + std::to_string(code)
        + ",\"uptime_ms\":" + std::to_string(ms) + "}";
    UrlParts u;
    if (!parseUrl("https://mimita.fun/api/game/analytics/events", u)) return;
    HINTERNET s = hOpen(); if (!s) return;
    HINTERNET c = hConnect(s, u); if (!c) { WinHttpCloseHandle(s); return; }
    HINTERNET r = hRequest(c, u, L"POST"); if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return; }
    const wchar_t* hdrs = L"Content-Type: application/json\r\n";
    WinHttpSendRequest(r, hdrs, (DWORD)-1L, (LPVOID)b.data(), (DWORD)b.size(), (DWORD)b.size(), 0);
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
}

void writeMinidump(DWORD pid, const std::string& path)
{
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, pid);
    if (!p) return;
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { CloseHandle(p); return; }
    MINIDUMP_EXCEPTION_INFORMATION mei{};
    MiniDumpWriteDump(p, pid, f, MiniDumpNormal, nullptr, nullptr, nullptr);
    CloseHandle(f);
    CloseHandle(p);
}

bool spawnSelfUpdate(const std::string& installerPath, const std::string& dir)
{
    char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
    std::string bp = std::string(td) + "mimita-update.cmd";
    std::string en = appExeName();
    std::string batch =
        "@echo off\r\n"
        "ping -n 3 127.0.0.1 > nul\r\n"
        "\"" + installerPath + "\" /VERYSILENT /NORESTART /DIR=\"" + dir + "\"\r\n"
        "del \"" + installerPath + "\"\r\n"
        "start \"\" \"" + dir + "\\" + en + "\"\r\n"
        "del \"%~f0\"\r\n";
    HANDLE h = CreateFileA(bp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0; WriteFile(h, batch.data(), (DWORD)batch.size(), &wr, nullptr); CloseHandle(h);
    SHELLEXECUTEINFOA si = { sizeof(si) };
    si.lpFile = bp.c_str(); si.nShow = SW_HIDE;
    return ShellExecuteExA(&si);
}

}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::string dir = appDir();
    std::string localVer = readFile(dir + "\\version.txt");
    if (localVer.empty()) localVer = "0.0.0";

    std::string gameExe = dir + "\\mimita.exe";

    // Clean up old files
    DeleteFileA((dir + "\\MimitaLauncher.old.exe").c_str());

    // ── Fetch version + manifest ──────────────────────────────────────────────
    std::string versionJson, manifestJson, latestVer, manifestUrl;
    if (httpGET("https://mimita.fun/api/game/version", versionJson))
        latestVer = extractJsonStr(versionJson, "version");

    if (!latestVer.empty() && latestVer != localVer) {
        // Get manifest URL
        std::string uv;
        if (httpGET("https://mimita.fun/api/update/latest-version", uv))
            manifestUrl = extractJsonStr(uv, "manifest_url");

        if (!manifestUrl.empty()) {
            std::string fullUrl = "https://mimita.fun" + manifestUrl;
            httpGET(fullUrl, manifestJson);
        }
    }

    // ── Differential update ───────────────────────────────────────────────────
    bool updated = false;
    if (!manifestJson.empty() && manifestJson[0] == '{') {
        std::string pendingDir = dir + "\\pending";
        CreateDirectoryA(pendingDir.c_str(), nullptr);
        bool allOk = true;

        // Parse files array — simple scan for "path" and "sha256"
        size_t pos = 0, end = manifestJson.size();
        while ((pos = manifestJson.find("\"path\"", pos)) != std::string::npos) {
            auto ps = manifestJson.find('"', pos + 7);
            if (ps == std::string::npos) break;
            auto pe = manifestJson.find('"', ps + 1);
            if (pe == std::string::npos) break;
            std::string relPath = manifestJson.substr(ps + 1, pe - ps - 1);
            pos = pe;

            // Find sha256
            auto sh = manifestJson.find("\"sha256\"", pos);
            if (sh == std::string::npos) break;
            auto ss = manifestJson.find('"', sh + 9);
            if (ss == std::string::npos) break;
            auto se = manifestJson.find('"', ss + 1);
            if (se == std::string::npos) break;
            std::string wantHash = manifestJson.substr(ss + 1, se - ss - 1);
            pos = se;

            std::string localPath = dir + "\\" + relPath;
            std::string localHash;
            bool needDownload = true;

            if (sha256File(localPath, localHash) && localHash == wantHash)
                needDownload = false;

            if (needDownload) {
                std::string dlUrl = "https://mimita.fun/api/download/file/" + relPath;
                std::string destPath = pendingDir + "\\" + relPath;

                // Ensure subdirectory exists
                auto slash = destPath.find_last_of("\\");
                if (slash != std::string::npos) {
                    std::string sub = destPath.substr(0, slash);
                    CreateDirectoryA(sub.c_str(), nullptr);
                }

                // Resume partial download
                DWORD existing = 0;
                {
                    HANDLE pf = CreateFileA((destPath + ".part").c_str(), GENERIC_READ,
                        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (pf != INVALID_HANDLE_VALUE) {
                        existing = GetFileSize(pf, nullptr);
                        CloseHandle(pf);
                    }
                }

                std::string remoteUrl = dlUrl;
                if (!downloadFileTo(remoteUrl, destPath, existing)) {
                    allOk = false;
                    break;
                }

                // Verify downloaded file
                std::string dlHash;
                if (!sha256File(destPath, dlHash) || dlHash != wantHash) {
                    allOk = false;
                    break;
                }
            }
        }

        if (allOk) {
            // Walk pending dir tree and copy over originals
            std::vector<std::string> dirs = { "" };
            while (!dirs.empty()) {
                std::string prefix = dirs.back(); dirs.pop_back();
                std::string search = pendingDir + "\\" + prefix + "*";
                WIN32_FIND_DATAA fd;
                HANDLE ff = FindFirstFileA(search.c_str(), &fd);
                if (ff == INVALID_HANDLE_VALUE) continue;
                do {
                    std::string name = fd.cFileName;
                    if (name == "." || name == "..") continue;
                    std::string rel = prefix + name;
                    std::string src = pendingDir + "\\" + rel;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        dirs.push_back(rel + "\\");
                    } else {
                        std::string dst = dir + "\\" + rel;
                        CreateDirectoryA(dst.substr(0, dst.find_last_of("\\")).c_str(), nullptr);
                        CopyFileA(src.c_str(), dst.c_str(), FALSE);
                    }
                } while (FindNextFileA(ff, &fd));
                FindClose(ff);
            }

            // Clean up pending
            {
                std::vector<std::string> dirs2 = { "" };
                while (!dirs2.empty()) {
                    std::string p = dirs2.back(); dirs2.pop_back();
                    std::string s = pendingDir + "\\" + p + "*";
                    WIN32_FIND_DATAA fd;
                    HANDLE ff = FindFirstFileA(s.c_str(), &fd);
                    if (ff == INVALID_HANDLE_VALUE) continue;
                    do {
                        std::string n = fd.cFileName;
                        if (n == "." || n == "..") continue;
                        std::string r = p + n;
                        std::string fp = pendingDir + "\\" + r;
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            dirs2.push_back(r + "\\");
                        } else {
                            DeleteFileA(fp.c_str());
                        }
                    } while (FindNextFileA(ff, &fd));
                    FindClose(ff);
                }
                RemoveDirectoryA(pendingDir.c_str());
            }

            // Update version
            writeFile(dir + "\\version.txt", latestVer);
            updated = true;
        }
    }

    // ── Full installer fallback (with SHA-256 verify) ──────────────────────
    if (!updated && !latestVer.empty() && latestVer != localVer) {
        std::string wantHash = extractJsonStr(versionJson, "installer_sha256");
        char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
        std::string ip = std::string(td) + "MimitaSetup-" + latestVer + ".exe";

        if (downloadFileTo("https://mimita.fun/api/download/latest", ip, 0)) {
            std::string dlHash;
            bool ok = true;
            if (!wantHash.empty()) {
                if (sha256File(ip, dlHash) && dlHash == wantHash) {
                    // Hash matches — proceed
                } else {
                    ok = false; // hash mismatch or missing, don't run
                }
            }
            if (ok) {
                if (spawnSelfUpdate(ip, dir)) { Sleep(500); return 0; }
            }
        }
    }

    // ── Launch game ──────────────────────────────────────────────────────────
    if (GetFileAttributesA(gameExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(nullptr, "Mimita not found. Run MimitaSetup.exe to install.",
                    "Mimita Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD start = GetTickCount();

    if (!CreateProcessA(nullptr, &gameExe[0], nullptr, nullptr, FALSE, 0,
                        nullptr, dir.c_str(), &si, &pi))
        return 1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    DWORD elapsed = GetTickCount() - start;

    if (exitCode != 0 && elapsed > 1000) {
        // Write minidump
        std::string dumpPath = dir + "\\crash-" + std::to_string(GetTickCount()) + ".dmp";
        writeMinidump(pi.dwProcessId, dumpPath);
        sendCrashReport(localVer, exitCode, elapsed);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
