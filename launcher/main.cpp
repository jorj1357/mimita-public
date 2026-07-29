// MimitaLauncher v2 — Self-extracting, GitHub Release bootstrap

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "dbghelp.lib")

namespace {

const char* PAK_MAGIC = "MIMIPAK!";

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

std::string selfPath()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return buf;
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

bool httpGETWithHeaders(const std::string& url, std::string& out, const std::wstring& extraHeaders = L"")
{
    UrlParts u;
    if (!parseUrl(url, u)) return false;
    HINTERNET s = hOpen(); if (!s) return false;
    HINTERNET c = hConnect(s, u); if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = hRequest(c, u, L"GET"); if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    BOOL ok = WinHttpSendRequest(r,
        extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
        extraHeaders.empty() ? 0 : (DWORD)-1L,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
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

bool httpGET(const std::string& url, std::string& out)
{
    return httpGETWithHeaders(url, out, L"");
}

bool httpPOST(const std::string& url, const std::string& body, std::string& out)
{
    UrlParts u;
    if (!parseUrl(url, u)) return false;
    HINTERNET s = hOpen(); if (!s) return false;
    HINTERNET c = hConnect(s, u); if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = hRequest(c, u, L"POST"); if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    std::wstring hdrs = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(r, hdrs.c_str(), (DWORD)-1L,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);
    DWORD st = 0, sz = sizeof(st);
    if (ok) { WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &st, &sz, WINHTTP_NO_HEADER_INDEX); ok = (st >= 200 && st < 300); }
    if (ok) {
        std::vector<char> b;
        DWORD rd = 0;
        do { char t[4096]; if (!WinHttpReadData(r, t, sizeof(t), &rd)) break;         b.insert(b.end(), b.data(), b.data() + rd); } while (rd > 0);
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

// ── ZIP extraction via PowerShell Expand-Archive ─────────────────────────
bool extractZipFile(const std::string& zipPath, const std::string& destDir)
{
    CreateDirectoryA(destDir.c_str(), nullptr);
    std::string cmd = "powershell -Command \"Expand-Archive -Path '"
        + zipPath + "' -DestinationPath '" + destDir + "' -Force\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

// ── Embedded ZIP extraction ──────────────────────────────────────────────
// Layout: [EXE][ZIP data][ZIP size:4][MIMIZIP! magic:8]
// The last 12 bytes are always [zip_size][MIMIZIP!]
const char* ZIP_MAGIC = "MIMIZIP!";

bool hasEmbeddedZip()
{
    HANDLE f = CreateFileA(selfPath().c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    SetFilePointer(f, -(LONG)8, nullptr, FILE_END); // last 8 bytes = magic
    char magic[8];
    DWORD rd = 0;
    bool found = ReadFile(f, magic, 8, &rd, nullptr) && rd == 8 && memcmp(magic, ZIP_MAGIC, 8) == 0;
    CloseHandle(f);
    return found;
}

bool extractEmbeddedZip(const std::string& destDir)
{
    CreateDirectoryA(destDir.c_str(), nullptr);

    HANDLE f = CreateFileA(selfPath().c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;

    // Read magic from last 8 bytes
    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)8;
    SetFilePointerEx(f, li, nullptr, FILE_END);
    char magic[8];
    DWORD rd = 0;
    if (!ReadFile(f, magic, 8, &rd, nullptr) || rd != 8 || memcmp(magic, ZIP_MAGIC, 8) != 0) {
        CloseHandle(f); return false;
    }

    // Read ZIP size from 4 bytes before magic
    li.QuadPart = -(LONGLONG)(8 + 4);
    SetFilePointerEx(f, li, nullptr, FILE_END);
    uint32_t zipSize = 0;
    if (!ReadFile(f, &zipSize, 4, &rd, nullptr) || rd != 4 || zipSize == 0) {
        CloseHandle(f); return false;
    }

    // Seek to start of ZIP data (zip_size + 12 bytes from end)
    li.QuadPart = -(LONGLONG)(zipSize + (uint64_t)12);
    SetFilePointerEx(f, li, nullptr, FILE_END);

    // Write ZIP to temp file
    char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
    std::string tmpZip = std::string(td) + "mimita-extract.zip";
    HANDLE wf = CreateFileA(tmpZip.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (wf == INVALID_HANDLE_VALUE) { CloseHandle(f); return false; }

    char buf[65536];
    uint32_t remaining = zipSize;
    while (remaining > 0) {
        DWORD toRead = (remaining > sizeof(buf)) ? (DWORD)sizeof(buf) : remaining;
        DWORD r = 0;
        if (!ReadFile(f, buf, toRead, &r, nullptr) || r == 0) break;
        DWORD wr = 0;
        WriteFile(wf, buf, r, &wr, nullptr);
        remaining -= r;
    }
    CloseHandle(wf);
    CloseHandle(f);

    bool success = extractZipFile(tmpZip, destDir);
    DeleteFileA(tmpZip.c_str());
    return success;
}

// ── GitHub API: get latest release info ───────────────────────────────────
// Returns version, ZIP download URL, ZIP SHA-256 from release body
bool getGitHubReleaseInfo(std::string& outVersion, std::string& outZipUrl, std::string& outSha256)
{
    std::string json;
    if (!httpGETWithHeaders("https://api.github.com/repos/jorj1357/mimita-public/releases/latest",
                            json, L"Accept: application/vnd.github+json\r\n"))
        return false;

    std::string tag = extractJsonStr(json, "tag_name");
    if (tag.empty()) return false;
    if (tag.size() > 1 && tag[0] == 'v') tag = tag.substr(1);
    outVersion = tag;

    // Find the mimita-game.zip asset
    std::string searchKey = "\"name\":\"mimita-game.zip\"";
    auto assetPos = json.find(searchKey);
    if (assetPos == std::string::npos) return false;

    auto urlKey = json.rfind("\"browser_download_url\"", assetPos);
    if (urlKey == std::string::npos || urlKey > json.find('}', assetPos))
        urlKey = json.find("\"browser_download_url\"", assetPos);
    if (urlKey == std::string::npos) return false;

    auto us = json.find('"', urlKey + 21);
    if (us == std::string::npos) return false;
    auto ue = json.find('"', us + 1);
    if (ue == std::string::npos) return false;
    outZipUrl = json.substr(us + 1, ue - us - 1);

    std::string body = extractJsonStr(json, "body");
    if (!body.empty()) {
        auto sh = body.find("SHA-256:");
        if (sh != std::string::npos) {
            auto bt = body.find('`', sh);
            if (bt != std::string::npos) {
                auto be = body.find('`', bt + 1);
                if (be != std::string::npos) {
                    std::string hash = body.substr(bt + 1, be - bt - 1);
                    if (hash.size() == 64) {
                        bool valid = true;
                        for (char c : hash) {
                            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                                { valid = false; break; }
                        }
                        if (valid) outSha256 = hash;
                    }
                }
            }
        }
    }
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

bool spawnSelfUpdate(const std::string& newLauncherPath, const std::string& dir)
{
    char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
    std::string bp = std::string(td) + "mimita-update.cmd";
    std::string en = appExeName();
    std::string batch =
        "@echo off\r\n"
        "ping -n 3 127.0.0.1 > nul\r\n"
        "copy /Y \"" + newLauncherPath + "\" \"" + dir + "\\" + en + "\"\r\n"
        "del \"" + newLauncherPath + "\"\r\n"
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

void storeSessionToken(const std::string& dir, const std::string& token)
{
    std::string configDir = dir + "\\config";
    CreateDirectoryA(configDir.c_str(), nullptr);
    writeFile(configDir + "\\auth-token.json", "{\"session_token\":\"" + token + "\"}\n");
}

std::string urlDecode(const std::string& input)
{
    std::string out;
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '%' && i + 2 < input.size())
        {
            char hi = input[i + 1];
            char lo = input[i + 2];
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            out += (char)((hex(hi) << 4) | hex(lo));
            i += 2;
        }
        else
        {
            out += input[i];
        }
    }
    return out;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    std::string cmdLine = lpCmdLine ? lpCmdLine : "";
    std::string dir = appDir();
    std::string localVer = readFile(dir + "\\version.txt");
    if (localVer.empty()) localVer = "0.0.0";

    std::string gameExe = dir + "\\mimita.exe";
    bool gameExists = GetFileAttributesA(gameExe.c_str()) != INVALID_FILE_ATTRIBUTES;

    DeleteFileA((dir + "\\MimitaLauncher.old.exe").c_str());

    // ── Handle mimita:// protocol ─────────────────────────────────────────────
    if (cmdLine.find("mimita://") != std::string::npos)
    {
        auto qpos = cmdLine.find('?');
        if (qpos != std::string::npos)
        {
            std::string query = cmdLine.substr(qpos + 1);
            auto tpos = query.find("token=");
            if (tpos != std::string::npos)
            {
                tpos += 6;
                auto tend = query.find('&', tpos);
                if (tend == std::string::npos) tend = query.find(' ', tpos);
                if (tend == std::string::npos) tend = query.size();
                std::string exchangeToken = urlDecode(query.substr(tpos, tend - tpos));

                std::string reqBody = "{\"exchange_token\":\"" + exchangeToken + "\"}";
                std::string respBody;
                if (httpPOST("https://mimita.fun/api/auth/exchange-session", reqBody, respBody))
                {
                    std::string sessionToken = extractJsonStr(respBody, "session_token");
                    if (!sessionToken.empty())
                    {
                        storeSessionToken(dir, sessionToken);
                        if (gameExists) {
                            std::string cli = gameExe + " --session \"" + sessionToken + "\"";
                            STARTUPINFOA si = { sizeof(si) };
                            PROCESS_INFORMATION pi;
                            if (CreateProcessA(nullptr, &cli[0], nullptr, nullptr, FALSE, 0,
                                                nullptr, dir.c_str(), &si, &pi))
                            {
                                WaitForSingleObject(pi.hProcess, INFINITE);
                                CloseHandle(pi.hProcess);
                                CloseHandle(pi.hThread);
                            }
                        }
                        return 0;
                    }
                }
                storeSessionToken(dir, exchangeToken);
            }
        }
        return 0;
    }

    // ── Self-extract on first run (embedded ZIP) ──────────────────────────────
    if (!gameExists && hasEmbeddedZip())
    {
        if (extractEmbeddedZip(dir))
        {
            // Write version from our own version tag (read from GitHub or fallback)
            // We know our tag from self knowledge, or just write a placeholder
            // The launcher's own version.txt is part of the extraction
            gameExists = GetFileAttributesA(gameExe.c_str()) != INVALID_FILE_ATTRIBUTES;
        }
    }

    // ── Check GitHub for updates ──────────────────────────────────────────────
    std::string latestVer, zipUrl, zipSha256;
    bool gotLatest = getGitHubReleaseInfo(latestVer, zipUrl, zipSha256);

    // ── If newer version on GitHub, download and self-update ──────────────────
    if (gotLatest && gameExists && latestVer != localVer)
    {
        char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
        std::string zipPath = std::string(td) + "mimita-game-" + latestVer + ".zip";

        if (downloadFileTo(zipUrl, zipPath, 0))
        {
            bool hashOk = true;
            if (!zipSha256.empty()) {
                std::string dlHash;
                if (sha256File(zipPath, dlHash) && dlHash == zipSha256) {
                    // Hash matches
                } else {
                    hashOk = false;
                }
            }

            if (hashOk) {
                if (extractZipFile(zipPath, dir)) {
                    writeFile(dir + "\\version.txt", latestVer);
                    DeleteFileA(zipPath.c_str());
                }
            }
        }
    }

    // ── Launch game ──────────────────────────────────────────────────────────
    if (!gameExists) {
        MessageBoxA(nullptr,
            "Mimita is not installed. The launcher will install it when online.",
            "Mimita Launcher", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    std::string sessionArg;
    {
        std::string authJson = readFile(dir + "\\config\\auth-token.json");
        std::string token = extractJsonStr(authJson, "session_token");
        if (!token.empty())
            sessionArg = " --session \"" + token + "\"";
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD start = GetTickCount();

    std::string commandLine = gameExe + sessionArg;
    if (!CreateProcessA(nullptr, &commandLine[0], nullptr, nullptr, FALSE, 0,
                        nullptr, dir.c_str(), &si, &pi))
        return 1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    DWORD elapsed = GetTickCount() - start;

    if (exitCode != 0 && elapsed > 1000) {
        std::string dumpPath = dir + "\\crash-" + std::to_string(GetTickCount()) + ".dmp";
        writeMinidump(pi.dwProcessId, dumpPath);
        sendCrashReport(localVer, exitCode, elapsed);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
