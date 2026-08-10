// 08 10 2026, 17 00
/* purpose
* MimitaLauncher — single-entry installer/launcher/hub for MiMITA.
* Downloads mimita-game.zip from GitHub, installs into version folders.
* Self-installs to %LOCALAPPDATA%\MiMITA\launcher and self-updates silently.
* Sits in the system tray with a lightweight hub window (start/stop game,
* account name, notifications, version, auto-start toggle).
* Does NOT compile, modify, or deploy game source code.
* Does NOT manage network ICE connections or game sessions.
* Does NOT require an account or block offline play.
*/

#define WIN32_LEAN_AND_MEAN
#define IDB_LOADING_IMAGE 101

#define LAUNCHER_VERSION "1.0.0"
#define GITHUB_REPO "jorj1357/mimita-public"
#define RELEASE_API_URL "https://api.github.com/repos/jorj1357/mimita-public/releases/latest"

// Tray / hub / worker messaging
#define WM_APP_TRAYICON   (WM_APP + 1)
#define WM_APP_SHOW_HUB   (WM_APP + 2)
#define WM_APP_STATUS     (WM_APP + 3)
#define WM_APP_SILENT_DONE (WM_APP + 4)

#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <dbghelp.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// Local ZIP testing flags
bool g_useLocalZip = false;
std::string g_localZipPath;
bool g_noVerify = false;

// Suppress modal error dialogs (used by automated tests so an invalid game
// executable cannot block the process on OS or launcher message boxes).
bool g_noErrorDialogs = false;

// ── Launcher home / lifecycle ─────────────────────────────────
HANDLE g_hSingleton = nullptr;
bool g_quit = false;
bool g_altF4Pending = false;

// Test override: point at a local launcher_info.json (no GitHub needed).
std::string g_releaseJsonPath;

// ── Game process state ────────────────────────────────────────
HANDLE g_gameProc = nullptr;
DWORD g_gamePid = 0;
std::string g_hubNote;

// ── Tray / hub windows ────────────────────────────────────────
HWND g_trayWnd = nullptr;
HWND g_hubWnd = nullptr;
NOTIFYICONDATAA g_nid = {};

// Release info cache (self-update + game update)
struct ReleaseInfo {
    std::string gameVersion;
    std::string zipUrl, zipSha256;
    std::string launcherVersion, launcherUrl, launcherSha256;
    std::string changelog;
    bool gotLauncherInfo = false;
};
ReleaseInfo g_releaseInfo;
bool g_releaseFetched = false;

// Hub control IDs
enum {
    IDH_ACCOUNT = 300,
    IDH_NEWS,
    IDH_BUILD,
    IDH_STATUS,
    IDH_START,
    IDH_STOP,
    IDH_AUTOSTART,
    IDH_CLOSE,
};

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
    IDC_CHK_AUTOSTART,
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

bool hasFlag(const std::string& cmd, const std::string& flag)
{
    return cmd.find(flag) != std::string::npos;
}

std::string getFlagValue(const std::string& cmd, const std::string& flag)
{
    auto pos = cmd.find(flag);
    if (pos == std::string::npos) return "";
    pos += flag.size();
    while (pos < cmd.size() && (cmd[pos] == ' ' || cmd[pos] == '=')) ++pos;
    if (pos < cmd.size() && cmd[pos] == '"') {
        ++pos;
        auto end = cmd.find('"', pos);
        if (end == std::string::npos) return cmd.substr(pos);
        return cmd.substr(pos, end - pos);
    }
    auto end = pos;
    while (end < cmd.size() && cmd[end] != ' ') ++end;
    return cmd.substr(pos, end - pos);
}

void printHelp()
{
    // Help text uses printf since this runs before window creation
    printf(
        "MimitaLauncher - Installer, updater and hub for Mimita\n"
        "\n"
        "Usage: MimitaLauncher.exe [options]\n"
        "\n"
        "Options:\n"
        "  --help              Show this help message and exit.\n"
        "  --tray              Start silently into the system tray (no auto-launch).\n"
        "  --local-zip <path>  Use a local mimita-game.zip instead of GitHub.\n"
        "  --release-json <f>  Use a local launcher_info.json instead of GitHub.\n"
        "  --no-verify         Skip SHA-256 verification of the ZIP.\n"
        "  --no-error-dialogs  Never show modal error dialogs (tests).\n"
        "  --post-upgrade <p>  Internal: delete the old launcher exe after a swap.\n"
        "\n"
        "Examples:\n"
        "  MimitaLauncher.exe --local-zip mimita-game.zip\n"
        "  MimitaLauncher.exe --local-zip build/mimita-game.zip --no-verify\n"
    );
}

// ── Path / version helpers ────────────────────────────────────

std::string currentExePath()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::string(buf);
}

std::string getLocalAppData()
{
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path) == S_OK)
        return std::string(path);
    return "C:\\";
}

std::string launcherHomeDir()
{
    return getLocalAppData() + "\\MiMITA\\launcher";
}

std::string launcherHomePath()
{
    return launcherHomeDir() + "\\MimitaLauncher.exe";
}

bool pathExists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Compare "1.2.3" vs "1.2.10" numerically. Returns -1/0/1.
int versionCompare(const std::string& a, const std::string& b)
{
    std::string sa = a, sb = b;
    if (!sa.empty() && sa[0] == 'v') sa = sa.substr(1);
    if (!sb.empty() && sb[0] == 'v') sb = sb.substr(1);
    size_t ia = 0, ib = 0;
    while (ia < sa.size() || ib < sb.size()) {
        size_t ea = sa.find('.', ia), eb = sb.find('.', ib);
        if (ea == std::string::npos) ea = sa.size();
        if (eb == std::string::npos) eb = sb.size();
        long na = ia < sa.size() ? atol(sa.substr(ia, ea - ia).c_str()) : 0;
        long nb = ib < sb.size() ? atol(sb.substr(ib, eb - ib).c_str()) : 0;
        if (na != nb) return na < nb ? -1 : 1;
        ia = ea + 1; ib = eb + 1;
    }
    return 0;
}

// ── Version-folder install layout ─────────────────────────────
// root/
//   versions/v<tag>/mimita.exe ...   (one folder per game version)
//   data/config|logs|replays/        (shared user data, junctioned into the
//                                     active version folder so the game's
//                                     cwd-relative paths keep working)
//   active-version.txt               ("2.0.1")
//   install-config.json              (stores root under key "install_dir")

std::string versionDir(const std::string& root, const std::string& tag)
{
    return root + "\\versions\\v" + tag;
}

std::string activeVersionFile(const std::string& root)
{
    return root + "\\active-version.txt";
}

std::string getActiveVersion(const std::string& root)
{
    return readFile(activeVersionFile(root));
}

void setActiveVersion(const std::string& root, const std::string& tag)
{
    writeFile(activeVersionFile(root), tag + "\n");
}

std::string gameExeForRoot(const std::string& root)
{
    std::string tag = getActiveVersion(root);
    if (tag.empty()) return "";
    return versionDir(root, tag) + "\\mimita.exe";
}

std::string installedVersion(const std::string& root)
{
    std::string tag = getActiveVersion(root);
    if (tag.empty()) return "";
    std::string v = readFile(versionDir(root, tag) + "\\version.txt");
    return v.empty() ? tag : v;
}

std::string launcherDataDir()
{
    return getLocalAppData() + "\\MiMITA\\launcher-data";
}

// ── Filesystem helpers ────────────────────────────────────────

bool deleteDirectory(const std::string& path)
{
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    buf.push_back('\0');
    SHFILEOPSTRUCTA fo = {};
    fo.wFunc = FO_DELETE;
    fo.pFrom = buf.data();
    fo.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationA(&fo) == 0;
}

void copyTree(const std::string& src, const std::string& dst)
{
    CreateDirectoryA(dst.c_str(), nullptr);
    std::string search = src + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string s = src + "\\" + name;
        std::string d = dst + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) copyTree(s, d);
        else CopyFileA(s.c_str(), d.c_str(), FALSE);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// Directory junction (works without admin, unlike symlinks).
bool makeJunction(const std::string& link, const std::string& target)
{
    std::string cmd = "cmd /c mklink /J \"" + link + "\" \"" + target + "\"";
    return system(cmd.c_str()) == 0;
}

// Seed shared user data from a freshly extracted version folder and junction
// config/logs/replays back so the game keeps its cwd-relative data paths.
void linkUserDirs(const std::string& root, const std::string& verFolder)
{
    std::string dataRoot = root + "\\data";
    CreateDirectoryA(dataRoot.c_str(), nullptr);
    const char* dirs[] = { "config", "logs", "replays" };
    std::string base = root + "\\versions\\" + verFolder;
    for (const char* d : dirs) {
        std::string src = base + "\\" + d;
        std::string dst = dataRoot + "\\" + d;
        if (!pathExists(dst)) {
            if (pathExists(src))
                MoveFileExA(src.c_str(), dst.c_str(),
                            MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
            else
                CreateDirectoryA(dst.c_str(), nullptr);
        } else if (pathExists(src)) {
            deleteDirectory(src);
        }
        if (pathExists(src)) {
            // Junction failed or move failed; give the game a working copy.
            copyTree(dst, src);
        } else {
            makeJunction(src, dst);
        }
    }
}

// Delete every version folder except the active one and the newest other one.
void cleanupOldVersions(const std::string& root, const std::string& activeTag)
{
    std::string versionsRoot = root + "\\versions";
    std::string search = versionsRoot + "\\v*";
    std::vector<std::string> folders;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string name = fd.cFileName;
            if (name.size() > 1 && name[0] == 'v') folders.push_back(name);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    std::string bestOther;
    for (const std::string& f : folders) {
        std::string tag = f.substr(1);
        if (tag == activeTag) continue;
        if (bestOther.empty() || versionCompare(tag, bestOther) > 0) bestOther = tag;
    }
    for (const std::string& f : folders) {
        std::string tag = f.substr(1);
        if (tag == activeTag || tag == bestOther) continue;
        deleteDirectory(versionsRoot + "\\" + f);
    }
}

// Convert a legacy single-directory install into version folders.
void migrateLegacyInstall(const std::string& root)
{
    std::string versionsRoot = root + "\\versions";
    if (pathExists(versionsRoot)) return;          // already migrated
    if (!pathExists(root + "\\mimita.exe")) return; // not a legacy install

    CreateDirectoryA(versionsRoot.c_str(), nullptr);
    std::string tag = readFile(root + "\\version.txt");
    if (tag.empty()) tag = "0.0.0";
    std::string verDir = versionDir(root, tag);
    CreateDirectoryA(verDir.c_str(), nullptr);

    std::string search = root + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;
            if (name == "install-config.json" || name == "versions" ||
                name == "launcher" || name == "data") continue;
            std::string s = root + "\\" + name;
            MoveFileExA(s.c_str(), (verDir + "\\" + name).c_str(),
                        MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    setActiveVersion(root, tag);
    linkUserDirs(root, "v" + tag);
}

// ── Windows auto-start toggle ─────────────────────────────────

std::string runKeyPath()
{
    return "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

bool isAutoStartEnabled()
{
    HKEY k = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKeyPath().c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    LONG r = RegQueryValueExA(k, "MimitaLauncher", nullptr, &type, nullptr, nullptr);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

void setAutoStartEnabled(bool enabled)
{
    HKEY k = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKeyPath().c_str(), 0, KEY_WRITE, &k) != ERROR_SUCCESS) {
        RegCreateKeyExA(HKEY_CURRENT_USER, runKeyPath().c_str(), 0, nullptr, 0,
                        KEY_WRITE, nullptr, &k, nullptr);
    }
    if (!k) return;
    if (enabled) {
        std::string cmd = "\"" + launcherHomePath() + "\" --tray";
        RegSetValueExA(k, "MimitaLauncher", 0, REG_SZ,
                       (const BYTE*)cmd.c_str(), (DWORD)cmd.size() + 1);
    } else {
        RegDeleteValueA(k, "MimitaLauncher");
    }
    RegCloseKey(k);
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
    // Local-file support (used by --local-zip and --release-json test overrides).
    if (url.compare(0, 7, "file://") == 0 ||
        GetFileAttributesA(url.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::string local = url.compare(0, 7, "file://") == 0 ? url.substr(7) : url;
        while (!local.empty() && (local[0] == '/' || local[0] == '\\'))
            local = local.substr(1);
        for (size_t i = 0; i < local.size(); ++i)
            if (local[i] == '/') local[i] = '\\';
        if (resumeAt > 0) return false;
        return CopyFileA(local.c_str(), dest.c_str(), FALSE) != FALSE;
    }

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
                int pct = (int)(totalRead * 85 / contentLen);
                if (pct > 85) pct = 85;
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

bool extractZipFileWithProgress(const std::string& zipPath, const std::string& destDir)
{
    CreateDirectoryA(destDir.c_str(), nullptr);

    // Write PowerShell extraction script to temp file
    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    std::string psPath = std::string(tempDir) + "mimita-extract.ps1";

    std::string ps =
        "param($zip,$dest)\n"
        "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8\n"
        "Add-Type -AssemblyName System.IO.Compression.FileSystem\n"
        "$z=[System.IO.Compression.ZipFile]::OpenRead($zip)\n"
        "$e=$z.Entries;$t=$e.Count;$i=0\n"
        "foreach($f in $e){\n"
        "  $p=[int](++$i*100/$t)\n"
        "  Write-Output \"$p|$($f.FullName)\"\n"
        "  $d=Join-Path $dest $f.FullName\n"
        "  $dn=[IO.Path]::GetDirectoryName($d)\n"
        "  if(!(Test-Path $dn)){New-Item -ItemType Directory -Path $dn -Force|Out-Null}\n"
        "  if(!$f.FullName.EndsWith('/')){$s=$f.Open();$ds=[IO.File]::Create($d);$s.CopyTo($ds);$ds.Close();$s.Close()}\n"
        "}\n"
        "$z.Dispose()\n";

    writeFile(psPath, ps);

    std::string cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File \""
        + psPath + "\" \"" + zipPath + "\" \"" + destDir + "\"";

    // Create pipe to capture stdout
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) { DeleteFileA(psPath.c_str()); return false; }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) { CloseHandle(hRead); DeleteFileA(psPath.c_str()); return false; }

    // Read output lines: "pct|filepath"
    char buf[4096];
    DWORD rd = 0;
    std::string leftover;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &rd, nullptr) && rd > 0) {
        buf[rd] = '\0';
        leftover += buf;
        size_t nl;
        while ((nl = leftover.find('\n')) != std::string::npos) {
            std::string ln = leftover.substr(0, nl);
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            leftover.erase(0, nl + 1);
            auto sep = ln.find('|');
            if (sep != std::string::npos) {
                int pct = atoi(ln.substr(0, sep).c_str());
                std::string fname = ln.substr(sep + 1);
                if (fname.size() > 65) fname = "..." + fname.substr(fname.size() - 62);
                setProgress(pct);
                setStatusText(("Extracting: " + fname).c_str());
            }
            pumpMessages();
        }
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD psExit = 0;
    GetExitCodeProcess(pi.hProcess, &psExit);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
    DeleteFileA(psPath.c_str());
    return psExit == 0;
}

std::string getAssetDownloadUrl(const std::string& json, const std::string& assetName)
{
    std::string key = "\"name\":\"" + assetName + "\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return "";
    auto urlKey = json.find("\"browser_download_url\"", pos);
    if (urlKey == std::string::npos) return "";
    auto us = json.find('"', urlKey + 21);
    if (us == std::string::npos) return "";
    auto ue = json.find('"', us + 1);
    if (ue == std::string::npos) return "";
    return json.substr(us + 1, ue - us - 1);
}

void parseLauncherInfoJson(const std::string& li, ReleaseInfo& info)
{
    info.launcherVersion = extractJsonStr(li, "launcher_version");
    info.launcherSha256 = extractJsonStr(li, "launcher_sha256");
    info.launcherUrl = extractJsonStr(li, "launcher_url");
    info.zipUrl = extractJsonStr(li, "game_zip_url");
    info.zipSha256 = extractJsonStr(li, "game_zip_sha256");
    info.changelog = extractJsonStr(li, "changelog");
    if (info.gameVersion.empty()) info.gameVersion = extractJsonStr(li, "game_version");
    info.gotLauncherInfo = !info.launcherVersion.empty() || !info.zipUrl.empty();
}

// Fetch the latest release info. Prefers the launcher_info.json asset; falls
// back to the release tag + mimita-game.zip asset + SHA-256 from the body so
// releases published without launcher_info.json keep working.
bool fetchReleaseInfo(ReleaseInfo& info)
{
    if (!g_releaseJsonPath.empty()) {
        std::string li = readFile(g_releaseJsonPath);
        if (li.empty()) return false;
        info = ReleaseInfo();
        parseLauncherInfoJson(li, info);
        return true;
    }

    std::string json;
    if (!httpGETWithHeaders(RELEASE_API_URL, json,
                            L"Accept: application/vnd.github+json\r\n"))
        return false;

    std::string tag = extractJsonStr(json, "tag_name");
    if (tag.empty()) return false;
    if (tag.size() > 1 && tag[0] == 'v') tag = tag.substr(1);
    info.gameVersion = tag;

    std::string liUrl = getAssetDownloadUrl(json, "launcher_info.json");
    std::string li;
    if (!liUrl.empty() && httpGET(liUrl, li)) {
        parseLauncherInfoJson(li, info);
        if (info.gameVersion.empty()) info.gameVersion = tag;
    }

    if (info.zipUrl.empty()) info.zipUrl = getAssetDownloadUrl(json, "mimita-game.zip");
    if (info.zipSha256.empty()) {
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
                            if (valid) info.zipSha256 = hash;
                        }
                    }
                }
            }
        }
    }
    if (info.launcherUrl.empty()) info.launcherUrl = getAssetDownloadUrl(json, "MimitaLauncher.exe");
    return true;
}

bool getGitHubReleaseInfo(std::string& outVersion, std::string& outZipUrl, std::string& outSha256)
{
    ReleaseInfo info;
    if (!fetchReleaseInfo(info)) return false;
    outVersion = info.gameVersion;
    outZipUrl = info.zipUrl;
    outSha256 = info.zipSha256;
    return !outVersion.empty();
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

void storeSessionToken(const std::string& dir, const std::string& token)
{
    // The game reads config/auth-token.json from its working directory, which
    // is junctioned to <root>\data\config for every version. Store there so
    // the token survives version switches and installs.
    std::string root = dir;
    std::string cfg;
    if (pathExists(root + "\\data") || pathExists(root + "\\versions")) {
        cfg = root + "\\data\\config";
    } else {
        cfg = root + "\\config";
    }
    CreateDirectoryA(cfg.c_str(), nullptr);
    writeFile(cfg + "\\auth-token.json", "{\"session_token\":\"" + token + "\"}\n");
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
    HWND hStatus = g_pageControls[1][1];
    if (hStatus) SetWindowTextA(hStatus, text);
    pumpMessages();
}

void setProgress(int percent)
{
    HWND hBar = g_pageControls[1][2];
    if (hBar) SendMessageA(hBar, PBM_SETPOS, percent, 0);
    if (percent >= 100) {
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

    HWND chk3 = makeCheckbox(g_hWnd, "Start MiMITA Launcher when my computer turns on",
                             px, 276, 400, 24, IDC_CHK_AUTOSTART);
    Button_SetCheck(chk3, isAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);
    g_pageControls[0][g_pageControlCount[0]++] = chk3;

    g_pageControls[0][g_pageControlCount[0]++] = makeButton(g_hWnd, "Install", px + 170, 320, 140, 40, IDC_INSTALL_BTN);
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

void writeCrashLog(const std::string& path, const std::string& version,
                   DWORD exitCode, DWORD uptimeMs)
{
    char hex[16];
    snprintf(hex, sizeof(hex), "0x%08lx", (unsigned long)exitCode);
    std::string log =
        "Launcher Crash Report\n"
        "=====================\n"
        "Timestamp: " + std::to_string(GetTickCount()) + " ms since boot\n"
        "Launcher Version: " + version + "\n"
        "Game Version: " + version + "\n"
        "Exit Code: " + std::string(hex) + "\n"
        "Uptime: " + std::to_string(uptimeMs) + " ms\n"
        "Process: mimita.exe\n"
        "\n"
        "The game's crash handler also writes a minidump and detailed\n"
        "crash report to: %LOCALAPPDATA%\\MiMITA\\crashes\\\n";
    writeFile(path, log);
}

// Crash recovery dialog actions
enum CrashRecoveryAction {
    CRASH_RECOVER_CLOSE = 0,
    CRASH_RECOVER_RESTART = 1,
    CRASH_RECOVER_OPEN_FOLDER = 2,
    CRASH_RECOVER_COPY = 3,
    CRASH_RECOVER_ROLLBACK = 4,
};

// Returns true if the crash directory contains a nonzero file matching prefix+ext.
bool crashArtifactExists(const std::string& dir, const std::string& prefix,
                         const std::string& ext)
{
    std::string path = dir + "\\" + prefix + ext;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD hi = 0;
    bool ok = GetFileSize(h, &hi) > 0;
    CloseHandle(h);
    return ok;
}

// TaskDialogIndirect comes from comctl32 v6. It is NOT exported by the v5.82
// copy that the loader binds to without a side-by-side manifest, so importing
// it statically makes the launcher fail to start on a fresh load. Resolve it
// at runtime and fall back to MessageBoxA so startup never depends on it.
typedef HRESULT(WINAPI* TaskDialogIndirectFn)(const TASKDIALOGCONFIG* pTaskConfig,
                                              int* pnButton, int* pnRadioButton,
                                              BOOL* pfVerificationFlagChecked);

// Inspect the crash directory and show one recovery window. Returns the chosen
// action. Never claims a file was saved unless it exists and is nonzero.
CrashRecoveryAction showCrashRecoveryDialog(const std::string& crashDir,
                                            const std::string& details,
                                            bool txtOk, bool dmpOk,
                                            bool canRollback)
{
    std::wstring dirW = widen(crashDir);
    std::wstring detailsW = widen(details);

    // Load comctl32.dll and resolve TaskDialogIndirect at runtime. Never a
    // static import: if the system copy lacks it (e.g. v5.82 without a v6
    // manifest), fall back to a plain message box instead of failing startup.
    HMODULE hComctl = LoadLibraryW(L"comctl32.dll");
    TaskDialogIndirectFn taskDialogIndirect = nullptr;
    if (hComctl)
        taskDialogIndirect = reinterpret_cast<TaskDialogIndirectFn>(
            (void*)GetProcAddress(hComctl, "TaskDialogIndirect"));

    if (taskDialogIndirect)
    {
        TASKDIALOG_BUTTON buttons[5] = {
            { CRASH_RECOVER_RESTART, L"Restart MiMITA" },
            { CRASH_RECOVER_OPEN_FOLDER, L"Open crash folder" },
            { CRASH_RECOVER_COPY, L"Copy error details" },
            { CRASH_RECOVER_CLOSE, L"Close" },
        };
        int buttonCount = 4;
        if (canRollback)
            buttons[buttonCount++] = { CRASH_RECOVER_ROLLBACK, L"Restore previous version" };

        TASKDIALOGCONFIG cfg = {};
        cfg.cbSize = sizeof(cfg);
        cfg.hwndParent = nullptr;
        cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
        cfg.pszMainIcon = TD_ERROR_ICON;
        cfg.pszWindowTitle = L"Mimita Launcher";
        cfg.pszMainInstruction = L"Mimita closed unexpectedly";
        std::wstring content;
        content += L"Exit code: " + widen(details.empty() ? "?" : details) + L"\n";
        content += txtOk ? L"Crash report: saved\n" : L"Crash report: not saved\n";
        content += dmpOk ? L"Minidump: saved\n" : L"Minidump: not saved\n";
        content += L"\nCrash folder:\n" + dirW;
        cfg.pszContent = content.c_str();
        cfg.cButtons = buttonCount;
        cfg.pButtons = buttons;
        cfg.nDefaultButton = CRASH_RECOVER_RESTART;

        int clicked = CRASH_RECOVER_CLOSE;
        HRESULT hr = taskDialogIndirect(&cfg, &clicked, nullptr, nullptr);
        if (hComctl) FreeLibrary(hComctl);
        if (SUCCEEDED(hr))
            return (CrashRecoveryAction)clicked;
        // Fall through to the plain message box on TaskDialog failure.
    }

    std::string msg = "Mimita closed unexpectedly.\n\n"
        "Exit code: " + (details.empty() ? std::string("?") : details) + "\n"
        "Crash report: " + std::string(txtOk ? "saved" : "not saved") + "\n"
        "Minidump: " + std::string(dmpOk ? "saved" : "not saved") + "\n\n"
        "Crash folder:\n" + crashDir + "\n\n"
        "Choose Yes to restart, No to open the crash folder, Cancel to close.";
    if (canRollback)
        msg += "\n\nThere is a previous version you can restore.";
    int choice = MessageBoxA(nullptr, msg.c_str(), "Mimita Launcher",
                             MB_YESNOCANCEL | MB_ICONERROR | MB_DEFBUTTON1);
    if (hComctl) FreeLibrary(hComctl);
    if (choice == IDYES) return CRASH_RECOVER_RESTART;
    if (choice == IDNO) return CRASH_RECOVER_OPEN_FOLDER;
    return CRASH_RECOVER_CLOSE;
}

// ── Game launch helpers ───────────────────────────────────────

bool isValidPeExecutable(const std::string& path)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    BYTE buf[4096];
    DWORD rd = 0;
    bool ok = false;
    if (ReadFile(h, buf, sizeof(buf), &rd, nullptr) && rd >= 0x40) {
        if (buf[0] == 'M' && buf[1] == 'Z') {
            DWORD e_lfanew = *(DWORD*)(buf + 0x3C);
            if (e_lfanew + 4 <= rd)
                ok = (buf[e_lfanew] == 'P' && buf[e_lfanew + 1] == 'E' &&
                      buf[e_lfanew + 2] == 0 && buf[e_lfanew + 3] == 0);
        }
    }
    CloseHandle(h);
    return ok;
}

std::string win32ErrorMessage(DWORD err)
{
    LPSTR msg = nullptr;
    DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, (LPSTR)&msg, 0, nullptr);
    std::string out = (n && msg) ? std::string(msg, n) : "unknown error";
    if (msg) LocalFree(msg);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return out;
}

void reportLaunchFailure(const std::string& workDir, const std::string& exePath,
                         const std::string& reason, DWORD win32Error)
{
    std::string msg = "Game launch failed: path=\"" + exePath
                    + "\" win32_error=" + std::to_string(win32Error)
                    + " message=\"" + reason + "\"";
    printf("%s\n", msg.c_str());
    fflush(stdout);

    std::string logDir = workDir + "\\launcher-data\\logs";
    CreateDirectoryA((workDir + "\\launcher-data").c_str(), nullptr);
    CreateDirectoryA(logDir.c_str(), nullptr);
    std::string logPath = logDir + "\\launch-error-" + std::to_string(GetTickCount()) + ".txt";
    writeFile(logPath, msg + "\n");

    if (!g_noErrorDialogs)
        MessageBoxA(nullptr, msg.c_str(), "Mimita Launcher", MB_OK | MB_ICONERROR);
}

// ── Tray icon + hub window + game session ─────────────────────

HBRUSH g_hubGreenBrush = nullptr;
HBRUSH g_hubRedBrush = nullptr;
HWND g_hubStartBtn = nullptr;
HWND g_hubStopBtn = nullptr;
HFONT g_hubBoldFont = nullptr;

struct GameSession { std::string exe, workDir; };

LRESULT CALLBACK TrayWndProc(HWND, UINT, WPARAM, LPARAM);
void startGameInThread(const std::string& root);
void launchGame(const std::string& exePath, const std::string& workDir);

void showTrayBalloon(const std::string& title, const std::string& text)
{
    if (!g_trayWnd) return;
    g_nid.uFlags = NIF_INFO;
    lstrcpynA(g_nid.szInfoTitle, title.c_str(), sizeof(g_nid.szInfoTitle));
    lstrcpynA(g_nid.szInfo, text.c_str(), sizeof(g_nid.szInfo));
    g_nid.uTimeout = 5000;
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconA(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void notifyStatus()
{
    if (g_hubWnd) PostMessageA(g_hubWnd, WM_APP_STATUS, 0, 0);
}

std::string currentRoot()
{
    std::string root;
    if (!loadInstallConfig(root)) root = getDefaultInstallDir();
    return root;
}

void refreshHub()
{
    if (!g_hubWnd) return;
    std::string root = currentRoot();
    std::string user = "Guest";
    std::string pf = root + "\\data\\config\\current-profile.json";
    if (pathExists(pf)) {
        std::string u = extractJsonStr(readFile(pf), "username");
        if (!u.empty()) user = u;
    }
    HWND h;
    if ((h = GetDlgItem(g_hubWnd, IDH_ACCOUNT)))
        SetWindowTextA(h, ("Hey, " + user).c_str());
    std::string ver = installedVersion(root);
    if (ver.empty()) ver = "not installed";
    if ((h = GetDlgItem(g_hubWnd, IDH_BUILD)))
        SetWindowTextA(h, ("Build: v" + ver).c_str());
    bool running = g_gameProc != nullptr;
    if ((h = GetDlgItem(g_hubWnd, IDH_STATUS)))
        SetWindowTextA(h, running ? "Status: Running" : "Status: Not running");
    std::string news = g_hubNote.empty() ? "Nothing new yet." : g_hubNote;
    news += "\r\n\r\n\u2022 Messages from players (coming soon)";
    if ((h = GetDlgItem(g_hubWnd, IDH_NEWS)))
        SetWindowTextA(h, news.c_str());
    if (g_hubStartBtn) ShowWindow(g_hubStartBtn, running ? SW_HIDE : SW_SHOW);
    if (g_hubStopBtn) ShowWindow(g_hubStopBtn, running ? SW_SHOW : SW_HIDE);
    if ((h = GetDlgItem(g_hubWnd, IDH_AUTOSTART)))
        Button_SetCheck(h, isAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);
}

void showHub()
{
    if (!g_hubWnd) return;
    refreshHub();
    ShowWindow(g_hubWnd, SW_SHOW);
    SetForegroundWindow(g_hubWnd);
}

// ── Update application (version-folder installs) ──────────────

bool applyGameUpdate(const std::string& root, std::string tag,
                     const std::string& zipPath)
{
    // Sweep any stale staging folders left by interrupted updates.
    std::string versionsRoot = root + "\\versions";
    std::string sweepSearch = versionsRoot + "\\.staging-*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(sweepSearch.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                deleteDirectory(versionsRoot + "\\" + fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Stage the new version in a fresh folder, verify it, then atomically move
    // it into place. The currently active version is never touched until the
    // replacement is verified, so an interrupted update can't break the game.
    std::string stage = versionsRoot + "\\.staging-" + tag;
    deleteDirectory(stage);
    if (!extractZipFileWithProgress(zipPath, stage)) {
        deleteDirectory(stage);
        return false;
    }
    if (!isValidPeExecutable(stage + "\\mimita.exe")) {
        deleteDirectory(stage);
        return false;
    }

    // The zip carries its own version.txt — prefer it so the version folder is
    // named after the real build.
    std::string vt = readFile(stage + "\\version.txt");
    std::string finalTag = vt.empty() ? tag : vt;
    std::string finalDir = versionDir(root, finalTag);

    deleteDirectory(finalDir);
    if (!MoveFileExA(stage.c_str(), finalDir.c_str(), MOVEFILE_WRITE_THROUGH)) {
        deleteDirectory(stage);
        return false;
    }
    linkUserDirs(root, "v" + finalTag);
    setActiveVersion(root, finalTag);
    cleanupOldVersions(root, finalTag);
    return isValidPeExecutable(finalDir + "\\mimita.exe");
}

void showChangelogOnce(const std::string& version, const std::string& changelog)
{
    if (changelog.empty()) return;
    CreateDirectoryA(launcherDataDir().c_str(), nullptr);
    std::string seenFile = launcherDataDir() + "\\seen-news.txt";
    std::string key = version + ":" + changelog;
    if (readFile(seenFile) == key) return;
    writeFile(seenFile, key);
    std::string text = changelog;
    if (text.size() > 120) text = text.substr(0, 120) + "...";
    showTrayBalloon(("MiMITA " + version).c_str(), text.c_str());
}

bool isDevMode()
{
    if (g_useLocalZip) return true;
    return pathExists(appDir() + "\\mimita-game.zip");
}

// Ensure the active install is the latest release. Returns true when a usable
// game executable exists afterward.
bool ensureLatest(const std::string& root, bool visible)
{
    // Never replace game files while the game is running; defer to next launch.
    if (g_gameProc)
        return isValidPeExecutable(gameExeForRoot(root));

    ReleaseInfo info = g_releaseInfo;
    if (!g_releaseFetched) {
        g_releaseFetched = fetchReleaseInfo(info);
        g_releaseInfo = info;
    }
    std::string localVer = installedVersion(root);

    if (isDevMode()) {
        if (!g_localZipPath.empty() && pathExists(g_localZipPath)) {
            if (visible) setStatusText("Extracting local zip...");
            applyGameUpdate(root, localVer.empty() ? "dev" : localVer, g_localZipPath);
        }
        return isValidPeExecutable(gameExeForRoot(root));
    }

    if (!info.gameVersion.empty() && info.gameVersion != localVer && !info.zipUrl.empty()) {
        if (visible) setStatusText("Downloading update...");
        char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
        std::string zip = std::string(td) + "mimita-game-" + info.gameVersion + ".zip";
        if (downloadFileTo(info.zipUrl, zip, 0)) {
            bool ok = true;
            if (!info.zipSha256.empty() && !g_noVerify) {
                std::string h;
                ok = sha256File(zip, h) && h == info.zipSha256;
            }
            if (ok) {
                if (visible) setStatusText("Installing update...");
                if (applyGameUpdate(root, info.gameVersion, zip)) {
                    g_hubNote = "MiMITA updated to v" + info.gameVersion;
                    if (!info.changelog.empty()) g_hubNote += ": " + info.changelog;
                    showChangelogOnce(info.gameVersion, info.changelog);
                }
            }
            DeleteFileA(zip.c_str());
        }
    }

    // Repair: if the active exe is unusable, redownload it fresh.
    if (!isValidPeExecutable(gameExeForRoot(root)) && !info.zipUrl.empty()) {
        char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
        std::string zip = std::string(td) + "mimita-game-" + info.gameVersion + ".zip";
        if (downloadFileTo(info.zipUrl, zip, 0)) {
            applyGameUpdate(root, info.gameVersion, zip);
            DeleteFileA(zip.c_str());
        }
    }
    return isValidPeExecutable(gameExeForRoot(root));
}

// Visible launch flow: progress window, update check, then (optionally) play.
bool runVisibleLaunchFlow(const std::string& root, bool launchAfter)
{
    if (g_hWnd) {
        ShowWindow(g_hWnd, SW_SHOW);
        createPage2();
        showPage(1);
        setProgress(0);
        setStatusText("Checking for updates...");
    }
    pumpMessages();

    bool usable = ensureLatest(root, true);

    if (g_hWnd) {
        setProgress(100);
        setStatusText("Complete!");
        pumpMessages();
        Sleep(400);
        pumpMessages();
    }

    if (launchAfter && usable) {
        if (g_hWnd) ShowWindow(g_hWnd, SW_HIDE);
        startGameInThread(root);
    }
    if (g_hWnd && !launchAfter) ShowWindow(g_hWnd, SW_HIDE);
    return usable;
}

void playFromTray()
{
    std::string root = currentRoot();
    if (!pathExists(gameExeForRoot(root))) {
        g_installDir = root;
        ShowWindow(g_hWnd, SW_SHOW);
        createPage1();
        showPage(0);
        SetForegroundWindow(g_hWnd);
        return;
    }
    runVisibleLaunchFlow(root, true);
}

// ── Game process control ──────────────────────────────────────

DWORD WINAPI gameThreadProc(LPVOID p)
{
    GameSession* s = (GameSession*)p;
    launchGame(s->exe, s->workDir);
    delete s;
    return 0;
}

void startGameInThread(const std::string& root)
{
    if (g_gameProc) { showHub(); return; }
    std::string tag = getActiveVersion(root);
    std::string exe = gameExeForRoot(root);
    std::string verDir = versionDir(root, tag);
    if (exe.empty() || !isValidPeExecutable(exe)) {
        if (runVisibleLaunchFlow(root, true)) return;
        reportLaunchFailure(root, exe, "game install is not usable", 0);
        return;
    }
    if (g_hWnd) ShowWindow(g_hWnd, SW_HIDE);
    GameSession* s = new GameSession{ exe, verDir };
    HANDLE h = CreateThread(nullptr, 0, gameThreadProc, s, 0, nullptr);
    if (h) CloseHandle(h);
}

BOOL CALLBACK enumCloseProc(HWND h, LPARAM l)
{
    DWORD wp = 0;
    GetWindowThreadProcessId(h, &wp);
    if (wp == (DWORD)l) PostMessageA(h, WM_CLOSE, 0, 0);
    return TRUE;
}

void requestGameClose(DWORD pid)
{
    if (!pid) return;
    EnumWindows(enumCloseProc, (LPARAM)pid);
}

void stopGame()
{
    HANDLE h = g_gameProc;
    if (!h) return;
    DWORD pid = g_gamePid;
    requestGameClose(pid);
    if (WaitForSingleObject(h, 5000) == WAIT_TIMEOUT) {
        TerminateProcess(h, 0);
        WaitForSingleObject(h, 3000);
    }
}

struct SilentCtx { std::string root; };

DWORD WINAPI silentUpdateThreadProc(LPVOID p)
{
    SilentCtx* c = (SilentCtx*)p;
    ensureLatest(c->root, false);
    delete c;
    if (g_hubWnd) PostMessageA(g_hubWnd, WM_APP_STATUS, 0, 0);
    return 0;
}

// ── Self-install + self-update ────────────────────────────────

void spawnLauncher(const std::string& exePath, const std::string& args,
                   const std::string& cwd)
{
    std::string cli = "\"" + exePath + "\"" + (args.empty() ? "" : " " + args);
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    CreateProcessA(nullptr, &cli[0], nullptr, nullptr, FALSE, 0, nullptr,
                   cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    if (pi.hProcess) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}

// Copy the running launcher into %LOCALAPPDATA%\MiMITA\launcher and relaunch
// from there so self-updates have a stable home. Returns true to exit the
// current (temporary) instance.
bool selfInstallToHome(const std::string& args)
{
    std::string home = launcherHomePath();
    std::string cur = currentExePath();
    if (_stricmp(cur.c_str(), home.c_str()) == 0) return false;
    CreateDirectoryA(launcherHomeDir().c_str(), nullptr);
    std::string tmp = launcherHomeDir() + "\\MimitaLauncher.new.exe";
    if (!CopyFileA(cur.c_str(), tmp.c_str(), FALSE)) return false;
    DeleteFileA((launcherHomeDir() + "\\MimitaLauncher.old.exe").c_str());
    MoveFileExA(tmp.c_str(), home.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (g_hSingleton) { CloseHandle(g_hSingleton); g_hSingleton = nullptr; }
    spawnLauncher(home, args, launcherHomeDir());
    return true;
}

// Zero-click launcher self-update: download the new exe, swap it in, spawn it,
// exit. Returns true when the caller should terminate.
bool performSelfUpdate(const ReleaseInfo& info, const std::string& args)
{
    std::string home = launcherHomePath();
    if (_stricmp(currentExePath().c_str(), home.c_str()) != 0) return false;
    if (info.launcherUrl.empty() || info.launcherVersion.empty()) return false;
    if (versionCompare(LAUNCHER_VERSION, info.launcherVersion) >= 0) return false;

    std::string newPath = launcherHomeDir() + "\\MimitaLauncher.new.exe";
    DeleteFileA(newPath.c_str());
    if (!downloadFileTo(info.launcherUrl, newPath)) return false;
    if (!isValidPeExecutable(newPath)) { DeleteFileA(newPath.c_str()); return false; }
    if (!info.launcherSha256.empty()) {
        std::string h;
        if (sha256File(newPath, h) && h != info.launcherSha256) {
            DeleteFileA(newPath.c_str());
            return false;
        }
    }
    DeleteFileA((launcherHomeDir() + "\\MimitaLauncher.old.exe").c_str());
    MoveFileExA(home.c_str(), (launcherHomeDir() + "\\MimitaLauncher.old.exe").c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    MoveFileExA(newPath.c_str(), home.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    std::string newArgs = args;
    if (newArgs.find("--post-upgrade") == std::string::npos) {
        if (!newArgs.empty()) newArgs += " ";
        newArgs += "--post-upgrade " + std::to_string((unsigned long)GetCurrentProcessId());
    }
    if (g_hSingleton) { CloseHandle(g_hSingleton); g_hSingleton = nullptr; }
    spawnLauncher(home, newArgs, launcherHomeDir());
    return true;
}

// ── Tray icon ─────────────────────────────────────────────────

void destroyTrayIcon()
{
    if (g_trayWnd) {
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
    }
}

void createTrayIcon(HINSTANCE hInst)
{
    if (g_trayWnd) return;
    WNDCLASSA wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "MimitaTrayWindow";
    RegisterClassA(&wc);
    g_trayWnd = CreateWindowExA(0, "MimitaTrayWindow", "", WS_POPUP,
                                0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (!g_trayWnd) return;
    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_trayWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYICON;
    g_nid.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    lstrcpynA(g_nid.szTip, "MiMITA Launcher", sizeof(g_nid.szTip));
    Shell_NotifyIconA(NIM_ADD, &g_nid);
}

void showTrayMenu()
{
    HMENU m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, 1, "Play MiMITA");
    AppendMenuA(m, MF_STRING | (g_gameProc ? MF_ENABLED : MF_GRAYED), 2, "Stop MiMITA");
    AppendMenuA(m, MF_STRING, 3, "Show MiMITA Launcher");
    AppendMenuA(m, MF_STRING, 4, "Check for updates");
    AppendMenuA(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(m, MF_STRING, 5, "Exit MiMITA Launcher");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_trayWnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_trayWnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
    case 1: playFromTray(); break;
    case 2: stopGame(); break;
    case 3: showHub(); break;
    case 4: runVisibleLaunchFlow(currentRoot(), false); break;
    case 5:
        if (g_noErrorDialogs || MessageBoxA(nullptr,
                "Are you sure you want to close MiMITA Launcher?",
                "MiMITA Launcher", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
            g_quit = true;
            PostQuitMessage(0);
        }
        break;
    }
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_APP_TRAYICON) {
        switch (LOWORD(lParam)) {
        case WM_LBUTTONDBLCLK:
            if (g_gameProc) showHub();
            else playFromTray();
            return 0;
        case WM_LBUTTONUP:
            showHub();
            return 0;
        case WM_RBUTTONUP:
            showTrayMenu();
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ── Hub window ────────────────────────────────────────────────

LRESULT CALLBACK HubWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_APP_SHOW_HUB:
    case WM_APP_STATUS:
        refreshHub();
        if (msg == WM_APP_SHOW_HUB) showHub();
        return 0;
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lParam;
        if (di->CtlType != ODT_BUTTON) break;
        bool green = (di->CtlID == IDH_START);
        bool red = (di->CtlID == IDH_STOP);
        if (!green && !red) break;
        COLORREF fill = green ? RGB(0, 160, 60) : RGB(205, 40, 40);
        HDC hdc = di->hDC;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        HGDIOBJ oldBr = SelectObject(hdc, br);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        RECT rc = di->rcItem;
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);
        DeleteObject(br);

        wchar_t label[64] = { 0 };
        GetWindowTextW(di->hwndItem, label, 63);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, g_hubBoldFont);
        RECT tr = rc;
        DrawTextW(hdc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return TRUE;
    }
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        HWND ctrl = (HWND)lParam;
        if (ctrl == g_hubStartBtn) {
            SetBkColor(hdc, RGB(0, 160, 60));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)g_hubGreenBrush;
        }
        if (ctrl == g_hubStopBtn) {
            SetBkColor(hdc, RGB(205, 40, 40));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)g_hubRedBrush;
        }
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDH_START) { playFromTray(); return 0; }
        if (id == IDH_STOP) { stopGame(); return 0; }
        if (id == IDH_AUTOSTART) {
            setAutoStartEnabled(Button_GetCheck((HWND)lParam) == BST_CHECKED);
            return 0;
        }
        if (id == IDH_CLOSE) {
            if (g_noErrorDialogs || MessageBoxA(hwnd,
                    "Are you sure you want to close MiMITA Launcher?",
                    "MiMITA Launcher", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
                g_quit = true;
                PostQuitMessage(0);
            }
            return 0;
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_F4 && (GetKeyState(VK_MENU) & 0x8000))
            g_altF4Pending = true;
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            if (g_altF4Pending) {
                g_altF4Pending = false;
                g_quit = true;
                PostQuitMessage(0);
            } else {
                ShowWindow(hwnd, SW_HIDE); // X button minimizes to tray
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE); // X button minimizes to tray
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

HWND createHubWindow(HINSTANCE hInst)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = HubWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MimitaHubWindow";
    RegisterClassA(&wc);

    int W = 360, H = 440;
    int x = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    HWND hwnd = CreateWindowExA(0, "MimitaHubWindow", "MiMITA",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, W, H, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return nullptr;

    g_hubBoldFont = createArialFont(16);
    HWND title = CreateWindowA("STATIC", "MiMITA", WS_CHILD | WS_VISIBLE,
        20, 12, 320, 26, hwnd, nullptr, hInst, nullptr);
    SendMessageA(title, WM_SETFONT, (WPARAM)g_hubBoldFont, TRUE);

    HWND acc = CreateWindowA("STATIC", "Hey, Guest", WS_CHILD | WS_VISIBLE,
        20, 44, 320, 20, hwnd, (HMENU)(INT_PTR)IDH_ACCOUNT, hInst, nullptr);
    setControlFont(acc);

    HWND newsLbl = CreateWindowA("STATIC", "Notifications", WS_CHILD | WS_VISIBLE,
        20, 72, 320, 18, hwnd, nullptr, hInst, nullptr);
    setControlFont(newsLbl);

    HWND news = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        20, 94, 320, 118, hwnd, (HMENU)(INT_PTR)IDH_NEWS, hInst, nullptr);
    setControlFont(news);

    HWND build = CreateWindowA("STATIC", "Build:", WS_CHILD | WS_VISIBLE,
        20, 220, 320, 18, hwnd, (HMENU)(INT_PTR)IDH_BUILD, hInst, nullptr);
    setControlFont(build);

    HWND status = CreateWindowA("STATIC", "Status: Not running", WS_CHILD | WS_VISIBLE,
        20, 242, 320, 18, hwnd, (HMENU)(INT_PTR)IDH_STATUS, hInst, nullptr);
    setControlFont(status);

    g_hubStartBtn = CreateWindowA("BUTTON", "START MiMITA",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        20, 270, 320, 48, hwnd, (HMENU)(INT_PTR)IDH_START, hInst, nullptr);
    SendMessageA(g_hubStartBtn, WM_SETFONT, (WPARAM)g_hubBoldFont, TRUE);

    g_hubStopBtn = CreateWindowA("BUTTON", "STOP MiMITA",
        WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        20, 270, 320, 48, hwnd, (HMENU)(INT_PTR)IDH_STOP, hInst, nullptr);
    SendMessageA(g_hubStopBtn, WM_SETFONT, (WPARAM)g_hubBoldFont, TRUE);

    HWND chk = CreateWindowA("BUTTON",
        "Start MiMITA Launcher when my computer turns on",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 332, 320, 24, hwnd, (HMENU)(INT_PTR)IDH_AUTOSTART, hInst, nullptr);
    setControlFont(chk);
    Button_SetCheck(chk, isAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);

    HWND closeLink = CreateWindowA("BUTTON", "Close MiMITA Launcher",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_FLAT,
        20, 372, 200, 26, hwnd, (HMENU)(INT_PTR)IDH_CLOSE, hInst, nullptr);
    setControlFont(closeLink);

    return hwnd;
}

void launchGame(const std::string& exePath, const std::string& workDir)
{
    std::string sessionArg;
    {
        std::string authJson = readFile(workDir + "\\config\\auth-token.json");
        std::string token = extractJsonStr(authJson, "session_token");
        if (!token.empty())
            sessionArg = " --session \"" + token + "\"";
    }

    // Hide launcher window before starting the game
    if (g_hWnd) ShowWindow(g_hWnd, SW_HIDE);
    pumpMessages();

    if (!isValidPeExecutable(exePath)) {
        // Never hand a non-PE file to CreateProcessA: Windows would pop a
        // modal "Unsupported 16-Bit Application" dialog and block this thread.
        reportLaunchFailure(workDir, exePath,
            "not a valid Windows executable (PE header check failed)", 193);
        notifyStatus();
        return;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cli = exePath + sessionArg;
    bool started = CreateProcessA(nullptr, &cli[0], nullptr, nullptr, FALSE, 0,
                                  nullptr, workDir.c_str(), &si, &pi);

    if (!started) {
        DWORD err = GetLastError();
        reportLaunchFailure(workDir, exePath, win32ErrorMessage(err), err);
        notifyStatus();
        return;
    }

    g_gameProc = pi.hProcess;
    g_gamePid = pi.dwProcessId;
    g_hubNote = "MiMITA is running. Click Stop to close it.";
    notifyStatus();

    DWORD start = GetTickCount();

    // Wait for game to exit (zero CPU — kernel wait)
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    DWORD elapsed = GetTickCount() - start;

    // On abnormal exit (crash), write diagnostics
    if (exitCode != 0 && elapsed > 1000)
    {
        std::string ts = std::to_string(GetTickCount());

        // Ensure logs directory exists
        std::string logDir = workDir + "\\launcher-data\\logs";
        std::string dumpDir = workDir + "\\launcher-data\\dumps";
        CreateDirectoryA((workDir + "\\launcher-data").c_str(), nullptr);
        CreateDirectoryA(logDir.c_str(), nullptr);
        CreateDirectoryA(dumpDir.c_str(), nullptr);

        // Write text crash log (launcher-side summary)
        std::string logPath = logDir + "\\crash-" + ts + ".txt";
        std::string version = readFile(workDir + "\\version.txt");
        if (version.empty()) version = "unknown";
        writeCrashLog(logPath, version, exitCode, elapsed);

        // Verify log file is non-zero
        bool logOk = false;
        HANDLE hCheck = CreateFileA(logPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hCheck != INVALID_HANDLE_VALUE) {
            DWORD hi = 0;
            logOk = GetFileSize(hCheck, &hi) > 0;
            CloseHandle(hCheck);
        }
        if (!logOk) DeleteFileA(logPath.c_str());

        // Send lightweight analytics event (tiny JSON, ~200 bytes)
        sendCrashReport(version, exitCode, elapsed);

        // Brief console output + notification
        printf("[LAUNCHER] Game crashed. Exit code=0x%08lx uptime=%lums\n",
               (unsigned long)exitCode, (unsigned long)elapsed);
        if (logOk) printf("[LAUNCHER] Log: %s\n", logPath.c_str());
        else printf("[LAUNCHER] Log FAILED to write\n");
        fflush(stdout);

        // Inspect the game's crash directory and verify artifacts actually exist
        char crashDirBuf[MAX_PATH];
        std::string crashDir;
        if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, crashDirBuf) == S_OK) {
            crashDir = std::string(crashDirBuf) + "\\MiMITA\\crashes";
        }

        // The game names artifacts crash-<time>-<pid>.txt/.dmp. Check for any
        // nonzero file matching those prefixes so we never claim a save that
        // did not happen.
        bool txtArtifactOk = false;
        bool dmpArtifactOk = false;
        WIN32_FIND_DATAA fd{};
        std::string search = crashDir + "\\crash-*.txt";
        HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string p = crashDir + "\\" + fd.cFileName;
                HANDLE hf = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hf != INVALID_HANDLE_VALUE) {
                    DWORD hi = 0;
                    if (GetFileSize(hf, &hi) > 0) { txtArtifactOk = true; }
                    CloseHandle(hf);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
        search = crashDir + "\\crash-*.dmp";
        hFind = FindFirstFileA(search.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string p = crashDir + "\\" + fd.cFileName;
                HANDLE hf = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hf != INVALID_HANDLE_VALUE) {
                    DWORD hi = 0;
                    if (GetFileSize(hf, &hi) > 0) { dmpArtifactOk = true; }
                    CloseHandle(hf);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        char hex[16];
        snprintf(hex, sizeof(hex), "0x%08lx", (unsigned long)exitCode);
        std::string details = std::string(hex) + ", uptime=" + std::to_string(elapsed) + " ms";

        // Offer to restore the previous version when one exists.
        std::string root = workDir;
        auto slash = root.find_last_of("\\/");
        if (slash != std::string::npos) root = root.substr(0, slash);
        slash = root.find_last_of("\\/");
        if (slash != std::string::npos) root = root.substr(0, slash);
        std::string activeTag = getActiveVersion(root);
        std::string prevTag;
        std::string versionsRoot = root + "\\versions";
        std::string search2 = versionsRoot + "\\v*";
        WIN32_FIND_DATAA fd2{};
        HANDLE hFind2 = FindFirstFileA(search2.c_str(), &fd2);
        if (hFind2 != INVALID_HANDLE_VALUE) {
            do {
                if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string name = fd2.cFileName;
                    if (name.size() > 1 && name[0] == 'v') {
                        std::string tag = name.substr(1);
                        if (tag != activeTag &&
                            (prevTag.empty() || versionCompare(tag, prevTag) > 0))
                            prevTag = tag;
                    }
                }
            } while (FindNextFileA(hFind2, &fd2));
            FindClose(hFind2);
        }

        CrashRecoveryAction action = showCrashRecoveryDialog(crashDir, details,
                                                             txtArtifactOk, dmpArtifactOk,
                                                             !prevTag.empty());

        if (action == CRASH_RECOVER_ROLLBACK && !prevTag.empty())
        {
            setActiveVersion(root, prevTag);
            g_hubNote = "Restored MiMITA v" + prevTag + " (the new version had a problem).";
            showTrayBalloon("MiMITA", "Restored v" + prevTag + " after a crash.");
            notifyStatus();
        }
        else if (action == CRASH_RECOVER_OPEN_FOLDER && !crashDir.empty())
            ShellExecuteA(nullptr, "open", crashDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else if (action == CRASH_RECOVER_COPY)
        {
            std::string copyText = "Mimita crash report\n"
                "====================\n"
                "Exit code: " + std::string(hex) + "\n"
                "Uptime: " + std::to_string(elapsed) + " ms\n"
                "Crash folder: " + crashDir + "\n"
                "Launcher log: " + logPath + "\n"
                "Text report saved: " + (txtArtifactOk ? "yes" : "no") + "\n"
                "Minidump saved: " + (dmpArtifactOk ? "yes" : "no") + "\n";
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, copyText.size() + 1);
                if (mem) {
                    void* p = GlobalLock(mem);
                    memcpy(p, copyText.c_str(), copyText.size() + 1);
                    GlobalUnlock(mem);
                    SetClipboardData(CF_TEXT, mem);
                }
                CloseClipboard();
            }
        }
        else if (action == CRASH_RECOVER_RESTART)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            g_gameProc = nullptr;
            g_gamePid = 0;
            notifyStatus();
            launchGame(exePath, workDir);
            return;
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    g_gameProc = nullptr;
    g_gamePid = 0;
    g_hubNote = "MiMITA closed. Click Start to play again.";
    notifyStatus();
}

// ── New: Shortcut creation ────────────────────────────────────

void createShortcuts()
{
    // Shortcuts always point at the launcher (never mimita.exe directly) so
    // every launch gets the health + update checks. Prefer the stable home.
    std::string launcher = pathExists(launcherHomePath()) ? launcherHomePath()
                                                          : currentExePath();

    if (g_createStartMenu) {
        std::string smPath = getStartMenuPath();
        if (!smPath.empty()) {
            CreateDirectoryA(smPath.c_str(), nullptr);
            createShellLink(launcher, smPath + "\\Mimita.lnk", "", "Mimita");
        }
    }

    if (g_createDesktop) {
        std::string deskPath = getDesktopPath();
        if (!deskPath.empty()) {
            createShellLink(launcher, deskPath + "\\Mimita.lnk", "", "Mimita");
        }
    }
}

// ── New: Install flow (runs on page 2) ────────────────────────

void runInstall(HWND hwnd)
{
    g_installing = true;

    setStatusText("Checking for updates...");
    CreateDirectoryA(g_installDir.c_str(), nullptr);
    CreateDirectoryA((g_installDir + "\\versions").c_str(), nullptr);

    ReleaseInfo info;
    if (!g_releaseFetched) {
        g_releaseFetched = fetchReleaseInfo(info);
        g_releaseInfo = info;
    } else {
        info = g_releaseInfo;
    }

    std::string zipPath;
    std::string tag;
    if (g_useLocalZip)
    {
        // Use local ZIP instead of downloading from GitHub
        zipPath = g_localZipPath;
        tag = "dev";
    }
    else
    {
        if (!g_releaseFetched || info.zipUrl.empty()) {
            if (!g_noErrorDialogs)
                MessageBoxA(hwnd, "Could not connect to GitHub to download Mimita.\nCheck your internet connection and try again.",
                    "Download Error", MB_OK | MB_ICONERROR);
            g_installing = false;
            return;
        }
        tag = info.gameVersion.empty() ? "latest" : info.gameVersion;

        setProgress(0);
        setStatusText("Downloading...");

        char td[MAX_PATH]; GetTempPathA(MAX_PATH, td);
        zipPath = std::string(td) + "mimita-game-" + tag + ".zip";

        if (!downloadFileTo(info.zipUrl, zipPath, 0)) {
            if (!g_noErrorDialogs)
                MessageBoxA(hwnd, "Failed to download Mimita.\nCheck your internet connection and try again.",
                    "Download Error", MB_OK | MB_ICONERROR);
            g_installing = false;
            return;
        }

        if (!info.zipSha256.empty() && !g_noVerify) {
            std::string dlHash;
            if (sha256File(zipPath, dlHash) && dlHash != info.zipSha256) {
                if (!g_noErrorDialogs)
                    MessageBoxA(hwnd, "Download verification failed.\nPlease try again.",
                        "Verification Error", MB_OK | MB_ICONERROR);
                DeleteFileA(zipPath.c_str());
                g_installing = false;
                return;
            }
        }
    }

    setProgress(85);
    setStatusText("Extracting...");

    if (!applyGameUpdate(g_installDir, tag, zipPath)) {
        if (!g_noErrorDialogs)
            MessageBoxA(hwnd, "Failed to extract files.\nThe download may be corrupted.",
                "Extraction Error", MB_OK | MB_ICONERROR);
        if (!g_useLocalZip) DeleteFileA(zipPath.c_str());
        g_installing = false;
        return;
    }
    // extractZipFileWithProgress updates progress 85→98 per-file

    // Only delete the ZIP if we downloaded it (not a local file)
    if (!g_useLocalZip) DeleteFileA(zipPath.c_str());

    saveInstallConfig(g_installDir);
    g_gameExePath = gameExeForRoot(g_installDir);

    setProgress(98);
    setStatusText("Creating shortcuts...");
    createShortcuts();

    if (Button_GetCheck(GetDlgItem(g_hWnd, IDC_CHK_AUTOSTART)) == BST_CHECKED)
        setAutoStartEnabled(true);

    setProgress(100);
    setStatusText("Complete!");
    Sleep(600);
    pumpMessages();
    g_installing = false;

    // Launch the game and drop into the tray.
    if (isValidPeExecutable(g_gameExePath)) {
        if (g_hWnd) ShowWindow(g_hWnd, SW_HIDE);
        startGameInThread(g_installDir);
    } else {
        reportLaunchFailure(g_installDir, g_gameExePath, "install produced an unusable game", 0);
    }
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
            EnableWindow(g_pageControls[0][8], FALSE);
            createPage2();
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
    case WM_KEYDOWN:
        if (wParam == VK_F4 && (GetKeyState(VK_MENU) & 0x8000))
            g_altF4Pending = true;
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            if (g_altF4Pending) {
                g_altF4Pending = false;
                g_quit = true;
                PostQuitMessage(0);
            } else if (!g_installing) {
                ShowWindow(hwnd, SW_HIDE); // X button hides to tray
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!g_installing)
            ShowWindow(hwnd, SW_HIDE); // X button hides to tray
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
    // Never let the OS pop its own critical-error dialogs (e.g. the
    // "Unsupported 16-Bit Application" box) while this launcher is running.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);

    std::string cmdLine = lpCmdLine ? lpCmdLine : "";
    std::string origArgs = cmdLine;

    // ── Parse command-line flags ──────────────────────────────
    bool trayMode = hasFlag(cmdLine, "--tray");
    if (hasFlag(cmdLine, "--help")) {
        printHelp();
        return 0;
    }
    if (hasFlag(cmdLine, "--local-zip")) {
        g_localZipPath = getFlagValue(cmdLine, "--local-zip");
        if (g_localZipPath.empty()) {
            MessageBoxA(nullptr, "--local-zip requires a path argument.\n\nExample:\n  MimitaLauncher.exe --local-zip mimita-game.zip",
                "Command-Line Error", MB_OK | MB_ICONERROR);
            return 1;
        }
        g_useLocalZip = true;
        if (hasFlag(cmdLine, "--no-verify")) g_noVerify = true;
    }
    if (hasFlag(cmdLine, "--no-verify") && !g_useLocalZip) {
        g_noVerify = true;
    }
    if (hasFlag(cmdLine, "--no-error-dialogs")) {
        g_noErrorDialogs = true;
    }
    if (hasFlag(cmdLine, "--release-json")) {
        g_releaseJsonPath = getFlagValue(cmdLine, "--release-json");
    }

    // Internal: clean up the swapped-out launcher after a self-update. Wait for
    // the old instance to fully exit (it spawned us), then delete old.exe.
    if (hasFlag(cmdLine, "--post-upgrade")) {
        std::string oldPid = getFlagValue(cmdLine, "--post-upgrade");
        if (!oldPid.empty()) {
            DWORD pid = (DWORD)atoi(oldPid.c_str());
            HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
            if (h) {
                WaitForSingleObject(h, 5000);
                TerminateProcess(h, 0);
                CloseHandle(h);
            }
        }
        std::string oldPath = appDir() + "\\MimitaLauncher.old.exe";
        for (int attempt = 0; attempt < 25; ++attempt) {
            if (DeleteFileA(oldPath.c_str()) || GetFileAttributesA(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES)
                break;
            Sleep(200);
        }
    }

    // Auto-detect a local mimita-game.zip beside this launcher. In dev mode
    // the zip sits beside the launcher exe, so use it instead of downloading
    // from GitHub. End users never have a zip beside the installed launcher,
    // so they always get the release ZIP from GitHub.
    if (!g_useLocalZip) {
        std::string localZipPath = appDir() + "\\mimita-game.zip";
        if (pathExists(localZipPath)) {
            g_useLocalZip = true;
            g_localZipPath = localZipPath;
        }
    }

    // ── Single-instance guard ─────────────────────────────────
    g_hSingleton = CreateMutexA(nullptr, FALSE, "MimitaLauncherSingleton");
    if (g_hSingleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another launcher is running. Surface its hub window and exit so
        // launchers never stack up.
        HWND hub = FindWindowA("MimitaHubWindow", nullptr);
        if (hub) {
            PostMessageA(hub, WM_APP_SHOW_HUB, 0, 0);
            ShowWindow(hub, SW_SHOW);
            SetForegroundWindow(hub);
        }
        CloseHandle(g_hSingleton);
        return 0;
    }

    DeleteFileA((appDir() + "\\MimitaLauncher.old.exe").c_str());

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
                    std::string root;
                    if (!loadInstallConfig(root)) root = getDefaultInstallDir();
                    if (!sessionToken.empty()) storeSessionToken(root, sessionToken);
                    storeSessionToken(root, exchangeToken);
                }
            }
        }
        // Continue to launch if installed, or show wizard
    }

    // ── Self-install + self-update (skipped in dev mode) ──────
    if (!isDevMode()) {
        if (selfInstallToHome(origArgs)) return 0;
        g_releaseFetched = fetchReleaseInfo(g_releaseInfo);
        if (g_releaseFetched && performSelfUpdate(g_releaseInfo, origArgs)) return 0;
    } else {
        // Dev mode: still honor --release-json so self-update can be tested.
        g_releaseFetched = fetchReleaseInfo(g_releaseInfo);
    }

    // ── Initialize COM, GDI+, common controls, fonts ──────────
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ULONG_PTR gdipToken;
    GdiplusStartupInput gdipInput;
    GdiplusStartup(&gdipToken, &gdipInput, nullptr);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    createArialFonts();

    // ── Create windows ────────────────────────────────────────
    g_hWnd = createWizardWindow(hInst);
    if (!g_hWnd) {
        GdiplusShutdown(gdipToken);
        CoUninitialize();
        return 1;
    }
    loadBackgroundImageFromResource();

    g_hubGreenBrush = CreateSolidBrush(RGB(0, 160, 60));
    g_hubRedBrush = CreateSolidBrush(RGB(205, 40, 40));
    createTrayIcon(hInst);
    g_hubWnd = createHubWindow(hInst);

    // ── Determine install state ───────────────────────────────
    std::string root;
    bool configured = loadInstallConfig(root);
    if (configured) {
        migrateLegacyInstall(root);
    } else {
        root = getDefaultInstallDir();
    }

    if (!configured)
    {
        // ── First run: show install wizard ─────────────────────
        g_installDir = root;
        ShowWindow(g_hWnd, SW_SHOW);
        createPage1();
        showPage(0);
    }
    else if (trayMode)
    {
        // ── Boot / tray mode: silent background update, sit in tray ──
        SilentCtx* c = new SilentCtx{ root };
        HANDLE h = CreateThread(nullptr, 0, silentUpdateThreadProc, c, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else
    {
        // ── Manual launch: visible update flow, then play ─────
        runVisibleLaunchFlow(root, true);
    }

    // ── Main message loop (tray keeps the process alive) ──────
    MSG msg;
    while (!g_quit && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // ── Cleanup ───────────────────────────────────────────────
    destroyTrayIcon();
    if (g_hubWnd) { DestroyWindow(g_hubWnd); g_hubWnd = nullptr; }
    if (g_hWnd) { DestroyWindow(g_hWnd); g_hWnd = nullptr; }
    destroyFonts();
    if (g_hubBoldFont) { DeleteObject(g_hubBoldFont); g_hubBoldFont = nullptr; }
    if (g_hubGreenBrush) DeleteObject(g_hubGreenBrush);
    if (g_hubRedBrush) DeleteObject(g_hubRedBrush);
    DeleteObject(g_blackBrush);
    delete g_bgImage;
    g_bgImage = nullptr;
    GdiplusShutdown(gdipToken);
    CoUninitialize();
    if (g_hSingleton) { CloseHandle(g_hSingleton); g_hSingleton = nullptr; }
    return 0;
}
