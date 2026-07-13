#include "server-info-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include "../ui-text-input.h"
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static bool launchServerProcess()
{
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char cmd[] = "mimita.exe --server";
    BOOL ok = CreateProcessA(
        nullptr, cmd,
        nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE,
        nullptr, nullptr,
        &si, &pi
    );
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("[SERVER LAUNCH] server process started PID=%lu\n", pi.dwProcessId);
    } else {
        printf("[SERVER LAUNCH] CreateProcess failed error=%lu\n", GetLastError());
    }
    return ok != FALSE;
}
#else
static bool launchServerProcess() { return false; }
#endif

namespace {

// ── Address text input state (persistent while menu is open) ─────────
UITextInputState gAddressState;

// ── IP:port character filter ─────────────────────────────────────────
bool addressCharFilter(unsigned int c)
{
    return (c >= '0' && c <= '9') || c == '.' || c == ':';
}

} // anonymous namespace

void serverInfoMenuSetActive(bool active)
{
    if (!active)
    {
        gAddressState.focused = false;
    }
}

void serverInfoMenuHandleChar(unsigned int codepoint)
{
    UITextInputOptions opts;
    opts.maxLength = 63;
    opts.characterFilter = addressCharFilter;
    uiTextInputHandleChar(gAddressState, codepoint, opts);
}

void serverInfoMenuHandleKey(int key, int action, int mods)
{
    (void)action;
    (void)mods;
    // Note: serverInfoMenuHandleKey is called from the GLFW key callback.
    // We use glfwGetCurrentContext() to get the window for clipboard ops.
    GLFWwindow* win = glfwGetCurrentContext();
    UITextInputOptions opts;
    opts.maxLength = 63;
    opts.characterFilter = addressCharFilter;
    uiTextInputHandleKey(win, gAddressState, key, action, mods, opts);
}

ServerInfoResult drawServerInfoMenu(GLFWwindow* win,
                                    char* serverAddress,
                                    bool serverRunning)
{
    ServerInfoResult r{};

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/server-info-menu.json");

    // Draw all static elements from layout
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        if (elem->type == "panel" || elem->type == "text" || elem->type == "label")
        {
            drawGuiElement(win, *elem);
            continue;
        }

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "startServerButton")
        {
            if (launchServerProcess())
                r.startServer = true;
        }
        else if (id == "connectButton")
        {
            r.connect = true;
        }
        else if (id == "backButton")
            r.goBack = true;
    }

    // ── Sync external buffer → text state on first activation ──────────
    if (!gAddressState.focused && gAddressState.value.empty() && serverAddress && serverAddress[0])
    {
        gAddressState.value = serverAddress;
        gAddressState.cursorPos = (int)gAddressState.value.size();
    }

    // ── Render reusable text input ─────────────────────────────────────
    UITextInputOptions opts;
    opts.maxLength = 63;
    opts.characterFilter = addressCharFilter;
    opts.selectAllOnFocus = true;
    uiTextInputRender(win, "serverAddress", {860.0f, 220.0f, 300.0f, 48.0f},
                      gAddressState, opts);

    // ── Sync text state → external buffer ─────────────────────────────
    if (!gAddressState.value.empty())
    {
        std::strncpy(serverAddress, gAddressState.value.c_str(), 63);
        serverAddress[63] = '\0';
    }
    else
    {
        serverAddress[0] = '\0';
    }

    // Dynamic status text
    if (serverRunning)
    {
        uiDrawText("Status: Running", cs.designToScreenX(760), cs.designToScreenY(330),
                   0.42f, {0.3f, 1.0f, 0.4f, 1.0f});
        uiDrawText("Server terminal is open in background",
                   cs.designToScreenX(760), cs.designToScreenY(365),
                   0.30f, {0.6f, 0.65f, 0.75f, 1.0f});
    }
    else
    {
        uiDrawText("Status: Not Running", cs.designToScreenX(760), cs.designToScreenY(330),
                   0.42f, {1.0f, 0.3f, 0.3f, 1.0f});
        uiDrawText("Click to start a dedicated server",
                   cs.designToScreenX(760), cs.designToScreenY(365),
                   0.30f, {0.6f, 0.65f, 0.75f, 1.0f});
    }

    return r;
}
