// 2026-11-15
// Transitional hot-DLL input fallback over the resident Win32 window.

#include "input/host-input-fallback.h"

#include <windows.h>
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
    cached = findGameWindow();
    if (!cached) {
        HWND foreground = GetForegroundWindow();
        DWORD foregroundPid = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
        if (foreground && foregroundPid == cachedPid) cached = foreground;
    }
    return cached;
}

bool keyDownVk(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

}  // namespace

void setGameWindow(GLFWwindow* window) { sWindow = window; }

void* gameWindowHandle() { return cachedWindow(); }

bool gameWindowFocused() {
    HWND window = cachedWindow();
    return window && GetForegroundWindow() == window;
}

bool focusGameWindow() {
    HWND window = cachedWindow();
    if (!window) return false;
    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    ShowWindow(window, SW_SHOW);
    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetFocus(window);
    return GetForegroundWindow() == window;
}

bool mouseDown(int glfwButton) {
    HWND window = cachedWindow();
    if (!window || GetForegroundWindow() != window) return false;
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
    if (!window || GetForegroundWindow() != window) return false;
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
