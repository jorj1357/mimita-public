// 07 31 2026, 00 00
/* purpose
* Shared helpers for the crash-handler test executables.
* Provides fileHasData() and localAppData() used by unit and forced-crash tests.
* Does NOT install the crash handler, trigger crashes, or modify game state.
* Does NOT open windows, sockets, or launch mimita.exe.
*/
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <string>

static bool fileHasData(const std::string& path)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD hi = 0;
    bool ok = GetFileSize(h, &hi) > 0;
    CloseHandle(h);
    return ok;
}

static std::string localAppData()
{
    char buf[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
        return std::string(buf);
    return "";
}
