#pragma once

#include <string>

namespace MimitaNet {

struct LaunchOptions
{
    bool server = false;
    bool client = false;
    std::string connect = "127.0.0.1:1357";
    std::string name;
    std::string sessionToken;
    std::string mapName;
    bool npcsEnabled = true;
    uint32_t npcCount = 3;
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
