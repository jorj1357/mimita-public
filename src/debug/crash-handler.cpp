#include "debug/crash-handler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

namespace {

// Stack-only dump path builder — no heap allocation
void buildDumpPath(char* buf, DWORD size)
{
    buf[0] = '\0';
    GetModuleFileNameA(nullptr, buf, size);
    buf[size - 1] = '\0';

    // Find last separator
    int sep = -1;
    for (int i = 0; buf[i]; ++i)
        if (buf[i] == '\\' || buf[i] == '/') sep = i;
    if (sep >= 0) buf[sep] = '\0';

    size_t len = strlen(buf);
    DWORD remaining = size - (DWORD)len - 1;

    auto append = [&](const char* s) {
        for (int i = 0; s[i] && remaining > 1; ++i, --remaining)
            buf[len++] = s[i];
    };

    append("\\crash-");

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    append(timeBuf);
    append(".dmp");

    buf[len] = '\0';
}

void safeLog(const char* msg)
{
    OutputDebugStringA(msg);
    HANDLE h = CreateFileA("crash-log.txt", GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, nullptr, FILE_END);
        DWORD wr;
        WriteFile(h, msg, (DWORD)strlen(msg), &wr, nullptr);
        WriteFile(h, "\r\n", 2, &wr, nullptr);
        CloseHandle(h);
    }
}

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ex)
{
    char dumpPath[MAX_PATH * 2];
    buildDumpPath(dumpPath, sizeof(dumpPath));

    HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ex;
        mei.ClientPointers = TRUE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
        safeLog("[CRASH] Minidump written");
    } else {
        safeLog("[CRASH] Failed to write minidump");
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

}

void installCrashHandler()
{
    SetUnhandledExceptionFilter(crashHandler);
    _set_purecall_handler([]() {
        safeLog("[CRASH] Pure virtual function call");
    });
    safeLog("[CRASH] Handler installed");
}
