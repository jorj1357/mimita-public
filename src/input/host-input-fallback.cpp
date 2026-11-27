// 2026-11-15
// Transitional hot-DLL input fallback over the resident Win32 window.

#include "input/host-input-fallback.h"

#include <windows.h>

#include <cstdlib>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <algorithm>
#include <cstdint>

namespace HostInputFallback {
namespace {

static GLFWwindow* sWindow = nullptr;

HWND findGameWindow() {
    struct Search {
        DWORD pid = GetCurrentProcessId();
        HWND best = nullptr;
        long long bestArea = 0;
    } search;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            auto* search = reinterpret_cast<Search*>(param);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != search->pid || !IsWindowVisible(hwnd)) return TRUE;
            wchar_t className[64]{};
            GetClassNameW(hwnd, className, 63);
            if (wcscmp(className, L"ConsoleWindowClass") == 0) return TRUE;
            RECT rect{};
            if (!GetClientRect(hwnd, &rect)) return TRUE;
            const long long area = static_cast<long long>(rect.right - rect.left) *
                                   (rect.bottom - rect.top);
            if (area <= 0) return TRUE;
            if (area > search->bestArea) {
                search->bestArea = area;
                search->best = hwnd;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.best;
}

HWND cachedWindow() {
    static HWND cached = nullptr;
    static DWORD cachedPid = 0;
    if (cached && IsWindow(cached) && cachedPid == GetCurrentProcessId()) {
        return cached;
    }
    cachedPid = GetCurrentProcessId();
    if (sWindow) {
        if (HWND direct = glfwGetWin32Window(sWindow)) {
            if (IsWindow(direct) && IsWindowVisible(direct)) {
                cached = direct;
                return cached;
            }
        }
    }
    HWND byTitle = FindWindowA(nullptr, "mimita.exe");
    DWORD titlePid = 0;
    if (byTitle) GetWindowThreadProcessId(byTitle, &titlePid);
    if (byTitle && titlePid == cachedPid && IsWindowVisible(byTitle)) {
        cached = byTitle;
        return cached;
    }
    if (!cached) {
        // Never synthesize ALT here. cachedWindow() runs from every key, mouse
        // and focus query, many times per frame; when the handle was briefly
        // unresolved this sent ALT down/up on every call, spamming the menu key
        // and eating the keyboard (2026-11-27 user report: "pressing alt over
        // and over"). Rescan at most once per second. The explicit
        // focus_game_window command owns the one-shot ALT unlock.
        static ULONGLONG lastScan = 0;
        const ULONGLONG now = GetTickCount64();
        if (now - lastScan >= 1000)
        {
            lastScan = now;
            cached = findGameWindow();
        }
    }
    return cached;
}

bool keyDownVk(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

}  // namespace

void setGameWindow(GLFWwindow* window) { sWindow = window; }

void* gameWindowHandle() { return cachedWindow(); }

// Dev-loop clients cannot always take OS foreground focus (the dashboard
// window owns it), which made every key and mouse read return false while the
// game rendered normally. MIMITA_INPUT_GLOBAL_KEYS opts a probe client into
// reading global key state instead of the foreground window's. It is set only
// by dev launch modes, so a normal session still ignores keys typed elsewhere.
static bool globalKeysEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("MIMITA_INPUT_GLOBAL_KEYS");
        return value && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

bool gameWindowFocused() {
    if (globalKeysEnabled()) return true;
    HWND window = cachedWindow();
    return window && GetForegroundWindow() == window;
}

bool focusGameWindow() {
    HWND window = cachedWindow();
    if (!window) window = findGameWindow();
    if (!window) return false;
    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    ShowWindow(window, SW_SHOW);
    BringWindowToTop(window);
    // Windows refuses SetForegroundWindow from a process that is not already
    // foreground. Attaching to the foreground thread's input queue is the
    // documented way to make the request legitimate; without it a launched game
    // keeps keyboard input dead until the user clicks the window.
    HWND foreground = GetForegroundWindow();
    DWORD foregroundThread = foreground
                                 ? GetWindowThreadProcessId(foreground, nullptr)
                                 : 0;
    const DWORD thisThread = GetCurrentThreadId();
    const bool attached = foregroundThread != 0 &&
                          foregroundThread != thisThread &&
                          AttachThreadInput(thisThread, foregroundThread, TRUE);
    SetForegroundWindow(window);
    SetFocus(window);
    if (attached) AttachThreadInput(thisThread, foregroundThread, FALSE);
    if (GetForegroundWindow() != window)
    {
        // One-shot ALT unlock for the explicit focus command only, and no more
        // often than every 500 ms. This used to live in cachedWindow(), where
        // it ran on every input query and spammed the keyboard with ALT.
        static ULONGLONG lastAltMs = 0;
        const ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastAltMs >= 500)
        {
            lastAltMs = nowMs;
            keybd_event(VK_MENU, 0, 0, 0);
            keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
            SetForegroundWindow(window);
        }
    }
    if (GetForegroundWindow() != window)
    {
        // Last resort that always activates: minimise then restore.
        ShowWindow(window, SW_MINIMIZE);
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
    return GetForegroundWindow() == window;
}

bool mouseDown(int glfwButton) {
    HWND window = cachedWindow();
    if (!globalKeysEnabled() && (!window || GetForegroundWindow() != window))
        return false;
    if (glfwButton == 1) return keyDownVk(VK_RBUTTON);
    if (glfwButton == 2) return keyDownVk(VK_MBUTTON);
    return keyDownVk(VK_LBUTTON);
}

bool cursorPos(GLFWwindow*, double* x, double* y) {
    HWND window = cachedWindow();
    if (!window || !x || !y) return false;
    POINT point{};
    if (!GetCursorPos(&point)) return false;
    if (!ScreenToClient(window, &point)) return false;
    *x = static_cast<double>(point.x);
    *y = static_cast<double>(point.y);
    return true;
}

bool keyDown(int glfwKey) {
    HWND window = cachedWindow();
    if (!globalKeysEnabled() && (!window || GetForegroundWindow() != window))
        return false;
    if (glfwKey >= 'A' && glfwKey <= 'Z') return keyDownVk(glfwKey);
    if (glfwKey >= '0' && glfwKey <= '9') return keyDownVk(glfwKey);
    switch (glfwKey) {
        case 32: return keyDownVk(VK_SPACE);
        case 256: return keyDownVk(VK_ESCAPE);
        case 257: return keyDownVk(VK_RETURN);
        case 258: return keyDownVk(VK_TAB);
        case 259: return keyDownVk(VK_BACK);
        case 262: return keyDownVk(VK_RIGHT);
        case 263: return keyDownVk(VK_LEFT);
        case 264: return keyDownVk(VK_DOWN);
        case 265: return keyDownVk(VK_UP);
        case 340: return keyDownVk(VK_LSHIFT);
        case 344: return keyDownVk(VK_RSHIFT);
        case 341: return keyDownVk(VK_LCONTROL);
        case 345: return keyDownVk(VK_RCONTROL);
        case 96: return keyDownVk(VK_OEM_3);
        default: return false;
    }
}

}  // namespace HostInputFallback
