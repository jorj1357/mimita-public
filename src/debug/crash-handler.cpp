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
#include <cstdarg>
#include <cstdlib>
#include <exception>
#include <cxxabi.h>
#include <typeinfo>

namespace {

// When enabled, the crash dialog is suppressed (for deterministic tests).
bool g_testMode = false;
// When enabled, force the TEMP fallback path (for deterministic tests).
bool g_forceTempFallback = false;

// ── Crash breadcrumbs ────────────────────────────────────────────────
// A fixed-size, heap-free ring of the most recent subsystem operations. The
// crash report dumps it so a crash names the failing subsystem even without a
// debugger. Writes are allocation-free and lock-free (single interlocked
// cursor); a torn slot during a crash is tolerated.
constexpr int kBreadcrumbCount = 32;
constexpr int kBreadcrumbLen = 200;
char g_breadcrumbs[kBreadcrumbCount][kBreadcrumbLen] = {};
volatile LONG g_breadcrumbCursor = 0;

void appendBreadcrumb(const char* subsystem, const char* body)
{
    const LONG idx = InterlockedIncrement(&g_breadcrumbCursor) - 1;
    char* slot = g_breadcrumbs[(unsigned)idx % (unsigned)kBreadcrumbCount];
    snprintf(slot, kBreadcrumbLen, "[%s] %s", subsystem ? subsystem : "?", body ? body : "");
}

void formatBreadcrumbs(char* out, DWORD size)
{
    out[0] = '\0';
    DWORD used = 0;
    const LONG written = g_breadcrumbCursor;
    const int total = written < kBreadcrumbCount ? (int)written : kBreadcrumbCount;
    for (int i = 0; i < total; ++i)
    {
        unsigned slot = (unsigned)((written - total + i) % kBreadcrumbCount);
        const char* text = g_breadcrumbs[slot];
        if (!text[0]) continue;
        int n = snprintf(out + used, size - used, "  %s\n", text);
        if (n < 0 || (DWORD)n >= size - used) break;
        used += (DWORD)n;
    }
}

// ── Best-effort symbolized stack ─────────────────────────────────────
bool g_dbgHelpInitialized = false;

void getExceptionAddressInfo(void* address, char* moduleName, DWORD moduleNameSize,
                             char* offsetStr, DWORD offsetStrSize);

// The exe's link-time (preferred) image base, read from the on-disk PE header at
// startup. Windows relocates the in-memory header under ASLR, so this is the
// only reliable value for offline `addr2line -e mimita.exe 0x<base + rva>`.
uintptr_t g_preferredExeBase = 0;

void capturePreferredExeBase()
{
    if (g_preferredExeBase) return;
    char path[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD read = 0;
    IMAGE_DOS_HEADER dos{};
    if (ReadFile(h, &dos, sizeof(dos), &read, nullptr) && read == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE)
    {
        SetFilePointer(h, dos.e_lfanew, nullptr, FILE_BEGIN);
        DWORD signature = 0;
        IMAGE_FILE_HEADER fileHeader{};
        IMAGE_OPTIONAL_HEADER64 optional{};
        if (ReadFile(h, &signature, sizeof(signature), &read, nullptr) &&
            signature == IMAGE_NT_SIGNATURE &&
            ReadFile(h, &fileHeader, sizeof(fileHeader), &read, nullptr) &&
            ReadFile(h, &optional, sizeof(optional), &read, nullptr))
        {
            g_preferredExeBase = static_cast<uintptr_t>(optional.ImageBase);
        }
    }
    CloseHandle(h);
}

uintptr_t modulePreferredBase(HMODULE module)
{
    if (!module) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const char*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return static_cast<uintptr_t>(nt->OptionalHeader.ImageBase);
}

void appendSymbolizedStack(char* out, DWORD size, CONTEXT* context)
{
    if (!context) return;

#ifdef _WIN64
    if (!g_dbgHelpInitialized)
    {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        g_dbgHelpInitialized = true;
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    DWORD used = (DWORD)strlen(out);
    for (int i = 0; i < 32; ++i)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        char storage[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        DWORD64 displacement = 0;
        int n;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
        {
            // Demangle C++ names so the failing function is readable.
            const char* name = symbol->Name;
            char demangled[256];
            if (name[0] == '_' && name[1] == 'Z')
            {
                size_t demLen = 0;
                int demStatus = 0;
                if (char* dem = abi::__cxa_demangle(name, nullptr, &demLen, &demStatus))
                {
                    snprintf(demangled, sizeof(demangled), "%s", dem);
                    free(dem);
                    name = demangled;
                }
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line))
                n = snprintf(out + used, size - used, "  %02d %s  %s:%lu\n",
                             i, name, line.FileName, line.LineNumber);
            else
                n = snprintf(out + used, size - used, "  %02d %s+0x%llx\n",
                             i, name, (unsigned long long)displacement);
        }
        else
        {
            // dbghelp cannot read MinGW DWARF for our own frames; fall back to
            // module+offset and the link-time address, which can be resolved
            // offline with `addr2line -e <module> 0x<link address>`.
            char moduleName[128];
            char offsetStr[64];
            getExceptionAddressInfo((void*)(uintptr_t)frame.AddrPC.Offset,
                                    moduleName, sizeof(moduleName),
                                    offsetStr, sizeof(offsetStr));
            HMODULE module = nullptr;
            uintptr_t linkAddress = frame.AddrPC.Offset;
            if (GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCSTR)(uintptr_t)frame.AddrPC.Offset, &module) && module)
            {
                const bool isMainExe = (module == GetModuleHandleA(nullptr));
                const uintptr_t preferred =
                    isMainExe && g_preferredExeBase ? g_preferredExeBase
                                                    : modulePreferredBase(module);
                if (preferred && preferred != (uintptr_t)module)
                    linkAddress = preferred + (frame.AddrPC.Offset -
                                               (uintptr_t)module);
            }
            n = snprintf(out + used, size - used, "  %02d %s+%s (link 0x%llx)\n",
                         i, moduleName, offsetStr,
                         (unsigned long long)linkAddress);
        }
        if (n < 0 || (DWORD)n >= size - used) break;
        used += (DWORD)n;
    }
#endif
}

// MinGW/GCC raises C++ exceptions through SEH with this code
// (_Unwind_RaiseException -> RaiseException). It is not a Windows SEH code.
constexpr DWORD kStatusGccThrow = 0x20474343u;

const char* exceptionCodeString(DWORD code)
{
    switch (code) {
        case kStatusGccThrow:                    return "CXX_EXCEPTION_GCC";
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

// Shared tail for both the SEH handler and the std::terminate handler: choose
// a usable crash directory, write the text report (and minidump when a
// faulting context exists), and show/dump the user message.
void emitCrashReport(const char* prefix, const char* report, const char* summary,
                     EXCEPTION_POINTERS* ex)
{
    char exeDir[MAX_PATH];
    buildExeDir(exeDir, sizeof(exeDir));

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
        if (ex)
        {
            writeMinidump(usedDir, prefix, ex, dumpResult);
            dumpOk = dumpResult.ok;
        }
    }

    char msg[4096];
    int msgLen = snprintf(msg, sizeof(msg),
        "Mimita has crashed.\n\n"
        "%s\n\n"
        "Saved to:\n"
        "%s\n",
        summary, savedDir[0] ? savedDir : "(no usable crash directory)");

    if (txtOk)
        msgLen += snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Crash report: %s.txt\n", prefix);
    else
        msgLen += snprintf(msg + msgLen, sizeof(msg) - msgLen,
            "Crash report: FAILED to write (%s, error=%lu)\n",
            txtResult.failOp ? txtResult.failOp : "unknown",
            txtResult.failError);

    if (ex)
    {
        if (dumpOk)
            snprintf(msg + msgLen, sizeof(msg) - msgLen, "Minidump: %s.dmp", prefix);
        else
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
}

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ex)
{
    // Prevent recursive crashes
    static LONG volatile g_crashInProgress = 0;
    if (InterlockedCompareExchange(&g_crashInProgress, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

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
    char crumbs[2048];
    formatBreadcrumbs(crumbs, sizeof(crumbs));
    char stack[4096] = "";
    appendSymbolizedStack(stack, sizeof(stack), ex->ContextRecord);

    const char* cxxNote = (code == kStatusGccThrow)
        ? "Note: uncaught C++ exception (MinGW STATUS_GCC_THROW). The C++ type and\n"
          "      message are not exposed through SEH; the recent-activity\n"
          "      breadcrumbs below name the failing subsystem. Resolve stack\n"
          "      frames offline: addr2line -e mimita.exe 0x<link address>.\n\n"
        : "";

    char report[8192];
    snprintf(report, sizeof(report),
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
        "\n%s"
        "Recent activity (oldest first):\n%s\n"
        "Stack (most recent call first):\n%s\n",
        timeBuf, pid, tid,
        exceptionCodeString(code), code,
        moduleName, offsetStr,
        accessDetail[0] ? accessDetail + 2 : "unknown",
        moduleName, offsetStr,
        cxxNote,
        crumbs[0] ? crumbs : "  (none)\n",
        stack[0] ? stack : "  (unavailable)\n");

    char summary[512];
    snprintf(summary, sizeof(summary), "Exception: %s (0x%08lx)\nLocation: %s+%s%s",
             exceptionCodeString(code), code, moduleName, offsetStr, accessDetail);

    emitCrashReport(prefix, report, summary, ex);
    return EXCEPTION_CONTINUE_SEARCH;
}

// std::terminate handler for uncaught C++ exceptions. MinGW raises these as
// STATUS_GCC_THROW (0x20474343) through KERNELBASE!RaiseException, which the
// SEH report can only see as "unknown"; here we can name the exception type
// and print the breadcrumbs + stack that identify the failing subsystem.
void terminateHandler()
{
    static LONG volatile g_terminateInProgress = 0;
    if (InterlockedCompareExchange(&g_terminateInProgress, 1, 0) != 0)
        abort();

    const char* typeName = "unknown";
    if (std::type_info* ti = abi::__cxa_current_exception_type())
        if (const char* name = ti->name())
            typeName = name;

    // Do NOT bare-rethrow here: with no active exception that recurses into
    // terminate and aborts before the report is written. rethrow_exception on
    // the captured failure is safe and local.
    char what[512] = "";
    if (std::exception_ptr failure = std::current_exception())
    {
        try { std::rethrow_exception(failure); }
        catch (const std::exception& e) { snprintf(what, sizeof(what), "%s", e.what()); }
        catch (...) {}
    }

    CONTEXT context;
    RtlCaptureContext(&context);
    char stack[4096] = "";
    appendSymbolizedStack(stack, sizeof(stack), &context);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d_%02d-%02d-%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    const DWORD pid = GetCurrentProcessId();
    const DWORD tid = GetCurrentThreadId();

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "crash-%s-%lu", timeBuf, pid);
    char crumbs[2048];
    formatBreadcrumbs(crumbs, sizeof(crumbs));

    char report[8192];
    snprintf(report, sizeof(report),
        "Crash Report (uncaught C++ exception)\n"
        "=====================================\n"
        "Timestamp (local): %s\n"
        "Process ID: %lu\n"
        "Thread ID: %lu\n"
        "Exception type: %s\n"
        "What: %s\n"
        "\nRecent activity (oldest first):\n%s\n"
        "Stack (most recent call first):\n%s\n",
        timeBuf, pid, tid, typeName, what[0] ? what : "(none)",
        crumbs[0] ? crumbs : "  (none)\n",
        stack[0] ? stack : "  (unavailable)\n");

    char summary[512];
    snprintf(summary, sizeof(summary), "Uncaught C++ exception: %s", typeName);
    emitCrashReport(prefix, report, summary, nullptr);

    abort();
}

} // namespace

void recordCrashBreadcrumb(const char* subsystem, const char* format, ...)
{
    char body[kBreadcrumbLen];
    va_list args;
    va_start(args, format);
    vsnprintf(body, sizeof(body), format, args);
    va_end(args);
    appendBreadcrumb(subsystem, body);
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

    capturePreferredExeBase();
    SetUnhandledExceptionFilter(crashHandler);
    // Uncaught C++ exceptions reach terminate without a usable SEH code
    // (MinGW raises STATUS_GCC_THROW 0x20474343). Route them through the same
    // report so the exception type, breadcrumbs, and stack are captured.
    std::set_terminate(terminateHandler);
    _set_purecall_handler([]() {
        recordCrashBreadcrumb("purecall", "pure virtual function call");
        terminateHandler();
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
