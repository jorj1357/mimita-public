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
            options.connect = argv[++i];
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc)
            options.name = argv[++i];
        else if (std::strcmp(argv[i], "--session") == 0 && i + 1 < argc)
            options.sessionToken = argv[++i];
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            options.mapName = argv[++i];
        else if (std::strcmp(argv[i], "--code") == 0 && i + 1 < argc)
            options.serverCode = argv[++i];
        else if (std::strcmp(argv[i], "--npcs") == 0 && i + 1 < argc)
            options.npcCount = (uint32_t)std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--no-npcs") == 0)
            options.npcsEnabled = false;
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
            printLaunchUsage();
    }
    return options;
}

void printLaunchUsage()
{
    printf("Mimita local multiplayer test mode:\n");
    printf("  mimita.exe --server\n");
    printf("  mimita.exe --client --name client1 --connect 127.0.0.1:1357\n");
    printf("  mimita.exe --session <token>\n");
    printf("No args keeps the normal single-player/menu flow.\n");
}

} // namespace MimitaNet
