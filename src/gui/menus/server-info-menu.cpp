#include "server-info-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-bindings.h"
#include "../gui-coord.h"
#include "../ui-system.h"
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
static bool sMenuActive = false;
}

void serverInfoMenuSetActive(bool active)
{
    sMenuActive = active;
    if (!active)
    {
        GuiBindings::instance().clearFocus();
    }
}

// Legacy forwarding functions kept for callers in input-commands.cpp.
// They are no longer necessary for the UI — input goes through guiBindingsHandleKey.
void serverInfoMenuHandleChar(unsigned int codepoint)
{
    (void)codepoint;
}

void serverInfoMenuHandleKey(int key, int action, int mods)
{
    (void)key;
    (void)action;
    (void)mods;
}

ServerInfoResult drawServerInfoMenu(GLFWwindow* win,
                                    char* serverAddress,
                                    bool serverRunning)
{
    ServerInfoResult r{};

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/server-info-menu.json");
    GuiBindings& b = GuiBindings::instance();

    // Sync external buffer → binding on menu open (only when no field is focused)
    if (!b.hasFocus("serverAddressInput"))
    {
        std::string currentBinding = b.get("server.address");
        if (currentBinding != serverAddress)
        {
            b.set("server.address", serverAddress);
        }
    }

    // Draw all static elements from layout — text_input now handled by drawGuiElement
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

    // ── Sync binding → external buffer ───────────────────────────────
    std::string address = b.get("server.address");
    std::strncpy(serverAddress, address.c_str(), 63);
    serverAddress[63] = '\0';

    // Dynamic status text
    if (serverRunning)
    {
        uiDrawText("Status: Running", cs.designToScreenX(760), cs.designToScreenY(350),
                   0.42f, {0.3f, 1.0f, 0.4f, 1.0f});
        uiDrawText("Server terminal is open in background",
                   cs.designToScreenX(760), cs.designToScreenY(385),
                   0.30f, {0.6f, 0.65f, 0.75f, 1.0f});
    }
    else
    {
        uiDrawText("Status: Not Running", cs.designToScreenX(760), cs.designToScreenY(350),
                   0.42f, {1.0f, 0.3f, 0.3f, 1.0f});
        uiDrawText("Click to start a dedicated server",
                   cs.designToScreenX(760), cs.designToScreenY(385),
                   0.30f, {0.6f, 0.65f, 0.75f, 1.0f});
    }

    return r;
}
