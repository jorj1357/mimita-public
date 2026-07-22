// 07 21 2026, 21 45
/* purpose
* Tests that Shotgun online firing routes through generic seeded hitscan AttackRequest code.
* Uses source assertions to prevent restoring the old PelletBlastRequest authority path.
* Verifies the legacy pellet handler returns before mutating server state.
* Does NOT open sockets, launch the game, render pellets, or run full ICE validation.
* Does NOT test Revolver single-ray behavior, Godball, Swordsword, or projectile migration.
* Does NOT inspect audio, recoil, local prediction visuals, or NPC damage.
*/

#include <cstdio>
#include <fstream>
#include <string>

static int gFailures = 0;

static std::string readFile(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

int main()
{
    const std::string terminal = readFile("src/terminal/weapon-commands.cpp");
    const std::string handlers = readFile("src/network/server-packet-handlers.cpp");
    check(terminal.find("mpSendPelletBlastRequest(") == std::string::npos,
          "terminal weapon route no longer sends PelletBlastRequest");
    const std::size_t fn = handlers.find("void handlePelletBlastRequest");
    const std::size_t ret = handlers.find("(void)request;\n    return;", fn);
    const std::size_t mutation = handlers.find("auto shooterIt = players.find", fn);
    check(fn != std::string::npos && ret != std::string::npos && mutation != std::string::npos && ret < mutation,
          "legacy pellet handler returns before player mutation");

    if (gFailures)
    {
        std::printf("[shotgun-network-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[shotgun-network-test] PASS\n");
    return 0;
}
