// 07 21 2026, 21 45
/* purpose
* Tests that Revolver online firing routes through generic hitscan AttackRequest code.
* Uses source assertions to prevent restoring the old ShotRequest authority path.
* Verifies server attack dispatch owns hitscan tracing and server damage application.
* Does NOT open sockets, launch the game, render tracers, or depend on user input.
* Does NOT test Shotgun pellet spread, physical-contact weapons, or projectile migration.
* Does NOT replace full process-level UDP/ICE validation.
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
    const std::string server = readFile("src/network/server-attack.cpp");
    check(terminal.find("mpSendShotEvent(") == std::string::npos,
          "terminal weapon route no longer sends old ShotRequest");
    check(terminal.find("send-generic-attack-request") != std::string::npos,
          "terminal route documents generic AttackRequest action");
    check(server.find("WeaponExecutionType::Hitscan") != std::string::npos,
          "server dispatch has generic hitscan branch");
    check(server.find("traceHitscan") != std::string::npos,
          "server hitscan branch uses shared trace helper");
    check(server.find("ServerDamageSource::Hitscan") != std::string::npos,
          "server applies authoritative hitscan damage");

    if (gFailures)
    {
        std::printf("[revolver-network-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[revolver-network-test] PASS\n");
    return 0;
}
