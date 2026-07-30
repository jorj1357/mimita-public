// 07 30 2026, 16 00
/* purpose
* MimitaLauncher — Standalone installer/launcher with GUI wizard.
* Downloads mimita-game.zip from GitHub, installs to chosen directory.
* Creates Start Menu and desktop shortcuts.
* Handles updates on subsequent runs.
* Does NOT compile, modify, or deploy game source code.
* Does NOT manage network ICE connections or game sessions.
* Does NOT replace the game engine.
*/

#define WIN32_LEAN_AND_MEAN
#define IDB_LOADING_IMAGE 101

#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <dbghelp.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

using namespace Gdiplus;

namespace {

// ── Global GUI state ──────────────────────────────────────────
HWND g_hWnd = nullptr;
Image* g_bgImage = nullptr;
int g_currentPage = 0;
HWND g_pageControls[3][12];
int g_pageControlCount[3] = {0, 0, 0};
HFONT g_arialFont = nullptr;

// Install state
std::string g_installDir;
std::string g_gameExePath;
bool g_gotLatest = false;
std::string g_latestVer, g_zipUrl, g_zipSha256;

// Checkbox state
bool g_createStartMenu = true;
bool g_createDesktop = true;

// Install guard: prevent force-close during install
bool g_installing = false;

// Page control IDs
enum {
    IDC_PATH_EDIT = 200,
    IDC_BROWSE_BTN,
    IDC_INSTALL_BTN,
    IDC_STATUS_TEXT,
    IDC_CHK_STARTMENU,
    IDC_CHK_DESKTOP,
    IDC_CHK_LAUNCH,
    IDC_FINISH_BTN,
    IDC_PROGRESS,
};

// ── Existing helper functions (unchanged) ─────────────────────

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

// Forward declarations
void pumpMessages();
void setProgress(int percent);
void setStatusText(const char* text);

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
        do { char t[4096]; if (!WinHttpReadData(r, t, sizeof(t), &rd)) break; b.insert(b.end(), t, t + rd); } while (rd > 0);
        out.assign(b.data(), b.size());
    }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return ok;
}

struct DownloadState {
    DWORD totalBytes;
    DWORD downloadedBytes;
};

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

    // Get content length
    DWORD contentLen = 0;
    if (ok) {
        DWORD headerSz = sizeof(contentLen);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &headerSz, WINHTTP_NO_HEADER_INDEX);
    }

    if (ok && contentLen > 0) {
        DWORD totalRead = resumeAt;
        char tmp[65536]; DWORD rd = 0;
        do {
            pumpMessages();
            if (!WinHttpReadData(r, tmp, sizeof(tmp), &rd)) break;
            if (rd > 0) {
                DWORD wr = 0;
                if (!WriteFile(f, tmp, rd, &wr, nullptr) || wr != rd) { ok = FALSE; break; }
                totalRead += rd;
                int pct = (int)(totalRead * 100 / contentLen);
                if (pct > 100) pct = 100;
                setProgress(pct);
            }
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

    std::string searchKey = "\"name\":\"mimita-game.zip\"";
    auto assetPos = json.find(searchKey);
    if (assetPos == std::string::npos) return false;

    auto urlKey = json.rfind("\"browser_download_url\"", assetPos);
    if (urlKey == std::string::npos || urlKey > json.find('}', assetPos))
        urlKey = json.find("\"browser_download_url\"", assetPos);
    if (urlKey == std::string::npos) return false;

    auto us = json.find('"', urlKey + 22);
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
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            out += (char)((hex(input[i+1]) << 4) | hex(input[i+2]));
            i += 2;
        }
        else
        {
            out += input[i];
        }
    }
    return out;
}

// ── New: Install config persistence ───────────────────────────

std::string installConfigPath()
{
    return appDir() + "\\install-config.json";
}

void saveInstallConfig(const std::string& installDir)
{
    std::string json = "{\"install_dir\":\"" + installDir + "\"}\n";
    writeFile(installConfigPath(), json);
}

bool loadInstallConfig(std::string& outDir)
{
    std::string json = readFile(installConfigPath());
    if (json.empty()) return false;
    outDir = extractJsonStr(json, "install_dir");
    return !outDir.empty();
}

// ── New: GDI+ background image loading ────────────────────────

bool loadBackgroundImageFromResource()
{
    HRSRC hRes = FindResourceA(nullptr, MAKEINTRESOURCEA(IDB_LOADING_IMAGE), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hMem = LoadResource(nullptr, hRes);
    if (!hMem) return false;
    DWORD size = SizeofResource(nullptr, hRes);
    void* data = LockResource(hMem);
    if (!data || size == 0) return false;

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hGlobal) return false;
    void* buf = GlobalLock(hGlobal);
    if (buf) {
        memcpy(buf, data, size);
        GlobalUnlock(hGlobal);
    }
    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK) {
        GlobalFree(hGlobal);
        return false;
    }
    g_bgImage = Image::FromStream(pStream);
    pStream->Release();
    return (g_bgImage && g_bgImage->GetLastStatus() == Ok);
}

// ── New: Utility functions ────────────────────────────────────

std::string getDefaultInstallDir()
{
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path) == S_OK) {
        return std::string(path) + "\\Mimita";
    }
    return "C:\\Mimita";
}

std::string getDesktopPath()
{
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, path) == S_OK) {
        return std::string(path);
    }
    return "";
}

std::string getStartMenuPath()
{
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, path) == S_OK) {
        return std::string(path) + "\\Mimita";
    }
    return "";
}

bool createShellLink(const std::string& exePath, const std::string& linkPath, const std::string& args, const std::string& description)
{
    IShellLinkA* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr) || !psl) return false;

    psl->SetPath(exePath.c_str());
    if (!args.empty()) psl->SetArguments(args.c_str());
    if (!description.empty()) psl->SetDescription(description.c_str());

    IPersistFile* ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hr) && ppf) {
        std::wstring wpath = widen(linkPath);
        ppf->Save(wpath.c_str(), TRUE);
        ppf->Release();
    }
    psl->Release();
    return true;
}

// ── Message pump (keep window responsive during long ops) ────

void pumpMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// ── Font helpers ──────────────────────────────────────────────

HFONT createArialFont(int pointSize)
{
    HDC hdc = GetDC(nullptr);
    int height = -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    return CreateFontA(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
}

void createArialFonts()
{
    if (!g_arialFont) g_arialFont = createArialFont(11);
}

void destroyFonts()
{
    if (g_arialFont) { DeleteObject(g_arialFont); g_arialFont = nullptr; }
}

void setControlFont(HWND hCtrl)
{
    if (g_arialFont) SendMessageA(hCtrl, WM_SETFONT, (WPARAM)g_arialFont, TRUE);
}

// ── New: Wizard control helpers ───────────────────────────────

void clearPageControls(int page)
{
    for (int i = 0; i < g_pageControlCount[page]; i++) {
        if (g_pageControls[page][i]) {
            DestroyWindow(g_pageControls[page][i]);
            g_pageControls[page][i] = nullptr;
        }
    }
    g_pageControlCount[page] = 0;
}

HWND makeButton(HWND parent, const char* text, int x, int y, int w, int h, int id)
{
    HWND ctrl = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(nullptr), nullptr);
    setControlFont(ctrl);
    return ctrl;
}

HWND makeCheckbox(HWND parent, const char* text, int x, int y, int w, int h, int id)
{
    HWND ctrl = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(nullptr), nullptr);
    setControlFont(ctrl);
    return ctrl;
}

HWND makeStatic(HWND parent, const char* text, int x, int y, int w, int h)
{
    HWND ctrl = CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, GetModuleHandleA(nullptr), nullptr);
    setControlFont(ctrl);
    return ctrl;
}

HWND makeEdit(HWND parent, const char* text, int x, int y, int w, int h, int id)
{
    HWND ctrl = CreateWindowA("EDIT", text, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(nullptr), nullptr);
    setControlFont(ctrl);
    return ctrl;
}

void setStatusText(const char* text)
{
    HWND hStatus = g_pageControls[1][0];
    if (hStatus) SetWindowTextA(hStatus, text);
    pumpMessages();
}

void setProgress(int percent)
{
    HWND hBar = g_pageControls[1][1];
    if (hBar) SendMessageA(hBar, PBM_SETPOS, percent, 0);
    if (percent >= 100) {
        // Turn green at 100%
        SendMessageA(hBar, PBM_SETBARCOLOR, 0, RGB(0, 180, 60));
    }
    pumpMessages();
}

// ── New: Wizard pages (800x600 layout) ────────────────────────

void createPage1()
{
    clearPageControls(0);
    int cx = 800, cy = 600;
    int px = (cx - 480) / 2;

    g_pageControls[0][g_pageControlCount[0]++] = makeStatic(g_hWnd, "Mimita Setup", px, 60, 480, 36);
    g_pageControls[0][g_pageControlCount[0]++] = makeStatic(g_hWnd, "Choose where to install Mimita", px, 100, 480, 22);
    g_pageControls[0][g_pageControlCount[0]++] = makeStatic(g_hWnd, "Install directory:", px, 145, 480, 20);
    g_pageControls[0][g_pageControlCount[0]++] = makeEdit(g_hWnd, g_installDir.c_str(), px, 170, 380, 26, IDC_PATH_EDIT);
    g_pageControls[0][g_pageControlCount[0]++] = makeButton(g_hWnd, "Browse", px + 390, 170, 80, 26, IDC_BROWSE_BTN);

    HWND chk1 = makeCheckbox(g_hWnd, "Create Start Menu shortcut", px, 220, 400, 24, IDC_CHK_STARTMENU);
    Button_SetCheck(chk1, g_createStartMenu ? BST_CHECKED : BST_UNCHECKED);
    g_pageControls[0][g_pageControlCount[0]++] = chk1;

    HWND chk2 = makeCheckbox(g_hWnd, "Create desktop shortcut", px, 248, 400, 24, IDC_CHK_DESKTOP);
    Button_SetCheck(chk2, g_createDesktop ? BST_CHECKED : BST_UNCHECKED);
    g_pageControls[0][g_pageControlCount[0]++] = chk2;

    g_pageControls[0][g_pageControlCount[0]++] = makeButton(g_hWnd, "Install", px + 170, 310, 140, 40, IDC_INSTALL_BTN);
}

void createPage2()
{
    clearPageControls(1);
    int cx = 800, cy = 600;
    int px = (cx - 480) / 2;

    g_pageControls[1][g_pageControlCount[1]++] = makeStatic(g_hWnd, "Installing...", px, 110, 480, 32);
    HWND hStatus = makeStatic(g_hWnd, "Starting...", px, 160, 480, 24);
    g_pageControls[1][g_pageControlCount[1]++] = hStatus;

    // Progress bar
    HWND hBar = CreateWindowA(PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE,
        px, 210, 480, 24, g_hWnd, (HMENU)(INT_PTR)IDC_PROGRESS, GetModuleHandleA(nullptr), nullptr);
    SendMessageA(hBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(hBar, PBM_SETSTEP, 1, 0);
    g_pageControls[1][g_pageControlCount[1]++] = hBar;
}

void showPage(int page)
{
    if (g_currentPage >= 0 && g_currentPage < 2) {
        for (int i = 0; i < 2; i++) {
            if (i != page) {
                for (int j = 0; j < g_pageControlCount[i]; j++) {
                    if (g_pageControls[i][j]) ShowWindow(g_pageControls[i][j], SW_HIDE);
                }
            }
        }
    }
    for (int i = 0; i < g_pageControlCount[page]; i++) {
        if (g_pageControls[page][i]) ShowWindow(g_pageControls[page][i], SW_SHOW);
    }
    g_currentPage = page;
    if (g_hWnd) {
        SetFocus(g_pageControls[page][0]);
        InvalidateRect(g_hWnd, nullptr, TRUE);
    }
}

// ── New: Browse for folder dialog ─────────────────────────────

int CALLBACK browseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
    if (msg == BFFM_INITIALIZED && lpData) {
        SendMessageA(hwnd, BFFM_SETSELECTIONA, TRUE, lpData);
    }
    return 0;
}

bool browseForFolder(HWND parent, std::string& outPath)
{
    BROWSEINFOA bi = {};
    bi.hwndOwner = parent;
    bi.lpszTitle = "Select installation directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = browseCallbackProc;
    bi.lParam = (LPARAM)outPath.c_str();

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return false;

    char path[MAX_PATH];
    if (SHGetPathFromIDListA(pidl, path)) {
        outPath = path;
        IMalloc* imalloc = nullptr;
        if (SHGetMalloc(&imalloc) == S_OK) {
            imalloc->Free(pidl);
            imalloc->Release();
        }
        return true;
    }
    IMalloc* imalloc = nullptr;
    if (SHGetMalloc(&imalloc) == S_OK) {
        imalloc->Free(pidl);
        imalloc->Release();
    }
    return false;
}

// ── New: Launch game helper ────────────────────────────────────

void launchGame(const std::string& exePath, const std::string& workDir)
{
    std::string sessionArg;
    {
        std::string authJson = readFile(workDir + "\\config\\auth-token.json");
        std::string token = extractJsonStr(authJson, "session_token");
        if (!token.empty())
            sessionArg = " --session \"" + token + "\"";
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cli = exePath + sessionArg;
    if (CreateProcessA(nullptr, &cli[0], nullptr, nullptr, FALSE, 0,
                       nullptr, workDir.c_str(), &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ── New: Shortcut creation ────────────────────────────────────

void createShortcuts()
{
    std::string gameExe = g_installDir + "\\mimita.exe";

    if (g_createStartMenu) {
        std::string smPath = getStartMenuPath();
        if (!smPath.empty()) {
            CreateDirectoryA(smPath.c_str(), nullptr);
            createShellLink(gameExe, smPath + "\\Mimita.lnk", "", "Mimita");
        }
    }

    if (g_createDesktop) {
        std::string deskPath = getDesktopPath();
        if (!deskPath.empty()) {
            createShellLink(gameExe, deskPath + "\\Mimita.lnk", "", "Mimita");
        }
    }
}

// ── New: Install flow (runs on page 2) ────────────────────────

void runInstall(HWND hwnd)
{
    g_installing = true;
    setStatusText("Checking for updates...");

    CreateDirectoryA(g_installDir.c_str(), nullptr);

    if (!getGitHubReleaseInfo(g_latestVer, g_zipUrl, g_zipSha256)) {
        MessageBoxA(hwnd, "Could not connect to GitHub to download Mimita.\nCheck your internet connection and try again.",
            "Download Error", MB_OK | MB_ICONERROR);
        return;
    }

    setProgress(0);
    setStatusText("Downloading Mimita...");

    char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
    std::string zipPath = std::string(td) + "mimita-game-" + g_latestVer + ".zip";

    if (!downloadFileTo(g_zipUrl, zipPath, 0)) {
        MessageBoxA(hwnd, "Failed to download Mimita.\nCheck your internet connection and try again.",
            "Download Error", MB_OK | MB_ICONERROR);
        return;
    }

    if (!g_zipSha256.empty()) {
        std::string dlHash;
        if (sha256File(zipPath, dlHash) && dlHash != g_zipSha256) {
            MessageBoxA(hwnd, "Download verification failed. The file may be corrupted.\nPlease try again.",
                "Verification Error", MB_OK | MB_ICONERROR);
            DeleteFileA(zipPath.c_str());
            return;
        }
    }

    setProgress(90);
    setStatusText("Extracting files...");

    if (!extractZipFile(zipPath, g_installDir)) {
        MessageBoxA(hwnd, "Failed to extract Mimita files.\nThe downloaded file may be corrupted.",
            "Extraction Error", MB_OK | MB_ICONERROR);
        DeleteFileA(zipPath.c_str());
        return;
    }

    DeleteFileA(zipPath.c_str());

    // Create writable directories the game expects
    CreateDirectoryA((g_installDir + "\\config\\accounts").c_str(), nullptr);
    CreateDirectoryA((g_installDir + "\\logs").c_str(), nullptr);
    CreateDirectoryA((g_installDir + "\\replays").c_str(), nullptr);

    writeFile(g_installDir + "\\version.txt", g_latestVer);
    saveInstallConfig(g_installDir);

    g_gameExePath = g_installDir + "\\mimita.exe";

    setProgress(95);
    setStatusText("Creating shortcuts...");
    createShortcuts();

    setProgress(100);
    setStatusText("Ready!");

    // Wait 1 second so user sees the green bar, then launch and close
    Sleep(1000);
    pumpMessages();

    // Launch game
    launchGame(g_gameExePath, g_installDir);

    // Close launcher
    DestroyWindow(g_hWnd);
    PostQuitMessage(0);
}

// ── New: Window procedure ─────────────────────────────────────

HBRUSH g_blackBrush = CreateSolidBrush(RGB(0, 0, 0));

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (g_bgImage) {
            Graphics graphics(hdc);
            graphics.DrawImage(g_bgImage, 0, 0, rc.right, rc.bottom);
        } else {
            // Gradient fallback: dark blue to black
            for (int y = 0; y < rc.bottom; y++) {
                int r = (int)(20 * (1.0f - (float)y / rc.bottom));
                int g = (int)(20 * (1.0f - (float)y / rc.bottom));
                int b = (int)(60 * (1.0f - (float)y / rc.bottom));
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
                HGDIOBJ old = SelectObject(hdc, pen);
                MoveToEx(hdc, rc.left, y, nullptr);
                LineTo(hdc, rc.right, y);
                SelectObject(hdc, old);
                DeleteObject(pen);
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(0, 0, 0));
        SetTextColor(hdc, RGB(255, 255, 255));
        return (LRESULT)g_blackBrush;
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(30, 30, 30));
        SetTextColor(hdc, RGB(255, 255, 255));
        static HBRUSH br = CreateSolidBrush(RGB(30, 30, 30));
        return (LRESULT)br;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_BROWSE_BTN) {
            if (browseForFolder(hwnd, g_installDir)) {
                SetWindowTextA(g_pageControls[0][3], g_installDir.c_str());
            }
            return 0;
        }

        if (id == IDC_INSTALL_BTN) {
            if (g_installDir.empty()) {
                MessageBoxA(hwnd, "Please select an install directory.", "Mimita Setup", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            EnableWindow(g_pageControls[0][7], FALSE);
            showPage(1);
            runInstall(hwnd);
            return 0;
        }

        if (id == IDC_CHK_STARTMENU) {
            g_createStartMenu = Button_GetCheck(g_pageControls[0][5]) == BST_CHECKED;
            return 0;
        }
        if (id == IDC_CHK_DESKTOP) {
            g_createDesktop = Button_GetCheck(g_pageControls[0][6]) == BST_CHECKED;
            return 0;
        }

        return 0;
    }
    case WM_CLOSE:
        if (!g_installing) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ── New: Window creation ──────────────────────────────────────

HWND createWizardWindow(HINSTANCE hInst)
{
    const char CLASS_NAME[] = "MimitaLauncherWizard";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassA(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - 800) / 2;
    int y = (screenH - 600) / 2;

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "Mimita Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, 800, 600,
        nullptr, nullptr, hInst, nullptr
    );

    if (hwnd) {
        SetWindowPos(hwnd, nullptr, x, y, 800, 600, SWP_NOZORDER);
    }

    return hwnd;
}

} // anonymous namespace

// ── WinMain ────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int)
{
    std::string cmdLine = lpCmdLine ? lpCmdLine : "";
    std::string launcherDir = appDir();
    std::string localVer = readFile(launcherDir + "\\version.txt");
    if (localVer.empty()) localVer = "0.0.0";

    DeleteFileA((launcherDir + "\\MimitaLauncher.old.exe").c_str());

    // ── Handle mimita:// protocol ─────────────────────────────
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
                        storeSessionToken(launcherDir, sessionToken);
                        // Game might not be installed yet, just store token
                    }
                }
                storeSessionToken(launcherDir, exchangeToken);
            }
        }
        // Continue to launch if installed, or show wizard
    }

    // ── Check if already installed ────────────────────────────
    bool alreadyInstalled = false;
    std::string installDir;
    std::string gameExePath;

    if (loadInstallConfig(installDir)) {
        gameExePath = installDir + "\\mimita.exe";
        alreadyInstalled = (GetFileAttributesA(gameExePath.c_str()) != INVALID_FILE_ATTRIBUTES);
    }

    // ── Initialize COM, GDI+, common controls, fonts ──────────
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ULONG_PTR gdipToken;
    GdiplusStartupInput gdipInput;
    GdiplusStartup(&gdipToken, &gdipInput, nullptr);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    createArialFonts();

    // ── Create window ─────────────────────────────────────────
    g_hWnd = createWizardWindow(hInst);
    if (!g_hWnd) {
        GdiplusShutdown(gdipToken);
        CoUninitialize();
        return 1;
    }

    // ── Load background image ─────────────────────────────────
    loadBackgroundImageFromResource();

    if (alreadyInstalled)
    {
        // ── Fast path: already installed, just update-check and launch ──
        // Show page 2-style status briefly
        createPage2();
        showPage(1); // index 1 = page 2
        setStatusText("Checking for updates...");

        g_gotLatest = getGitHubReleaseInfo(g_latestVer, g_zipUrl, g_zipSha256);

        if (g_gotLatest && g_latestVer != localVer)
        {
            setStatusText("Downloading update...");
            char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
            std::string zipPath = std::string(td) + "mimita-game-" + g_latestVer + ".zip";

            if (downloadFileTo(g_zipUrl, zipPath, 0)) {
                bool hashOk = true;
                if (!g_zipSha256.empty()) {
                    std::string dlHash;
                    if (sha256File(zipPath, dlHash) && dlHash == g_zipSha256) {
                        // OK
                    } else {
                        hashOk = false;
                    }
                }
                if (hashOk) {
                    setStatusText("Extracting update...");
                    if (extractZipFile(zipPath, installDir)) {
                        writeFile(installDir + "\\version.txt", g_latestVer);
                        // Update the launcher itself too if needed
                        std::string newLauncher = installDir + "\\MimitaLauncher.exe";
                        if (GetFileAttributesA(newLauncher.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            std::string thisLauncher = launcherDir + "\\" + appExeName();
                            if (thisLauncher != newLauncher) {
                                // Copy updated launcher to install dir, swap on next boot
                                spawnSelfUpdate(newLauncher, launcherDir);
                            }
                        }
                    }
                }
                DeleteFileA(zipPath.c_str());
            }
        }

        // Launch game — don't wait, close launcher immediately
        if (GetFileAttributesA(gameExePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::string sessionArg;
            {
                std::string authJson = readFile(installDir + "\\config\\auth-token.json");
                std::string token = extractJsonStr(authJson, "session_token");
                if (!token.empty())
                    sessionArg = " --session \"" + token + "\"";
            }

            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            std::string cli = gameExePath + sessionArg;
            if (CreateProcessA(nullptr, &cli[0], nullptr, nullptr, FALSE, 0,
                               nullptr, installDir.c_str(), &si, &pi))
            {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        DestroyWindow(g_hWnd);
    }
    else
    {
        // ── First run: show install wizard ─────────────────────
        g_installDir = getDefaultInstallDir();

        ShowWindow(g_hWnd, SW_SHOW);

        createPage1();
        showPage(0);

        // Message loop
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // ── Cleanup ───────────────────────────────────────────────
    destroyFonts();
    DeleteObject(g_blackBrush);
    delete g_bgImage;
    g_bgImage = nullptr;
    GdiplusShutdown(gdipToken);
    CoUninitialize();
    return 0;
}
