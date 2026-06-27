#include "server-info-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
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

bool addressInputActive = false;
char* activeAddress = nullptr;

}

void serverInfoMenuSetActive(bool active)
{
    addressInputActive = active;
    if (!active)
        activeAddress = nullptr;
}

void serverInfoMenuHandleChar(unsigned int codepoint)
{
    if (!addressInputActive || !activeAddress)
        return;
    const bool allowed =
        (codepoint >= '0' && codepoint <= '9') ||
        codepoint == '.' || codepoint == ':';
    const size_t length = std::strlen(activeAddress);
    if (allowed && length < 63)
    {
        activeAddress[length] = (char)codepoint;
        activeAddress[length + 1] = '\0';
    }
}

void serverInfoMenuHandleKey(int key, int action)
{
    if (!addressInputActive || !activeAddress ||
        (action != GLFW_PRESS && action != GLFW_REPEAT))
        return;
    if (key == GLFW_KEY_BACKSPACE)
    {
        const size_t length = std::strlen(activeAddress);
        if (length)
            activeAddress[length - 1] = '\0';
    }
}

ServerInfoResult drawServerInfoMenu(GLFWwindow* win,
                                    char* serverAddress,
                                    bool serverRunning)
{
    ServerInfoResult r{};
    activeAddress = serverAddress;
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

    // Dynamic address input
    {
        uiDrawRect(cs.designToScreen({860, 220, 300, 48}),
                   {0.12f,0.14f,0.18f,1}, "server-addr-input");
        uiDrawText(serverAddress, cs.designToScreenX(872), cs.designToScreenY(240),
                   0.42f, {0.95f, 0.98f, 1.0f, 1.0f});
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
