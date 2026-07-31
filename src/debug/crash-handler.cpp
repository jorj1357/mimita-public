// 07 31 2026, 00 00
/* purpose
* Installs a process-wide unhandled-exception filter that writes a text crash
* report and a minidump to %LOCALAPPDATA%\MiMITA\crashes.
* Creates the crash directory recursively at startup and verifies it with a
* probe file, so crash-time file creation never fails with ERROR_PATH_NOT_FOUND.
* Falls back to %TEMP%\MiMITA\crashes when the primary location is unusable.
* Does NOT suppress the exception — the process still terminates after the
* handler returns EXCEPTION_CONTINUE_SEARCH.
* Does NOT handle structured exceptions raised inside the handler itself.
*/
#include "debug/crash-handler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstring>

namespace {

// When enabled, the crash dialog is suppressed (for deterministic tests).
bool g_testMode = false;
// When enabled, force the TEMP fallback path (for deterministic tests).
bool g_forceTempFallback = false;

const char* exceptionCodeString(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLOAT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLOAT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "FLOAT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "FLOAT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "FLOAT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "FLOAT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        default:                                 return "UNKNOWN";
    }
}

void buildExeDir(char* buf, DWORD size)
{
    buf[0] = '\0';
    GetModuleFileNameA(nullptr, buf, size);
    buf[size - 1] = '\0';
    for (int i = (int)strlen(buf) - 1; i >= 0; --i)
        if (buf[i] == '\\' || buf[i] == '/') { buf[i] = '\0'; break; }
}

void appendPath(char* buf, DWORD size, const char* suffix)
{
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] != '\\' && suffix[0] != '\\')
    {
        if (len < size - 1) { buf[len++] = '\\'; buf[len] = '\0'; }
    }
    DWORD remaining = size - (DWORD)len - 1;
    for (int i = 0; suffix[i] && remaining > 1; ++i, --remaining)
        buf[len++] = suffix[i];
    buf[len] = '\0';
}

// Recursively create every path component of `path` (e.g. "...\MiMITA\crashes").
// Returns true if the final directory exists after the attempt.
bool createDirectoryRecursive(char* path)
{
    if (!path || !*path) return false;
    for (char* p = path + 1; *p; ++p)
    {
        if (*p != '\\' && *p != '/') continue;
        char saved = *p;
        *p = '\0';
        CreateDirectoryA(path, nullptr);
        *p = saved;
    }
    if (CreateDirectoryA(path, nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

// Verify a directory can actually hold files by creating, writing, flushing,
// closing, sizing, and deleting a small probe file.
bool verifyDirWritable(const char* dir)
{
    char probe[MAX_PATH * 2];
    _snprintf(probe, sizeof(probe), "%s\\mimita-probe-%lu.tmp", dir, GetCurrentProcessId());
    HANDLE h = CreateFileA(probe, GENERIC_WRITE, 0, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const char payload[] = "ok";
    DWORD wr = 0;
    BOOL wok = WriteFile(h, payload, (DWORD)strlen(payload), &wr, nullptr);
    BOOL fok = FlushFileBuffers(h);
    CloseHandle(h);

    bool sized = false;
    HANDLE hr = CreateFileA(probe, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hr != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz{};
        sized = GetFileSizeEx(hr, &sz) && sz.QuadPart > 0;
        CloseHandle(hr);
    }
    DeleteFileA(probe);
    return wok && fok && wr == strlen(payload) && sized;
}

// Build %LOCALAPPDATA%\MiMITA\crashes (or exeDir\crashes as a fallback) into buf.
// Returns true if the directory exists and passes the probe check.
bool buildCrashDir(char* buf, DWORD size, const char* exeDir)
{
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
    {
        appendPath(buf, size, "\\MiMITA\\crashes");
    }
    else
    {
        memcpy(buf, exeDir, strlen(exeDir) + 1);
        appendPath(buf, size, "\\crashes");
    }
    if (!createDirectoryRecursive(buf)) return false;
    return verifyDirWritable(buf);
}

// Track the exact operation that failed so the dialog is specific.
struct CrashOpResult {
    bool ok = false;
    const char* failOp = nullptr;
    char failPath[MAX_PATH * 2] = {0};
    DWORD failError = 0;

    void record(const char* op, const char* path, DWORD err)
    {
        if (ok) return; // keep the first failure
        ok = false;
        failOp = op;
        failError = err;
        if (path) { _snprintf(failPath, sizeof(failPath), "%s", path); }
    }
};

void getExceptionAddressInfo(void* address, char* moduleName, DWORD moduleNameSize,
                             char* offsetStr, DWORD offsetStrSize)
{
    moduleName[0] = '\0';
    offsetStr[0] = '\0';

    HMODULE hMod;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)address, &hMod))
    {
        GetModuleFileNameA(hMod, moduleName, moduleNameSize);
        const char* sep = strrchr(moduleName, '\\');
        if (sep) {
            size_t n = strlen(sep + 1) + 1;
            if (n <= moduleNameSize) memmove(moduleName, sep + 1, n);
        }
        uintptr_t base = (uintptr_t)hMod;
        uintptr_t addr = (uintptr_t)address;
        snprintf(offsetStr, offsetStrSize, "0x%llx", (unsigned long long)(addr - base));
    }
    else
    {
        snprintf(moduleName, moduleNameSize, "unknown");
        snprintf(offsetStr, offsetStrSize, "0x%llx", (unsigned long long)address);
    }
}

// Write a text report to crashDir\prefix.txt using crashDir\prefix.txt.tmp as a
// staging file. The final file is only produced after a successful write, flush,
// close, and a nonzero-size check. Zero-byte artifacts are never left behind.
void writeTextReport(const char* crashDir, const char* prefix,
                     const char* report, CrashOpResult& out)
{
    char tmpPath[MAX_PATH * 2];
    char finalPath[MAX_PATH * 2];
    _snprintf(tmpPath, sizeof(tmpPath), "%s\\%s.txt.tmp", crashDir, prefix);
    _snprintf(finalPath, sizeof(finalPath), "%s\\%s.txt", crashDir, prefix);

    HANDLE h = CreateFileA(tmpPath, GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { out.record("CreateFileW (text tmp)", tmpPath, GetLastError()); return; }

    DWORD wr = 0;
    if (!WriteFile(h, report, (DWORD)strlen(report), &wr, nullptr) || wr != strlen(report)) {
        DWORD err = GetLastError();
        CloseHandle(h);
        DeleteFileA(tmpPath);
        out.record("WriteFile (text)", tmpPath, err);
        return;
    }
    if (!FlushFileBuffers(h)) {
        DWORD err = GetLastError();
        CloseHandle(h);
        DeleteFileA(tmpPath);
        out.record("FlushFileBuffers (text)", tmpPath, err);
        return;
    }
    CloseHandle(h);

    bool sized = false;
    HANDLE hr = CreateFileA(tmpPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hr != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz{};
        sized = GetFileSizeEx(hr, &sz) && sz.QuadPart > 0;
        CloseHandle(hr);
    }
    if (!sized) {
        out.record("size check (text)", tmpPath, 0);
        DeleteFileA(tmpPath);
        return;
    }
    if (!MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING)) {
        DWORD err = GetLastError();
        DeleteFileA(tmpPath);
        out.record("MoveFileEx (text)", finalPath, err);
        return;
    }
    out.ok = true;
}

// Write a minidump to crashDir\prefix.dmp using crashDir\prefix.dmp.tmp as the
// staging file. Errors are attributed to the exact failing operation.
void writeMinidump(const char* crashDir, const char* prefix, EXCEPTION_POINTERS* ex,
                   CrashOpResult& out)
{
    char tmpPath[MAX_PATH * 2];
    char finalPath[MAX_PATH * 2];
    _snprintf(tmpPath, sizeof(tmpPath), "%s\\%s.dmp.tmp", crashDir, prefix);
    _snprintf(finalPath, sizeof(finalPath), "%s\\%s.dmp", crashDir, prefix);

    HANDLE h = CreateFileA(tmpPath, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { out.record("CreateFileW (minidump tmp)", tmpPath, GetLastError()); return; }

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ex;
    mei.ClientPointers = TRUE;

    BOOL wrote = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                   h, MiniDumpNormal, &mei, nullptr, nullptr);
    if (!wrote) {
        DWORD err = GetLastError();
        FlushFileBuffers(h);
        CloseHandle(h);
        DeleteFileA(tmpPath);
        out.record("MiniDumpWriteDump", tmpPath, err);
        return;
    }
    if (!FlushFileBuffers(h)) {
        DWORD err = GetLastError();
        CloseHandle(h);
        DeleteFileA(tmpPath);
        out.record("FlushFileBuffers (minidump)", tmpPath, err);
        return;
    }
    CloseHandle(h);

    bool sized = false;
    HANDLE hr = CreateFileA(tmpPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hr != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz{};
        sized = GetFileSizeEx(hr, &sz) && sz.QuadPart > 0;
        CloseHandle(hr);
    }
    if (!sized) {
        out.record("size check (minidump)", tmpPath, 0);
        DeleteFileA(tmpPath);
        return;
    }
    if (!MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING)) {
        DWORD err = GetLastError();
        DeleteFileA(tmpPath);
        out.record("MoveFileEx (minidump)", finalPath, err);
        return;
    }
    out.ok = true;
}

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ex)
{
    // Prevent recursive crashes
    static LONG volatile g_crashInProgress = 0;
    if (InterlockedCompareExchange(&g_crashInProgress, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    char exeDir[MAX_PATH];
    buildExeDir(exeDir, sizeof(exeDir));

    SYSTEMTIME st;
    GetLocalTime(&st);

    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d_%02d-%02d-%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();

    // Gather exception info
    DWORD code = ex->ExceptionRecord->ExceptionCode;
    void* address = ex->ExceptionRecord->ExceptionAddress;

    char moduleName[256];
    char offsetStr[64];
    getExceptionAddressInfo(address, moduleName, sizeof(moduleName),
                            offsetStr, sizeof(offsetStr));

    // Access violation detail
    char accessDetail[128] = "";
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
    {
        DWORD op = ex->ExceptionRecord->NumberParameters >= 2
                   ? ex->ExceptionRecord->ExceptionInformation[0] : 0;
        uintptr_t target = ex->ExceptionRecord->NumberParameters >= 2
                           ? (uintptr_t)ex->ExceptionRecord->ExceptionInformation[1] : 0;
        const char* opStr = (op == 0) ? "read" : (op == 1) ? "write" : "execute";
        snprintf(accessDetail, sizeof(accessDetail),
                 " (%s at 0x%llx)", opStr, (unsigned long long)target);
    }

    // Unique filename prefix
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "crash-%s-%lu", timeBuf, pid);

    // ── Build report text (always available, even if file writes fail) ────
    char report[4096];
    int n = snprintf(report, sizeof(report),
        "Crash Report\n"
        "============\n"
        "Timestamp (local): %s\n"
        "Process ID: %lu\n"
        "Thread ID: %lu\n"
        "Exception: %s (0x%08lx)\n"
        "Location: %s+%s\n"
        "Operation: %s\n"
        "Module: %s\n"
        "Offset: %s\n"
        "\n",
        timeBuf, pid, tid,
        exceptionCodeString(code), code,
        moduleName, offsetStr,
        accessDetail[0] ? accessDetail + 2 : "unknown",
        moduleName, offsetStr);

    // ── Pick a usable crash directory (primary, then TEMP fallback) ──────
    char crashDir[MAX_PATH];
    char tempCrashDir[MAX_PATH];
    const char* usedDir = nullptr;
    CrashOpResult dirResult;

    if (buildCrashDir(crashDir, sizeof(crashDir), exeDir) && !g_forceTempFallback)
    {
        usedDir = crashDir;
    }
    else
    {
        dirResult.record("crash dir (primary)", crashDir, GetLastError());
        char td[MAX_PATH];
        if (GetTempPathA(MAX_PATH, td) && td[0])
        {
            _snprintf(tempCrashDir, sizeof(tempCrashDir), "%sMiMITA\\crashes", td);
            if (createDirectoryRecursive(tempCrashDir) && verifyDirWritable(tempCrashDir))
                usedDir = tempCrashDir;
            else
                dirResult.record("crash dir (temp fallback)", tempCrashDir, GetLastError());
        }
    }

    bool txtOk = false;
    bool dumpOk = false;
    CrashOpResult txtResult, dumpResult;
    char savedDir[MAX_PATH * 2] = {0};

    if (usedDir)
    {
        _snprintf(savedDir, sizeof(savedDir), "%s", usedDir);
        writeTextReport(usedDir, prefix, report, txtResult);
        txtOk = txtResult.ok;
        writeMinidump(usedDir, prefix, ex, dumpResult);
        dumpOk = dumpResult.ok;
    }

    // ── Build message for user ────────────────────────────────
    char msg[4096];
    int msgLen = snprintf(msg, sizeof(msg),
        "Mimita has crashed.\n\n"
        "Exception: %s (0x%08lx)\n"
        "Location: %s+%s%s\n\n"
        "Saved to:\n"
        "%s\n",
        exceptionCodeString(code), code,
        moduleName, offsetStr, accessDetail,
        savedDir[0] ? savedDir : "(no usable crash directory)");

    if (txtOk)
        msgLen += snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Crash report: %s.txt\n", prefix);
    else
        msgLen += snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Crash report: FAILED to write (%s, error=%lu)\n",
            txtResult.failOp ? txtResult.failOp : "unknown",
            txtResult.failError);

    if (dumpOk)
        snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Minidump: %s.dmp", prefix);
    else
    {
        snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Minidump: FAILED (%s, error=%lu)",
            dumpResult.failOp ? dumpResult.failOp : "unknown",
            dumpResult.failError);
    }

    OutputDebugStringA(msg);
    // Emergency final output channel that never depends on file paths.
    OutputDebugStringW(L"[CRASH] WriteTextReport/Buffers flushed; process terminating.\n");
    if (!g_testMode)
        MessageBoxA(nullptr, msg, "Mimita Crash", MB_OK | MB_ICONERROR | MB_TASKMODAL);

    return EXCEPTION_CONTINUE_SEARCH;
}

}

void installCrashHandler()
{
    // Create the crash directory during normal startup and verify it with a
    // probe file, so the crash handler never has to create nested paths at
    // crash time (CreateDirectoryA is not recursive -> ERROR_PATH_NOT_FOUND).
    char exeDir[MAX_PATH];
    exeDir[0] = '\0';
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    exeDir[MAX_PATH - 1] = '\0';
    for (int i = (int)strlen(exeDir) - 1; i >= 0; --i)
        if (exeDir[i] == '\\' || exeDir[i] == '/') { exeDir[i] = '\0'; break; }

    char crashDir[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, crashDir) == S_OK)
    {
        char buf[MAX_PATH];
        _snprintf(buf, sizeof(buf), "%s", crashDir);
        appendPath(buf, sizeof(buf), "\\MiMITA\\crashes");
        createDirectoryRecursive(buf);
        verifyDirWritable(buf);
    }
    else
    {
        char buf[MAX_PATH];
        _snprintf(buf, sizeof(buf), "%s", exeDir);
        appendPath(buf, sizeof(buf), "\\crashes");
        createDirectoryRecursive(buf);
        verifyDirWritable(buf);
    }

    SetUnhandledExceptionFilter(crashHandler);
    _set_purecall_handler([]() {
        char buf[MAX_PATH];
        buf[0] = '\0';
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        HANDLE h = CreateFileA("crash-log.txt", GENERIC_WRITE, FILE_SHARE_READ,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            SetFilePointer(h, 0, nullptr, FILE_END);
            DWORD wr;
            const char* m = "[CRASH] Pure virtual function call";
            WriteFile(h, m, (DWORD)strlen(m), &wr, nullptr);
            WriteFile(h, "\r\n", 2, &wr, nullptr);
            CloseHandle(h);
        }
    });
    OutputDebugStringA("[CRASH] Handler installed\n");
}

void setCrashHandlerTestMode(bool enabled)
{
    g_testMode = enabled;
}

void setCrashHandlerForceTempFallback(bool enabled)
{
    g_forceTempFallback = enabled;
}
