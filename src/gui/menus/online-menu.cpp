#include "online-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

enum class InputField
{
    None,
    ServerName,
    HostIp,
    HostPort,
    JoinIp,
    JoinPort
};

bool menuActive = false;
InputField focusedField = InputField::None;
std::string serverName = "Mimita Server";
std::string hostIp = "127.0.0.1";
std::string hostPort = "1357";
std::string joinIp = "127.0.0.1";
std::string joinPort = "1357";
std::string processStatus = "Stopped";

#ifdef _WIN32
HANDLE serverProcess = nullptr;
DWORD serverProcessId = 0;
#endif

std::string* focusedValue()
{
    switch (focusedField)
    {
        case InputField::ServerName: return &serverName;
        case InputField::HostIp: return &hostIp;
        case InputField::HostPort: return &hostPort;
        case InputField::JoinIp: return &joinIp;
        case InputField::JoinPort: return &joinPort;
        default: return nullptr;
    }
}

bool validCharacter(InputField field, unsigned int codepoint)
{
    if (field == InputField::ServerName)
        return codepoint >= 32 && codepoint <= 126 && codepoint != '"';
    if (field == InputField::HostPort || field == InputField::JoinPort)
        return codepoint >= '0' && codepoint <= '9';
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.';
}

void appendCharacter(std::string& target, unsigned int codepoint)
{
    if (target.size() < 31)
        target.push_back((char)codepoint);
}

}

void onlineMenuSetActive(bool active) { menuActive = active; processStatus = "Stopped"; }
void onlineMenuHandleChar(unsigned int codepoint)
{
    if (!menuActive) return;
    std::string* val = focusedValue();
    if (val && validCharacter(focusedField, codepoint))
        appendCharacter(*val, codepoint);
}
void onlineMenuHandleKey(int key, int action)
{
    if (!menuActive || (action != GLFW_PRESS && action != GLFW_REPEAT)) return;
    if (key == GLFW_KEY_TAB) {
        int f = (int)focusedField;
        f = (f + 1) % 6;
        focusedField = (InputField)f;
        return;
    }
    if (key == GLFW_KEY_BACKSPACE) {
        std::string* val = focusedValue();
        if (val && !val->empty()) val->pop_back();
    }
}

OnlineMenuResult drawOnlineMenu(GLFWwindow* win)
{
    OnlineMenuResult r{};
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/community-menu.json");

    // Draw all static elements from layout (background, labels, separator, buttons)
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
            #ifdef _WIN32
            if (!serverProcess) {
                STARTUPINFOA si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                std::string cmd = "mimita.exe --server --port " + hostPort;
                if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                                   CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi))
                {
                    serverProcess = pi.hProcess;
                    serverProcessId = pi.dwProcessId;
                    CloseHandle(pi.hThread);
                    processStatus = "Running";
                    printf("[ONLINE MENU] Server started PID=%lu port=%s\n",
                           pi.dwProcessId, hostPort.c_str());
                }
            }
            #endif
        }
        else if (id == "stopServerButton")
        {
            #ifdef _WIN32
            if (serverProcess) {
                TerminateProcess(serverProcess, 0);
                CloseHandle(serverProcess);
                serverProcess = nullptr;
                serverProcessId = 0;
                processStatus = "Stopped";
            }
            #endif
        }
        else if (id == "joinServerButton")
        {
            r.connectToServer = true;
            r.connectAddress = joinIp + ":" + joinPort;
        }
        else if (id == "backButton")
            r.goBack = true;
    }

    // Dynamic input fields (Host side)
    {
        GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
        // Host Port input
        uiDrawRect(cs.designToScreen({700, 200, 200, 36}), {0.12f,0.14f,0.18f,1}, "host-port-input");
        uiDrawText(hostPort.c_str(), cs.designToScreenX(708), cs.designToScreenY(208), 0.32f, {1,1,1,1});
        if (uiButton(win, "", {700,200,200,36}, {0,0,0,0}).clicked) focusedField = InputField::HostPort;

        // Status
        char status[64];
        snprintf(status, sizeof(status), "Status: %s", processStatus.c_str());
        uiDrawText(status, cs.designToScreenX(510), cs.designToScreenY(230), 0.30f, {0.75f, 0.8f, 0.9f, 1.0f});
    }

    // Dynamic input fields (Join side)
    {
        GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
        // Join Address input
        uiDrawRect(cs.designToScreen({990, 200, 360, 36}), {0.12f,0.14f,0.18f,1}, "join-addr-input");
        std::string joinAddr = joinIp + ":" + joinPort;
        uiDrawText(joinAddr.c_str(), cs.designToScreenX(998), cs.designToScreenY(208), 0.32f, {1,1,1,1});
        if (uiButton(win, "", {990,200,360,36}, {0,0,0,0}).clicked) focusedField = InputField::JoinIp;
    }

    return r;
}
