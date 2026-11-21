"""Conservative process identity helpers for Windows orchestration.

PID access failures are not proof that a process exited. If OpenProcess cannot
inspect a PID, use the system process snapshot; if that snapshot also fails,
preserve the owner as live so callers do not reclaim its locks or spawn a twin.
"""

from __future__ import annotations

import ctypes
import datetime
import os
from ctypes import wintypes


def _windows_process_in_snapshot(pid: int) -> bool | None:
    class ProcessEntry32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", ctypes.c_long),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * 260),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE,
                                         ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE,
                                        ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if not snapshot or snapshot == ctypes.c_void_p(-1).value:
        return None
    try:
        entry = ProcessEntry32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            return False if ctypes.get_last_error() == 18 else None
        while True:
            if int(entry.th32ProcessID) == pid:
                return True
            if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                return False if ctypes.get_last_error() == 18 else None
    finally:
        kernel32.CloseHandle(snapshot)


def _windows_command_line(process_handle) -> str | None:
    """Read a process command line through the native process information API."""
    class UnicodeString(ctypes.Structure):
        _fields_ = [
            ("length", wintypes.USHORT),
            ("maximum_length", wintypes.USHORT),
            ("buffer", ctypes.c_void_p),
        ]

    ntdll = ctypes.WinDLL("ntdll", use_last_error=True)
    query = ntdll.NtQueryInformationProcess
    query.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p,
                      wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    query.restype = ctypes.c_long
    size = 4096
    for _ in range(4):
        buffer = ctypes.create_string_buffer(size)
        returned = wintypes.DWORD()
        status = int(query(process_handle, 60, ctypes.byref(buffer), size,
                           ctypes.byref(returned)))
        if status >= 0:
            value = UnicodeString.from_buffer(buffer)
            if not value.length:
                return ""
            base = ctypes.addressof(buffer)
            offset = int(value.buffer or 0) - base
            if offset < 0 or offset + int(value.length) > size:
                return None
            return ctypes.wstring_at(
                int(value.buffer), int(value.length) // ctypes.sizeof(wintypes.WCHAR))
        status_code = status & 0xFFFFFFFF
        if status_code not in (0xC0000004, 0xC0000023):
            return None
        size = max(size * 2, int(returned.value or 0))
    return None


def windows_process_inventory(
        executable_names: set[str] | None = None,
        pids: set[int] | None = None) -> list[dict]:
    """Return process identity through Win32 APIs, without WMI or psutil.

    The toolhelp snapshot is one kernel call. Process metadata is then queried
    only for matching executable names or PIDs. With no filters, retain the
    historical MiMITA-only behavior.
    """
    if os.name != "nt":
        return []
    wanted_names = ({"mimita.exe"} if executable_names is None and pids is None
                    else {str(name).casefold() for name in (executable_names or set())})
    wanted_pids = {int(pid) for pid in pids or set() if int(pid) > 0}

    class ProcessEntry32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", ctypes.c_long),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * 260),
        ]

    class FileTime(ctypes.Structure):
        _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE,
                                         ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE,
                                        ctypes.POINTER(ProcessEntry32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL,
                                     wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.QueryFullProcessImageNameW.argtypes = [
        wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
        ctypes.POINTER(wintypes.DWORD)]
    kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
    kernel32.GetProcessTimes.argtypes = [
        wintypes.HANDLE, ctypes.POINTER(FileTime), ctypes.POINTER(FileTime),
        ctypes.POINTER(FileTime), ctypes.POINTER(FileTime)]
    kernel32.GetProcessTimes.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if not snapshot or snapshot == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    inventory = []
    try:
        entry = ProcessEntry32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            error = ctypes.get_last_error()
            if error == 18:  # ERROR_NO_MORE_FILES
                return []
            raise ctypes.WinError(error)
        while True:
            pid = int(entry.th32ProcessID)
            name = str(entry.szExeFile)
            if ((wanted_pids and pid in wanted_pids) or
                    (not wanted_pids and name.casefold() in wanted_names)):
                handle = kernel32.OpenProcess(0x1000 | 0x0400, False, pid)
                if not handle:
                    error = ctypes.get_last_error()
                    if error not in (6, 87):  # exited during the scan
                        raise ctypes.WinError(error)
                else:
                    try:
                        buffer = ctypes.create_unicode_buffer(32768)
                        length = wintypes.DWORD(len(buffer))
                        created = FileTime()
                        exited = FileTime()
                        kernel = FileTime()
                        user = FileTime()
                        if (not kernel32.QueryFullProcessImageNameW(
                                handle, 0, buffer, ctypes.byref(length)) or
                                not kernel32.GetProcessTimes(
                                    handle, ctypes.byref(created), ctypes.byref(exited),
                                    ctypes.byref(kernel), ctypes.byref(user))):
                            error = ctypes.get_last_error()
                            if error not in (6, 87):
                                raise ctypes.WinError(error)
                        else:
                            filetime = ((int(created.high) << 32) |
                                        int(created.low))
                            unix_seconds = filetime / 10_000_000 - 11644473600
                            started = datetime.datetime.fromtimestamp(
                                unix_seconds, datetime.timezone.utc
                            ).isoformat(timespec="microseconds").replace("+00:00", "Z")
                            try:
                                command_line = _windows_command_line(handle)
                            except (AttributeError, OSError):
                                command_line = None
                            inventory.append({
                                "pid": pid,
                                "parent_pid": int(entry.th32ParentProcessID),
                                "executable": buffer.value,
                                "name": name,
                                "command_line": command_line,
                                "process_started_utc": started,
                            })
                    finally:
                        kernel32.CloseHandle(handle)
            if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                error = ctypes.get_last_error()
                if error == 18:  # ERROR_NO_MORE_FILES
                    break
                raise ctypes.WinError(error)
    finally:
        kernel32.CloseHandle(snapshot)
    return inventory


def pid_alive(pid: int) -> bool:
    """Return false only when Windows positively reports that a PID is gone."""
    if not isinstance(pid, int) or pid <= 0:
        return False
    if os.name != "nt":
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True
        except OSError:
            return True

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL,
                                     wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE,
                                            ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    handle = kernel32.OpenProcess(0x1000 | 0x00100000, False, pid)
    if handle:
        try:
            exit_code = wintypes.DWORD()
            if kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
                return exit_code.value == 259  # STILL_ACTIVE
        finally:
            kernel32.CloseHandle(handle)

    observed = _windows_process_in_snapshot(pid)
    # Unknown is treated as live. A false negative here can start duplicate
    # daemons/builders or let a second process reuse a live owner's lock.
    return observed is not False
