#include "server-info-menu.h"
#include "../gui-button.h"
#include "../gui-back.h"
#include "../gui-label.h"
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
    // CREATE_NEW_CONSOLE gives the server its own visible terminal window
    // so the user can see server logs
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

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;
    activeAddress = serverAddress;

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "server-info-bg");

    guiLabel("Host Server", cx - 80.0f, 140.0f);

    uiDrawText("Address:", cx - 200.0f, 240.0f, 0.38f, {0.7f, 0.75f, 0.85f, 1.0f});
    uiDrawRect({cx - 100.0f, 220.0f, 300.0f, 48.0f},
               {0.12f,0.14f,0.18f,1.0f}, "server-address-input");
    uiDrawText(serverAddress, cx - 88.0f, 240.0f, 0.42f, {0.95f, 0.98f, 1.0f, 1.0f});

    uiDrawText("Port: 1357", cx - 200.0f, 280.0f, 0.38f, {0.7f, 0.75f, 0.85f, 1.0f});

    if (serverRunning) {
        uiDrawText("Status: Running", cx - 200.0f, 330.0f, 0.42f, {0.3f, 1.0f, 0.4f, 1.0f});
        uiDrawText("Server terminal is open in background", cx - 200.0f, 365.0f, 0.30f, {0.6f, 0.65f, 0.75f, 1.0f});

        if (guiButton(win, "Connect", cx - 125.0f, 400.0f, 250.0f, 60.0f, {0.2f,0.7f,1.0f,1.0f}))
            r.connect = true;
    } else {
        uiDrawText("Status: Not Running", cx - 200.0f, 330.0f, 0.42f, {1.0f, 0.3f, 0.3f, 1.0f});
        uiDrawText("Click to start a dedicated server", cx - 200.0f, 365.0f, 0.30f, {0.6f, 0.65f, 0.75f, 1.0f});

        if (guiButton(win, "Start Server", cx - 125.0f, 400.0f, 250.0f, 60.0f, {0.2f,0.8f,0.3f,1.0f}))
        {
            if (launchServerProcess())
                r.startServer = true;
        }
    }

    if (guiBackButton(win))
        r.goBack = true;

    return r;
}
