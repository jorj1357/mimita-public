#include "debug/crash-handler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

namespace {

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
    for (int i = 0; buf[i]; ++i)
        if (buf[i] == '\\' || buf[i] == '/') { int sep = i; buf[sep] = '\0'; break; }
}

void appendPath(char* buf, DWORD size, const char* suffix)
{
    size_t len = strlen(buf);
    DWORD remaining = size - (DWORD)len - 1;
    for (int i = 0; suffix[i] && remaining > 1; ++i, --remaining)
        buf[len++] = suffix[i];
    buf[len] = '\0';
}

void safeWriteLog(const char* path, const char* msg)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    DWORD wr;
    WriteFile(h, msg, (DWORD)strlen(msg), &wr, nullptr);
    WriteFile(h, "\r\n", 2, &wr, nullptr);
    CloseHandle(h);
}

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

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ex)
{
    char exeDir[MAX_PATH];
    buildExeDir(exeDir, sizeof(exeDir));

    SYSTEMTIME st;
    GetLocalTime(&st);

    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d_%02d-%02d-%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

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

    // Build log entry
    char logEntry[1024];
    snprintf(logEntry, sizeof(logEntry),
             "[CRASH] %s code=%s (0x%08lx) addr=%s+%s%s",
             timeBuf, exceptionCodeString(code), code, moduleName, offsetStr, accessDetail);

    // Write minidump
    char dumpPath[MAX_PATH * 2];
    memcpy(dumpPath, exeDir, sizeof(exeDir));
    appendPath(dumpPath, sizeof(dumpPath), "\\crash-");
    appendPath(dumpPath, sizeof(dumpPath), timeBuf);
    appendPath(dumpPath, sizeof(dumpPath), ".dmp");

    HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool dumpOk = false;
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ex;
        mei.ClientPointers = TRUE;

        dumpOk = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                   hFile, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    // Write crash log to exe directory
    char logPath[MAX_PATH * 2];
    memcpy(logPath, exeDir, sizeof(exeDir));
    appendPath(logPath, sizeof(logPath), "\\crash-log.txt");

    char logLine[2048];
    snprintf(logLine, sizeof(logLine),
             "[CRASH] %s code=%s (0x%08lx) addr=%s+%s%s dump=%s",
             timeBuf,
             exceptionCodeString(code), code,
             moduleName, offsetStr, accessDetail,
             dumpOk ? dumpPath : "(failed)");
    safeWriteLog(logPath, logLine);

    OutputDebugStringA(logEntry);

    // Show crash message to user
    char msg[4096];
    snprintf(msg, sizeof(msg),
             "Mimita has crashed.\n\n"
             "Exception: %s (0x%08lx)\n"
             "Location: %s+%s%s\n\n"
             "Crash details saved to:\n"
             "%s\n"
             "Minidump saved to:\n"
             "%s",
             exceptionCodeString(code), code,
             moduleName, offsetStr, accessDetail,
             logPath,
             dumpOk ? dumpPath : "(failed to write minidump)");

    MessageBoxA(nullptr, msg, "Mimita Crash", MB_OK | MB_ICONERROR | MB_TASKMODAL);

    return EXCEPTION_CONTINUE_SEARCH;
}

}

void installCrashHandler()
{
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
