#include "online-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
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
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           codepoint == '.' || codepoint == '-' || codepoint == ':';
}

std::string endpoint(const std::string& ip, const std::string& port)
{
    return ip + ":" + port;
}

bool serverRunning()
{
#ifdef _WIN32
    if (!serverProcess)
        return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(serverProcess, &exitCode) || exitCode != STILL_ACTIVE)
    {
        CloseHandle(serverProcess);
        serverProcess = nullptr;
        serverProcessId = 0;
        processStatus = "Stopped";
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool launchServerProcess()
{
#ifdef _WIN32
    if (serverRunning())
        return true;

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::string command = "mimita.exe --server --connect " +
        endpoint(hostIp, hostPort) + " --name \"" + serverName + "\"";
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    BOOL ok = CreateProcessA(
        nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
        nullptr, nullptr, &startup, &process);
    if (!ok)
    {
        processStatus = "Start failed (" + std::to_string(GetLastError()) + ")";
        printf("[SERVER LAUNCH] CreateProcess failed error=%lu\n", GetLastError());
        return false;
    }

    CloseHandle(process.hThread);
    serverProcess = process.hProcess;
    serverProcessId = process.dwProcessId;
    processStatus = "Running (PID " + std::to_string(serverProcessId) + ")";
    printf("[SERVER LAUNCH] server process started PID=%lu endpoint=%s\n",
           serverProcessId, endpoint(hostIp, hostPort).c_str());
    return true;
#else
    processStatus = "Server launch is only implemented on Windows";
    return false;
#endif
}

bool stopServerProcess()
{
#ifdef _WIN32
    if (!serverRunning())
        return true;

    bool stoppedGracefully =
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, serverProcessId) != FALSE;
    if (stoppedGracefully)
        WaitForSingleObject(serverProcess, 1000);

    DWORD exitCode = STILL_ACTIVE;
    GetExitCodeProcess(serverProcess, &exitCode);
    if (exitCode == STILL_ACTIVE)
    {
        printf("[SERVER STOP] graceful stop unavailable; terminating PID=%lu\n",
               serverProcessId);
        TerminateProcess(serverProcess, 0);
        WaitForSingleObject(serverProcess, 1000);
    }

    CloseHandle(serverProcess);
    serverProcess = nullptr;
    serverProcessId = 0;
    processStatus = "Stopped";
    printf("[SERVER STOP] server process stopped\n");
    return true;
#else
    return false;
#endif
}

void drawInput(
    GLFWwindow* window,
    const char* label,
    std::string& value,
    InputField field,
    float x,   // design x
    float y,   // design y
    float width) // design width
{
    uiDrawText(label, uiScaleX(x), uiScaleY(y), 0.30f, {0.72f, 0.78f, 0.88f, 1.0f});
    const bool focused = focusedField == field;
    UIButtonState state = uiButton(
        window, value.empty() ? "_" : value.c_str(),
        {x, y + 24.0f, width, 42.0f},
        focused ? glm::vec4(0.19f, 0.28f, 0.38f, 1.0f)
                : glm::vec4(0.11f, 0.13f, 0.17f, 1.0f),
        label);
    if (state.clicked)
        focusedField = field;
}

} // namespace

void onlineMenuSetActive(bool active)
{
    menuActive = active;
    if (!active)
        focusedField = InputField::None;
}

void onlineMenuHandleChar(unsigned int codepoint)
{
    std::string* value = focusedValue();
    if (!menuActive || !value || !validCharacter(focusedField, codepoint))
        return;
    if (value->size() < 63)
        value->push_back((char)codepoint);
}

void onlineMenuHandleKey(int key, int action)
{
    std::string* value = focusedValue();
    if (!menuActive || !value ||
        (action != GLFW_PRESS && action != GLFW_REPEAT))
        return;
    if (key == GLFW_KEY_BACKSPACE && !value->empty())
        value->pop_back();
}

OnlineMenuResult drawOnlineMenu(GLFWwindow* win)
{
    OnlineMenuResult r{};

    float fbW = uiScreenW(), fbH = uiScreenH();
    const bool running = serverRunning();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/community-menu.json");

    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "online-menu-bg");

    uiDrawText("PLAY  >  ONLINE", uiScaleX(820.0f), uiScaleY(60.0f), 0.50f,
               {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawRect({uiScaleX(760.0f), uiScaleY(104.0f), uiScaleX(400.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.6f}, "online-menu-separator");

    uiDrawText("Multiplayer", uiScaleX(880.0f), uiScaleY(130.0f), 0.42f, {0.72f, 0.78f, 0.88f, 1.0f});

    // Design coordinates: left column at x=510, right column at x=990
    const float left = 510.0f;
    const float right = 990.0f;
    uiDrawText("HOST SERVER", uiScaleX(left), uiScaleY(160.0f), 0.42f, {0.3f, 1.0f, 0.5f, 1.0f});
    drawInput(win, "Server name", serverName, InputField::ServerName, left, 205.0f, 410.0f);
    drawInput(win, "Bind IP", hostIp, InputField::HostIp, left, 290.0f, 270.0f);
    drawInput(win, "Port", hostPort, InputField::HostPort, left + 290.0f, 290.0f, 120.0f);

    uiDrawText(("Status: " + processStatus).c_str(), uiScaleX(left), uiScaleY(380.0f), 0.32f,
               running ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
                       : glm::vec4(0.75f, 0.78f, 0.84f, 1.0f));
    if (!running) {
        if (uiButton(win, "Start Server",
            layout.getRectDesign("Start Server", {510.0f, 420.0f, 195.0f, 52.0f}),
            {0.2f,0.8f,0.3f,1.0f}, "Start Server").clicked)
            r.startServer = launchServerProcess();
    }
    if (running) {
        if (uiButton(win, "Stop Server",
            layout.getRectDesign("Stop Server", {510.0f, 420.0f, 195.0f, 52.0f}),
            {0.85f,0.22f,0.18f,1.0f}, "Stop Server").clicked)
            r.stopServer = stopServerProcess();
        if (uiButton(win, "Join Server",
            layout.getRectDesign("Join Server", {725.0f, 420.0f, 195.0f, 52.0f}),
            {0.2f,0.7f,1.0f,1.0f}, "Join Server").clicked)
        {
            r.connectToServer = true;
            r.connectAddress = endpoint(hostIp, hostPort);
        }
    }

    uiDrawText("JOIN SERVER", uiScaleX(right), uiScaleY(160.0f), 0.42f, {0.35f, 0.7f, 1.0f, 1.0f});
    drawInput(win, "Server IP / hostname", joinIp, InputField::JoinIp, right, 205.0f, 280.0f);
    drawInput(win, "Port", joinPort, InputField::JoinPort, right + 300.0f, 205.0f, 120.0f);
    {
        if (uiButton(win, "Connect",
            layout.getRectDesign("Connect", {990.0f, 310.0f, 420.0f, 58.0f}),
            {0.2f,0.7f,1.0f,1.0f}, "Connect").clicked)
        {
            r.connectToServer = true;
            r.connectAddress = endpoint(joinIp, joinPort);
        }
    }

    {
        const GuiElement* bb = layout.get("backButton");
        if (bb && drawGuiElement(win, *bb).clicked)
            r.goBack = true;
    }

    return r;
}
