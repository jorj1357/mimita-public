// 08 16 2026, 01 35
#include "game/window-bridge-module.h"
/* purpose
* Owns lightweight command-line self-tests and diagnostic launch paths.
* Runs requested checks before normal graphics or gameplay startup.
* Keeps dedicated verification commands process-bounded and script-friendly.
* Does NOT own the main game loop, rendering lifecycle, or packet schemas.
* Does NOT implement gameplay systems or long-running server/client loops.
* Does NOT replace feature-owned test logic.
*/

#include "game/game-cli.h"
#include "combat/weapon-runtime.h"
#include "network/snapshot-chunks.h"
#include "worldproject/world-project-store.h"
#include "worldproject/world-edit-session.h"
#include "procedural/procedural-world.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <chrono>
#include <thread>
#include <windows.h>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "engine/engine.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "physics/runtime-objects.h"
#include "camera.h"
#include "terminal/terminal-state.h"
#include "gui/menus/online-menu.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-export-target.h"
#include "replay/replay-factory.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/actor-triangle-spike.h"
#include "physics/physical-entity.h"
#include "impact/impact-system.h"
#include "physics/movement/actor-collision-mesh.h"
#include "physics/movement/actor-final-pose.h"
#include "physics/movement/actor-pair-collision.h"
#include "physics/movement/actor-triangle-solver.h"
#include "debug/crash-handler.h"
#include <exception>
#include <stdexcept>
#include <fstream>
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/ice/ice-connect.h"
#include "network/coordinator-client.h"
#include "network/badconn/badconn.h"
#include "gamemode/match-roles.h"
#include "gamemode/gamemode.h"
#include "gamemode/map-config.h"
#include "gamemode/payload-runtime.h"
#include "map/map-authored-geometry.h"
#include "map/map-static-union.h"
#include "gamemode/mode-pack-registry.h"
#include "gamemode/action-graph.h"
#include "gamemode/capability-registry.h"
#include "gamemode/disaster-runtime.h"
#include "map/map-loader-collision.h"
#include "network/server-gamemode.h"
#include "npc/npc-nav-request.h"
#include "npc/npc-navigation.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-nav-graph.h"
#include "npc/npc-surface.h"
#include "npc/npc-navigation-settings.h"
#include "npc/npc-targeting.h"
#include "entities/aim-fov.h"
#include "combat/grenade-registry.h"
#include "combat/area-effect.h"

extern DuelManager gDuelManager;
extern bool gMainmenuDebug;

static bool runVersionInfoCli()
{
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char cwd[MAX_PATH]{};
    GetCurrentDirectoryA(MAX_PATH, cwd);

    const char* eventsPath = std::getenv("MIMITA_EVENTS_FILE");
    const char* runId = std::getenv("MIMITA_RUN_ID");
    const char* commandLine = GetCommandLineA();
    const char* explicitRole = std::getenv("MIMITA_PROCESS_ROLE");
    const bool serverCommandLine = commandLine &&
        (std::strstr(commandLine, "--server") ||
         std::strstr(commandLine, "-server"));
    const std::string processRole = serverCommandLine ? "server" :
        ((explicitRole && *explicitRole) ? explicitRole : "client");
    const nlohmann::json info = {
        {"executable", exePath},
        {"pid", static_cast<unsigned long long>(GetCurrentProcessId())},
        {"working_directory", cwd},
        {"events_jsonl", eventsPath ? eventsPath : ""},
        {"run_id", runId ? runId : ""},
        {"process_role", processRole},
        {"build_date", __DATE__},
        {"build_time", __TIME__},
        {"arguments", "--versioninfo"}
    };

    // This preflight runs for every launch. Keep it independent from logger
    // initialization: the logger reconciles and prunes the entire journal tree
    // during startup, which can take longer than the launcher's readiness
    // budget on a large workspace. The dev-loop records this result itself.
    printf("VERSIONINFO_BEGIN\n");
    printf("EXECUTABLE_PATH=%s\n", exePath);
    printf("PID=%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
    printf("PROCESS_ROLE=%s\n", processRole.c_str());
    printf("WORKING_DIRECTORY=%s\n", cwd);
    printf("EVENTS_JSONL_PATH=%s\n", eventsPath ? eventsPath : "");
    printf("RUN_ID=%s\n", runId ? runId : "");
    printf("VERSIONINFO_JSON=%s\n", info.dump().c_str());
    printf("VERSIONINFO_END\n");

    return true;
}

// Pin deterministic movement/navigation tuning for the movement regression
// selftests so they guard the algorithm, not whatever the human is currently
// live-editing in config/npc-difficulty.json.
static void pinMovementSelftestTuning()
{
    auto& s = NpcDifficultyConfig::instance().settings();
    s.wallAvoidanceEnabled = true;
    s.wallCastDistance = 4.0f;
    s.wallSearchDistance = 4.0f;
    s.wallBacktrackEnabled = true;
    s.wallBacktrackDistance = 1.0f;
    s.wallBacktrackDuration = 0.8f;
    s.wallGroundSupportRequired = true;
    s.turnSideBias = 0.75f;
    s.wallAvoidMinProbe = 3.0f;
    s.jumpCooldownSeconds = 0.45f;
    s.exploreDistanceMeters = 60.0f;
    s.exploreHoldSeconds = 12.0f;
    s.exploreMinProgressMeters = 6.0f;
    s.useNavGraph = false;
    s.areaEscapeRadiusMeters = 5.0f;
    s.areaEscapeSeconds = 3.0f;
    s.areaEscapeHoldSeconds = 3.0f;
    s.lowObstacleJumpEnabled = true;
    s.lowObstacleProbe = 1.3f;
    s.lowObstacleLowOffset = -1.5f;
    s.lowObstacleHighOffset = -0.3f;
}

bool handleGameCLI(int argc, char** argv)
{
    if (argc <= 1) return false;

    const std::string command = argv[1];
    if (command == "--versioninfo")
        return runVersionInfoCli();

    if (command == "--ice-host-only" ||
        command == "--ice-join-only" ||
        command == "--ice-game-host" ||
        command == "--ice-game-client" ||
        command == "--ice-server")
    {
        printf("[ICE LEGACY DISABLED] command=%s reason=Stage3C-unified-server-path\n", argv[1]);
        printf("[ICE LEGACY DISABLED] use --server --ice --timeout <secs> for hosting and --ice-connect <room-code> for a client probe\n");
        std::exit(2);
    }

    // ── ICE Connect (client connects via ICE to an ICE server) ────
    if (std::string(argv[1]) == "--ice-connect") {
        if (argc < 3) {
            printf("[ICE] usage: mimita.exe --ice-connect <room-code>\n");
            return true;
        }
        IceConnectOptions opts;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--disable-relay") opts.disableRelay = true;
            else if (a == "--timeout-seconds" && i + 1 < argc)
                opts.timeoutSeconds = std::max(1, std::atoi(argv[++i]));
            else if (a == "--death-respawn-cycles" && i + 1 < argc)
                opts.deathRespawnCycles = std::max(0, std::atoi(argv[++i]));
            else if (a == "--client-index" && i + 1 < argc)
                opts.clientIndex = std::max(0, std::atoi(argv[++i]));
            else if (a == "--reconnect-token" && i + 1 < argc)
                opts.reconnectToken = argv[++i];
        }
        runIceConnect(argv[2], opts);
        return true;
    }











































































    return false;
}
