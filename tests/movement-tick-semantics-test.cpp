// 07 22 2026, 17 00
/* purpose
* Tests that movement tick semantics no longer compare serverTick against
* clientSimulationTick. Verifies the fix to tickTooOld/tickTooFuture.
* Does NOT open sockets, launch the game, or depend on ICE headers.
* Does NOT replace the movement-network-integration-test for runtime behavior.
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
    const std::string valid = readFile("src/network/movement-validation.cpp");

    // tickTooOld and tickTooFuture must no longer be called for validation
    check(valid.find("if (tickTooOld(player, report, context, config))") == std::string::npos,
          "tickTooOld call removed from validateClientMovementReport");
    check(valid.find("if (tickTooFuture(report, context, config))") == std::string::npos,
          "tickTooFuture call removed from validateClientMovementReport");

    // The replacement must use lastAcceptedClientTick progression check
    check(valid.find("lastAcceptedClientTick") != std::string::npos,
          "lastAcceptedClientTick referenced in movement-validation.cpp");
    check(valid.find("report.clientSimulationTick <= player.movementValidation.lastAcceptedClientTick") != std::string::npos,
          "client tick progression check replaces stale tick check");

    // hasAcceptedClientTransform reset on lifecycle
    const std::string serverPlayers = readFile("src/network/server-players.cpp");
    // Actually the reset is in movement-validation.cpp's resetServerMovementForAuthoritativeLifecycle
    check(valid.find("player.hasAcceptedClientTransform = false;") != std::string::npos,
          "hasAcceptedClientTransform reset on lifecycle");

    // Baseline establishment log in server-packets.cpp
    const std::string serverPackets = readFile("src/network/server-packets.cpp");
    check(serverPackets.find("SERVER MOVEMENT BASELINE") != std::string::npos,
          "baseline establishment log present");

    // Enhanced diagnostic log includes lastAcceptedClientTick
    check(serverPackets.find("lastAcceptedTick=") != std::string::npos,
          "diagnostic log includes lastAcceptedClientTick");

    if (gFailures)
    {
        std::printf("[movement-tick-semantics-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[movement-tick-semantics-test] PASS\n");
    return 0;
}
