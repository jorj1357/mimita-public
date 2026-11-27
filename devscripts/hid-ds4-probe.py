#!/usr/bin/env python3
# 2026-11-27
# purpose
# Standalone read-only probe for a Sony DualShock 4 HID input report.
# Enumerates HID interfaces, filters VID 054C / PID 09CC or 05C4, opens one
# shared, and prints raw report bytes plus a DS4 parse for a bounded time.
# Does NOT modify the device, post input, or hold the device while the game runs.
# use while the game is stopped:
#   python devscripts/hid-ds4-probe.py [--seconds 4]

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time

setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid = ctypes.WinDLL("hid", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

DIGCF_PRESENT = 0x00000002
DIGCF_DEVICEINTERFACE = 0x00000010
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
OPEN_EXISTING = 3
ERROR_IO_PENDING = 997
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 258


class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", wt.DWORD),
        ("Data2", wt.WORD),
        ("Data3", wt.WORD),
        ("Data4", ctypes.c_ubyte * 8),
    ]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [
        ("cbSize", wt.DWORD),
        ("InterfaceClassGuid", GUID),
        ("Flags", wt.DWORD),
        ("Reserved", ctypes.c_size_t),
    ]


class SP_DEVICE_INTERFACE_DETAIL_DATA(ctypes.Structure):
    _fields_ = [
        ("cbSize", wt.DWORD),
        ("DevicePath", ctypes.c_wchar * 1),
    ]


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("Size", wt.ULONG),
        ("VendorID", wt.USHORT),
        ("ProductID", wt.USHORT),
        ("VersionNumber", wt.USHORT),
    ]


class HIDP_CAPS(ctypes.Structure):
    _fields_ = [
        ("Usage", wt.USHORT),
        ("UsagePage", wt.USHORT),
        ("InputReportByteLength", wt.USHORT),
        ("OutputReportByteLength", wt.USHORT),
        ("FeatureReportByteLength", wt.USHORT),
        ("Reserved", wt.USHORT * 17),
        ("NumberLinkCollectionNodes", wt.USHORT),
        ("NumberInputButtonCaps", wt.USHORT),
        ("NumberInputValueCaps", wt.USHORT),
        ("NumberInputDataIndices", wt.USHORT),
        ("NumberOutputButtonCaps", wt.USHORT),
        ("NumberOutputValueCaps", wt.USHORT),
        ("NumberOutputDataIndices", wt.USHORT),
        ("NumberFeatureButtonCaps", wt.USHORT),
        ("NumberFeatureValueCaps", wt.USHORT),
        ("NumberFeatureDataIndices", wt.USHORT),
    ]


class OVERLAPPED(ctypes.Structure):
    _fields_ = [
        ("Internal", ctypes.c_size_t),
        ("InternalHigh", ctypes.c_size_t),
        ("Offset", wt.DWORD),
        ("OffsetHigh", wt.DWORD),
        ("hEvent", wt.HANDLE),
    ]


def _bind() -> None:
    setupapi.SetupDiGetClassDevsW.argtypes = [
        ctypes.POINTER(GUID), wt.LPCWSTR, wt.HWND, wt.DWORD]
    setupapi.SetupDiGetClassDevsW.restype = wt.HANDLE
    setupapi.SetupDiEnumDeviceInterfaces.argtypes = [
        wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(GUID), wt.DWORD,
        ctypes.POINTER(SP_DEVICE_INTERFACE_DATA)]
    setupapi.SetupDiEnumDeviceInterfaces.restype = wt.BOOL
    setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [
        wt.HANDLE, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
        ctypes.POINTER(SP_DEVICE_INTERFACE_DETAIL_DATA), wt.DWORD,
        ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
    setupapi.SetupDiGetDeviceInterfaceDetailW.restype = wt.BOOL
    setupapi.SetupDiDestroyDeviceInfoList.argtypes = [wt.HANDLE]
    setupapi.SetupDiDestroyDeviceInfoList.restype = wt.BOOL

    hid.HidD_GetHidGuid.argtypes = [ctypes.POINTER(GUID)]
    hid.HidD_GetHidGuid.restype = None
    hid.HidD_GetAttributes.argtypes = [wt.HANDLE, ctypes.POINTER(HIDD_ATTRIBUTES)]
    hid.HidD_GetAttributes.restype = wt.BOOL
    hid.HidD_GetPreparsedData.argtypes = [wt.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
    hid.HidD_GetPreparsedData.restype = wt.BOOL
    hid.HidD_FreePreparsedData.argtypes = [ctypes.c_void_p]
    hid.HidD_FreePreparsedData.restype = wt.BOOL
    hid.HidP_GetCaps.argtypes = [ctypes.c_void_p, ctypes.POINTER(HIDP_CAPS)]
    hid.HidP_GetCaps.restype = wt.LONG

    kernel32.CreateFileW.argtypes = [
        wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD,
        wt.DWORD, wt.HANDLE]
    kernel32.CreateFileW.restype = wt.HANDLE
    kernel32.CloseHandle.argtypes = [wt.HANDLE]
    kernel32.CloseHandle.restype = wt.BOOL
    kernel32.CreateEventW.argtypes = [
        ctypes.c_void_p, wt.BOOL, wt.BOOL, wt.LPCWSTR]
    kernel32.CreateEventW.restype = wt.HANDLE
    kernel32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
    kernel32.WaitForSingleObject.restype = wt.DWORD
    kernel32.ReadFile.argtypes = [
        wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD),
        ctypes.POINTER(OVERLAPPED)]
    kernel32.ReadFile.restype = wt.BOOL
    kernel32.GetOverlappedResult.argtypes = [
        wt.HANDLE, ctypes.POINTER(OVERLAPPED), ctypes.POINTER(wt.DWORD),
        wt.BOOL]
    kernel32.GetOverlappedResult.restype = wt.BOOL
    kernel32.CancelIo.argtypes = [wt.HANDLE]
    kernel32.CancelIo.restype = wt.BOOL
    kernel32.SetLastError(0)


def _detail_size() -> int:
    size = ctypes.sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA)
    return size


def enumerate_ds4_paths() -> list[tuple[str, str]]:
    guid = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(guid))
    info = setupapi.SetupDiGetClassDevsW(
        ctypes.byref(guid), None, None,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if info == wt.HANDLE(-1).value or info is None:
        raise OSError(ctypes.get_last_error(), "SetupDiGetClassDevsW failed")
    found: list[tuple[str, str]] = []
    try:
        index = 0
        while True:
            iface = SP_DEVICE_INTERFACE_DATA()
            iface.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
            if not setupapi.SetupDiEnumDeviceInterfaces(
                    info, None, ctypes.byref(guid), index, ctypes.byref(iface)):
                if ctypes.get_last_error() == 259:  # ERROR_NO_MORE_ITEMS
                    break
                break
            required = wt.DWORD(0)
            setupapi.SetupDiGetDeviceInterfaceDetailW(
                info, ctypes.byref(iface), None, 0,
                ctypes.byref(required), None)
            if required.value == 0:
                index += 1
                continue
            detail = ctypes.create_string_buffer(required.value)
            ctypes.cast(detail, ctypes.POINTER(wt.DWORD))[0] = _detail_size()
            if not setupapi.SetupDiGetDeviceInterfaceDetailW(
                    info, ctypes.byref(iface),
                    ctypes.cast(detail, ctypes.POINTER(
                        SP_DEVICE_INTERFACE_DETAIL_DATA)),
                    required.value, ctypes.byref(required), None):
                index += 1
                continue
            offset = SP_DEVICE_INTERFACE_DETAIL_DATA.DevicePath.offset
            path = ctypes.wstring_at(ctypes.addressof(detail) + offset)
            handle = kernel32.CreateFileW(
                path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, None,
                OPEN_EXISTING, 0, None)
            if handle != wt.HANDLE(-1).value:
                attrs = HIDD_ATTRIBUTES()
                attrs.Size = ctypes.sizeof(HIDD_ATTRIBUTES)
                if hid.HidD_GetAttributes(handle, ctypes.byref(attrs)):
                    if attrs.VendorID == 0x054C and attrs.ProductID in (
                            0x09CC, 0x05C4):
                        found.append((path, f"{attrs.VendorID:04X}:"
                                            f"{attrs.ProductID:04X}"))
                kernel32.CloseHandle(handle)
            index += 1
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(info)
    return found


def open_device(path: str) -> wt.HANDLE:
    handle = kernel32.CreateFileW(
        path, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, None, OPEN_EXISTING, 0, None)
    if handle == wt.HANDLE(-1).value:
        # Some Bluetooth stacks only allow a read handle.
        handle = kernel32.CreateFileW(
            path, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, None, OPEN_EXISTING, 0, None)
    if handle == wt.HANDLE(-1).value:
        raise OSError(ctypes.get_last_error(), "CreateFileW failed")
    return handle


def input_report_length(handle: wt.HANDLE) -> int:
    preparsed = ctypes.c_void_p()
    if not hid.HidD_GetPreparsedData(handle, ctypes.byref(preparsed)):
        return 78
    try:
        caps = HIDP_CAPS()
        if hid.HidP_GetCaps(preparsed, ctypes.byref(caps)) != 0x00110000:
            return 78
        return max(int(caps.InputReportByteLength), 16)
    finally:
        hid.HidD_FreePreparsedData(preparsed)


def parse_ds4(report: bytes) -> str:
    if not report:
        return ""
    report_id = report[0]
    if report_id == 0x01:
        base = 1
        mode = "usb 0x01"
    elif report_id == 0x11:
        # 0x11 is followed by 0xc0 0x00, so the common block starts at 3.
        base = 3
        mode = "bt 0x11"
    else:
        return f"unknown report 0x{report_id:02X} len={len(report)}"
    if len(report) < base + 9:
        return f"{mode} short len={len(report)}"
    lx = report[base + 0]
    ly = report[base + 1]
    rx = report[base + 2]
    ry = report[base + 3]
    b0 = report[base + 4]
    b1 = report[base + 5]
    b2 = report[base + 6]
    l2 = report[base + 7]
    r2 = report[base + 8]
    dpad = b0 & 0x0F
    face = (b0 >> 4) & 0x0F
    names = []
    if face & 0x1: names.append("square")
    if face & 0x2: names.append("cross")
    if face & 0x4: names.append("circle")
    if face & 0x8: names.append("triangle")
    if b1 & 0x01: names.append("l1")
    if b1 & 0x02: names.append("r1")
    if b1 & 0x04: names.append("l2")
    if b1 & 0x08: names.append("r2")
    if b1 & 0x10: names.append("share")
    if b1 & 0x20: names.append("options")
    if b1 & 0x40: names.append("l3")
    if b1 & 0x80: names.append("r3")
    if b2 & 0x01: names.append("ps")
    if b2 & 0x02: names.append("touchpad")
    return (f"{mode} lx={lx:3d} ly={ly:3d} rx={rx:3d} ry={ry:3d} "
            f"l2={l2:3d} r2={r2:3d} dpad={dpad} "
            f"buttons=[{','.join(names) if names else '-'}]")


def read_reports(path: str, seconds: float, print_all: bool = False) -> int:
    handle = open_device(path)
    length = input_report_length(handle)
    print(f"open ok length={length}", flush=True)
    event = kernel32.CreateEventW(None, True, False, None)
    buffer = ctypes.create_string_buffer(max(length, 512))
    seen = 0
    previous = None
    deadline = time.monotonic() + seconds
    try:
        while time.monotonic() < deadline:
            overlapped = OVERLAPPED()
            overlapped.hEvent = event
            kernel32.ResetEvent(event)
            read = wt.DWORD(0)
            ok = kernel32.ReadFile(
                handle, buffer, len(buffer), ctypes.byref(read),
                ctypes.byref(overlapped))
            if not ok:
                error = ctypes.get_last_error()
                if error != ERROR_IO_PENDING:
                    print(f"ReadFile error={error}", file=sys.stderr)
                    break
                wait = kernel32.WaitForSingleObject(event, 250)
                if wait == WAIT_TIMEOUT:
                    kernel32.CancelIo(handle)
                    kernel32.WaitForSingleObject(event, 100)
                    continue
                if not kernel32.GetOverlappedResult(
                        handle, ctypes.byref(overlapped),
                        ctypes.byref(read), False):
                    continue
            data = bytes(buffer.raw[:read.value])
            seen += 1
            parsed = parse_ds4(data)
            if not print_all and parsed == previous:
                continue
            previous = parsed
            stamp = time.strftime("%H:%M:%S")
            hexdump = data[:24].hex(" ")
            print(f"{stamp} len={len(data):3d} {parsed}", flush=True)
            print(f"         raw {hexdump}{' ...' if len(data) > 24 else ''}",
                  flush=True)
    finally:
        kernel32.CloseHandle(event)
        kernel32.CloseHandle(handle)
    return seen


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=4.0)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--all", action="store_true",
                        help="print every report, not just state changes")
    args = parser.parse_args()
    _bind()
    devices = enumerate_ds4_paths()
    if not devices:
        print("no Sony DS4 HID interface (VID 054C PID 09CC/05C4) found")
        return 2
    for path, ids in devices:
        print(f"device {ids}\n  {path}")
    if args.list:
        return 0
    total = 0
    for path, _ids in devices:
        try:
            total += read_reports(path, args.seconds, args.all)
        except OSError as error:
            print(f"open failed: {error}", file=sys.stderr)
    print(f"reports={total}")
    return 0 if total else 3


if __name__ == "__main__":
    raise SystemExit(main())
