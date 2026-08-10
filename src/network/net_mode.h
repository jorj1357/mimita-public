// 07 21 2026, 18 32
/* purpose
* Declares process launch options for headless server and client networking.
* Keeps CLI-owned bind, coordinator, ICE, and bounded-test flags in one struct.
* Exposes small entry points used by main.cpp and process-level harnesses.
* Does NOT own socket creation, packet dispatch, rendering, or gameplay logic.
* Does NOT register rooms, validate movement, or mutate weapon behavior.
* Does NOT change the no-argument single-player launch path.
*/

#pragma once

#include <string>

namespace MimitaNet {

struct LaunchOptions
{
    bool server = false;
    bool client = false;
    std::string connect;
    bool connectExplicit = false;
    std::string bind = "0.0.0.0:1357";
    bool bindExplicit = false;
    std::string roomFilePath;
    std::string name;
    std::string sessionToken;
    std::string mapName;
    std::string hostPlayerName;
    uint32_t maxPlayers = 999;
    bool passwordProtected = false;
    bool npcsEnabled = true;
    uint32_t npcCount = 3;
    uint32_t timeoutSecs = 0;
    bool udpEcho = false;
    bool duel = false;
    std::string gamemodeId = "duel";
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
