#pragma once

#include <string>

namespace MimitaNet {

struct LaunchOptions
{
    bool server = false;
    bool client = false;
    std::string connect = "127.0.0.1:1357";
    bool connectExplicit = false;
    std::string roomFilePath;
    std::string name;
    std::string sessionToken;
    std::string mapName;
    std::string serverCode;
    bool npcsEnabled = true;
    uint32_t npcCount = 3;
    uint32_t timeoutSecs = 0; // 0 = no timeout (default), >0 = auto-exit after N seconds
};

LaunchOptions parseLaunchOptions(int argc, char** argv);
void printLaunchUsage();
int runServer(const LaunchOptions& options);
int runClient(const LaunchOptions& options);

// Forward declaration
struct ServerLaunchSettings;
// Run server with explicit settings (from GUI process launch)
int runServerWithSettings(const ServerLaunchSettings& settings);

} // namespace MimitaNet
