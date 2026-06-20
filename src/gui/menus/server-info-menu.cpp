#include "server-info-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
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
    float fbW = uiScreenW(), fbH = uiScreenH();
    activeAddress = serverAddress;

    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "server-info-bg");

    uiDrawText("Host Server", uiScaleX(880.0f), uiScaleY(140.0f), 0.42f, {0.55f, 0.78f, 1.0f, 1.0f});

    uiDrawText("Address:", uiScaleX(760.0f), uiScaleY(240.0f), 0.38f, {0.7f, 0.75f, 0.85f, 1.0f});
    uiDrawRect({uiScaleX(860.0f), uiScaleY(220.0f), uiScaleX(300.0f), uiScaleY(48.0f)},
               {0.12f,0.14f,0.18f,1.0f}, "server-address-input");
    uiDrawText(serverAddress, uiScaleX(872.0f), uiScaleY(240.0f), 0.42f, {0.95f, 0.98f, 1.0f, 1.0f});

    uiDrawText("Port: 1357", uiScaleX(760.0f), uiScaleY(280.0f), 0.38f, {0.7f, 0.75f, 0.85f, 1.0f});

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/server-info-menu.json");
    if (serverRunning) {
        uiDrawText("Status: Running", uiScaleX(760.0f), uiScaleY(330.0f), 0.42f, {0.3f, 1.0f, 0.4f, 1.0f});
        uiDrawText("Server terminal is open in background", uiScaleX(760.0f), uiScaleY(365.0f), 0.30f, {0.6f, 0.65f, 0.75f, 1.0f});

        const GuiElement* ce = layout.get("connect");
        if (ce && drawGuiElement(win, *ce).clicked)
            r.connect = true;
    } else {
        uiDrawText("Status: Not Running", uiScaleX(760.0f), uiScaleY(330.0f), 0.42f, {1.0f, 0.3f, 0.3f, 1.0f});
        uiDrawText("Click to start a dedicated server", uiScaleX(760.0f), uiScaleY(365.0f), 0.30f, {0.6f, 0.65f, 0.75f, 1.0f});

        const GuiElement* se = layout.get("startServer");
        if (se && drawGuiElement(win, *se).clicked)
        {
            if (launchServerProcess())
                r.startServer = true;
        }
    }

    {
        const GuiElement* bb = layout.get("backButton");
        if (bb && drawGuiElement(win, *bb).clicked)
            r.goBack = true;
    }

    return r;
}
