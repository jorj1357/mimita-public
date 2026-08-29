// 07 21 2026, 18 32
/* purpose
* Parses MiMITA networking launch flags for local clients, servers, and harnesses.
* Preserves the no-argument single-player path while adding bounded diagnostics.
* Keeps process mode selection data-only so main.cpp remains the orchestrator.
* Does NOT create sockets, run gameplay loops, or contact the coordinator.
* Does NOT own packet schemas, movement validation, or ICE agent setup.
* Does NOT launch graphics windows or mutate runtime gameplay rules.
*/

#include "network/net_mode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace MimitaNet {

LaunchOptions parseLaunchOptions(int argc, char** argv)
{
    LaunchOptions options;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--server") == 0)
            options.server = true;
        else if (std::strcmp(argv[i], "--client") == 0)
            options.client = true;
        else if (std::strcmp(argv[i], "--connect") == 0 && i + 1 < argc)
        {
            options.connect = argv[++i];
            options.connectExplicit = true;
        }
        else if (std::strcmp(argv[i], "--bind") == 0 && i + 1 < argc)
        {
            options.bind = argv[++i];
            options.bindExplicit = true;
        }
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc)
            options.name = argv[++i];
        else if (std::strcmp(argv[i], "--session") == 0 && i + 1 < argc)
            options.sessionToken = argv[++i];
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            options.mapName = argv[++i];
        else if (std::strcmp(argv[i], "--host-player") == 0 && i + 1 < argc)
            options.hostPlayerName = argv[++i];
        else if (std::strcmp(argv[i], "--max-players") == 0 && i + 1 < argc)
            options.maxPlayers = (uint32_t)std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--password-protected") == 0 && i + 1 < argc)
            options.passwordProtected = std::strcmp(argv[++i], "1") == 0;
        else if (std::strcmp(argv[i], "--password") == 0 && i + 1 < argc)
            options.password = argv[++i];
        else if (std::strcmp(argv[i], "--npcs") == 0 && i + 1 < argc)
            options.npcCount = (uint32_t)std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--no-npcs") == 0)
            options.npcsEnabled = false;
        else if (std::strcmp(argv[i], "--room-file") == 0 && i + 1 < argc)
            options.roomFilePath = argv[++i];
        else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            options.timeoutSecs = (uint32_t)std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--duel") == 0)
            options.duel = true;
        else if (std::strcmp(argv[i], "--gamemode") == 0 && i + 1 < argc)
            options.gamemodeId = argv[++i];
        else if (std::strcmp(argv[i], "--udp-echo") == 0)
        {
            options.udpEcho = true;
            options.server = true;
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
            printLaunchUsage();
    }
    return options;
}

void printLaunchUsage()
{
    printf("Mimita multiplayer mode:\n");
    printf("  mimita.exe --server [--bind 0.0.0.0:1357] [--timeout <secs>]\n");
    printf("  mimita.exe --server --udp-echo --bind 127.0.0.1:0 --timeout <secs>\n");
    printf("  mimita.exe --client --name client1 --connect 127.0.0.1:1357\n");
    printf("  mimita.exe --session <token>\n");
    printf("  mimita.exe --server --duel --gamemode duel --map <map>\n");
    printf("  --bind <addr:port> Server UDP bind address (IPv4; port 0 allowed for harnesses)\n");
    printf("  --timeout <secs>  Auto-exit server after N seconds (0=no timeout, default)\n");
    printf("  --duel            Run a first-to-goal PvP duel match (2 players)\n");
    printf("  --gamemode <id>   Gamemode JSON id used for duel rules (default: duel)\n");
    printf("  --ice             ICE NAT traversal is always enabled\n");
    printf("No args keeps the normal single-player/menu flow.\n");
}

} // namespace MimitaNet
