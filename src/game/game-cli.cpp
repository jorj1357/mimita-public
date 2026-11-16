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
    StructuredLogger::instance().init();

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char cwd[MAX_PATH]{};
    GetCurrentDirectoryA(MAX_PATH, cwd);

    const std::string eventsPath = StructuredLogger::instance().eventsPath();
    const std::string runId = StructuredLogger::instance().runId();
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
        {"events_jsonl", eventsPath},
        {"run_id", runId},
        {"process_role", processRole},
        {"build_date", __DATE__},
        {"build_time", __TIME__},
        {"arguments", "--versioninfo"}
    };

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General,
        StructuredLevel::Important,
        "versioninfo.executed",
        "",
        "bounded command-line identity probe completed",
        0,
        info,
        __FILE__,
        __LINE__,
        __FUNCTION__);

    // Deliberately obvious keys let an agent capture stdout and open the exact
    // journal without guessing the newest log folder.
    printf("VERSIONINFO_BEGIN\n");
    printf("EXECUTABLE_PATH=%s\n", exePath);
    printf("PID=%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
    printf("PROCESS_ROLE=%s\n", processRole.c_str());
    printf("WORKING_DIRECTORY=%s\n", cwd);
    printf("EVENTS_JSONL_PATH=%s\n", eventsPath.c_str());
    printf("RUN_ID=%s\n", runId.c_str());
    printf("VERSIONINFO_JSON=%s\n", info.dump().c_str());
    printf("VERSIONINFO_END\n");

    StructuredLogger::instance().shutdown();
    return true;
}

void forceMainMenu()
{
    auto startTime = std::chrono::steady_clock::now();
    Debug::log(Debug::Category::General, "[MAINMENU] requested");

    printf("[MAINMENU] entering\n");

    auto logPhase = [&](const char* phase) {
        if (!gMainmenuDebug) return;
        auto now = std::chrono::steady_clock::now();
        double ms = (double)std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() / 1000.0;
        Debug::log(Debug::Category::General, "[MAINMENU] %s: %.2fms", phase, ms);
    };

    Debug::log(Debug::Category::General, "[MAINMENU] currentState=%s",
        gDuelManager.phase() != DuelPhase::Off ? "DUEL" :
        REPLAY_PLAYER.isPlaying() ? "REPLAY" :
        MP_CONTEXT.active ? "MULTIPLAYER" : "GAMEPLAY");

    Debug::log(Debug::Category::General, "[MAINMENU] transitioning");

    // 1. Stop replay playback
    if (REPLAY_PLAYER.isPlaying()) {
        printf("[MAINMENU] cleaning replay\n");
        REPLAY_PLAYER.stopPlayback();
        logPhase("Replay Cleanup");
    }

    // 1b. Clear replay actor/weapon models
    printf("[MAINMENU] clearing replay models\n");
    if (gpReplayActorModels) gpReplayActorModels->clear();
    if (gpReplayWeaponModels) gpReplayWeaponModels->clear();
    if (gpReplayChatStates) gpReplayChatStates->clear();
    logPhase("Replay Models Clear");

    // 2. Stop replay recording
    if (REPLAY_RECORDER.isRecording()) {
        REPLAY_RECORDER.stopRecording();
        logPhase("Replay Recording Stop");
    }

    // 3. Stop duel
    if (gDuelManager.phase() != DuelPhase::Off) {
        printf("[MAINMENU] cleaning duel\n");
        gDuelManager.stopDuel();
        logPhase("Duel Cleanup");
    }

    // 4. Destroy NPCs
    THE_NPC_SYSTEM.destroyAll();
    logPhase("NPC Cleanup");

    // Runtime-only physical objects belong to the session, not the menu.
    clearRuntimeObjects("main-menu");

    // 5. Disconnect multiplayer
    if (MP_CONTEXT.active) {
        printf("[MAINMENU] cleaning network\n");
        Debug::log(Debug::Category::General, "[MAINMENU] disconnecting multiplayer");
        MimitaNet::mpShutdown(MP_CONTEXT);
        onlineMenuSetServerCode("");
        onlineMenuSetServerRunning(false);
        logPhase("Network Cleanup");
        Debug::log(Debug::Category::General, "[MAINMENU] client disconnected");
    }

    // 6. Reset freecam
    FREECAM_ENABLED = false;

    // 7. Reset player state
    resetAllWeaponRuntimesForSpawn(THE_PLAYER, "forceMainMenu");
    THE_PLAYER.dead = false;
    THE_PLAYER.currentHp = THE_PLAYER.maxHp;
    THE_PLAYER.vel = glm::vec3(0.0f);
    THE_PLAYER.externalImpulse = glm::vec3(0.0f);
    THE_PLAYER.proceduralFrozen = false;
    THE_PLAYER.respawnTimer = 0.0f;
    THE_PLAYER.killedBy.clear();

    // 8. Cancel any ongoing replay export
    cancelReplayExport();

    // 9. Force cursor visible
    mimita::game::hostSetCursorMode(0);

    // 10. Transition to menu
    printf("[MAINMENU] switching scene\n");
    GAME_STATE = GAME_MENU;

    logPhase("GUI Load");
    Debug::log(Debug::Category::General, "[MAINMENU] success");
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
            else if (a == "--badconn-preset" && i + 1 < argc)
                opts.badconnPreset = argv[++i];
        }
        if (!opts.badconnPreset.empty()) {
            badconn::loadConfig(badconn::configPath());
            if (opts.badconnPreset == "0")
                badconn::disable();
            else if (!badconn::activatePreset(opts.badconnPreset)) {
                printf("[BADCONN CLI] preset %s not found\n", opts.badconnPreset.c_str());
                std::exit(2);
            }
            printf("[BADCONN CLI] preset=%s active=%d name=%s\n",
                   opts.badconnPreset.c_str(), (int)badconn::active(),
                   badconn::activePresetName().c_str());
        }
        runIceConnect(argv[2], opts);
        return true;
    }














































    if (std::string(argv[1]) == "--gamemode-config-validate") {
        auto& registry = GamemodeRegistry::instance();
        registry.loadDirectory(argc > 2 ? argv[2] : "config/gamemodes");
        const auto& errors = registry.validationErrors();
        printf("[GAMEMODE CONFIG] masterlist=%d loaded=%zu errors=%zu\n",
               registry.masterlistLoaded() ? 1 : 0, registry.ids().size(),
               errors.size());
        for (const std::string& error : errors)
            printf("[GAMEMODE CONFIG] %s\n", error.c_str());
        const bool ok = registry.masterlistLoaded() && errors.empty();
        printf("[GAMEMODE CONFIG] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }





    if (std::string(argv[1]) == "--actor-triangle-spike") {
        std::string summary;
        const bool ok = actorTriangleSpike(&summary);
        printf("%s", summary.c_str());
        printf("[ACTOR TRIANGLE SPIKE] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }
















    // ── ICE Host Test ────────────────────────────────────────

    // ── ICE Join Test ────────────────────────────────────────
    if (std::string(argv[1]) == "--ice-join-test") {
        if (argc < 3) {
            printf("[ICE JOIN] usage: mimita.exe --ice-join-test <room-code>\nFAIL\n");
            return true;
        }

        std::string roomCode = argv[2];
        printf("[ICE TEST JOIN START] roomCode=%s processId=%lu\n",
               roomCode.c_str(), (unsigned long)GetCurrentProcessId());

        IceConfiguration iceConfig = loadIceConfig();
        if (iceConfig.turn.password.empty()) {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\nFAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig)) { printf("[ICE TEST] agent init failed\nFAIL\n"); return true; }
        if (!agent.gatherCandidates()) { printf("[ICE TEST] gather failed\nFAIL\n"); return true; }

        int waited = 0;
        while (waited < 15000) {
            if (agent.state() == IceAgentState::GatheringComplete ||
                agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (agent.state() != IceAgentState::GatheringComplete) {
            printf("[ICE TEST] gather timeout\nFAIL\n"); return true;
        }

        // Join via coordinator, get host SDP
        std::string clientSessionId = "client_" + std::to_string(GetCurrentProcessId());
        std::string sdp = agent.localSdp();

        printf("[ICE DEBUG] Join SDP first 100 chars: %.*s\n", 100, sdp.c_str());
        fflush(stdout);

        auto joinResult = MimitaNet::coordinatorIceJoin(roomCode, clientSessionId, sdp);
        if (!joinResult.ok || joinResult.hostIceDescription.empty()) {
            printf("[ICE TEST] coordinator join failed (room not found or expired)\nFAIL\n");
            return true;
        }

        printf("[ICE SIGNAL RECEIVE] role=join code=%s descBytes=%zu\n",
               roomCode.c_str(), joinResult.hostIceDescription.size());

        printf("[ICE DEBUG] Host SDP first 100 chars: %.*s\n", 100, joinResult.hostIceDescription.c_str());
        fflush(stdout);

        // Set remote (host) SDP
        if (!agent.setRemoteDescription(joinResult.hostIceDescription)) {
            printf("[ICE TEST] setRemoteDescription failed\nFAIL\n"); return true;
        }

        // Wait for connected
        printf("[ICE TEST] waiting for ICE connection...\n");
        waited = 0;
        bool connected = false;
        while (waited < 15000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::StateChanged) {
                    printf("[ICE STATE] role=join old=%d new=%d\n",
                           static_cast<int>(agent.state()), static_cast<int>(ev.newState));
                    if (ev.newState == IceAgentState::Connected || ev.newState == IceAgentState::Completed)
                        connected = true;
                }
            }
            if (connected || agent.state() == IceAgentState::Failed) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
        if (!connected) { printf("[ICE TEST] connection timeout\nFAIL\n"); return true; }
        printf("[ICE CONNECTED] role=join durationMs=%d\n", waited);

        // Small stabilization delay
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Send test payload
        const char* msg = "hello from client";
        bool sendOk = agent.send(msg, strlen(msg) + 1);
        printf("[ICE TEST SEND] role=join bytes=%zu result=%d\n", strlen(msg) + 1, sendOk);
        fflush(stdout);
        if (!sendOk) {
            printf("[ICE TEST] join send failed\nFAIL\n"); return true;
        }

        // Wait for reply
        printf("[ICE TEST] waiting for reply...\n");
        waited = 0;
        std::string reply;
        while (waited < 10000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::Recv) {
                    reply.assign(ev.data.data(), ev.data.size());
                }
            }
            if (!reply.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
        if (reply.empty()) { printf("[ICE TEST] no reply received\nFAIL\n"); return true; }
        printf("[ICE TEST RECEIVE] role=join bytes=%zu payload=%.*s\n", reply.size(), (int)reply.size(), reply.c_str());

        printf("[ICE TEST RESULT] role=join pass=1\nPASS\n");
        agent.shutdown();
        return true;
    }


    if (std::string(argv[1]) == "--replay-export-fbo-test") {
        printf("[REPLAY EXPORT FBO TEST] Starting...\n");
        // Verifies the offscreen capture FBO (create/render/read) used by Step 1
        // export. Creates a hidden GL context so it runs headlessly.
        int failures = 0, total = 0;
        auto check = [&](bool cond, const char* name) {
            total++;
            if (!cond) { printf("  FAIL: %s\n", name); failures++; }
            else { printf("  PASS: %s\n", name); }
        };
        if (!glfwInit()) {
            printf("[REPLAY EXPORT FBO TEST] glfwInit failed (no display) — SKIP\n");
            return true;
        }
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        GLFWwindow* win = glfwCreateWindow(64, 64, "mimita-fbo-test", nullptr, nullptr);
        if (!win) {
            printf("[REPLAY EXPORT FBO TEST] cannot create hidden context — SKIP\n");
            glfwTerminate();
            return true;
        }
        glfwMakeContextCurrent(win);
        bool glOk = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0;
        check(glOk, "load OpenGL (glad)");
        if (glOk) {
            check(replayExportTargetInit(32, 32), "create offscreen export FBO (32x32)");
            if (replayExportTarget().ready()) {
                auto& tgt = replayExportTarget();
                glBindFramebuffer(GL_FRAMEBUFFER, tgt.fbo);
                glViewport(0, 0, tgt.width, tgt.height);
                glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                std::vector<uint8_t> px(tgt.width * tgt.height * 3, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, tgt.fbo);
                glReadPixels(0, 0, tgt.width, tgt.height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                // Pixel (0,0) should be red (255,0,0).
                bool red = px[0] > 200 && px[1] < 60 && px[2] < 60;
                check(red, "read red pixel back from offscreen FBO");
                printf("    sample pixel(0,0) = (%u,%u,%u)\n", px[0], px[1], px[2]);
            } else {
                check(false, "offscreen FBO ready");
            }
            replayExportTargetDestroy();
            check(!replayExportTarget().ready(), "offscreen FBO destroyed");
        }
        glfwDestroyWindow(win);
        glfwTerminate();
        printf("\n[REPLAY EXPORT FBO TEST] Results: %d/%d passed, %d failed\n",
               total - failures, total, failures);
        return true;
    }


    if (std::string(argv[1]) == "-exportdiagnostic") {
        printf("[EXPORTDIAG] Running replay export diagnostic...\n");
        // Find newest replay
        std::vector<std::string> clips = listReplayClips();
        std::string reportPath = "replays/exports/export-diagnostic-report.txt";
        std::error_code ec;
        std::filesystem::create_directories("replays/exports", ec);
        FILE* report = fopen(reportPath.c_str(), "w");
        if (!report) {
            printf("[EXPORTDIAG] FAILED: cannot write report\n");
            return true;
        }
        fprintf(report, "=== EXPORT DIAGNOSTIC REPORT ===\n");
        fprintf(report, "Timestamp: %lld\n", (long long)std::time(nullptr));

        // CHECK A: Replay exists
        fprintf(report, "\n--- CHECK A: Replay exists ---\n");
        if (clips.empty()) {
            fprintf(report, "FAIL: No replays found\n");
            fclose(report);
            return true;
        }
        std::string newestPath = clips.front();
        fprintf(report, "PASS: Newest replay: %s\n", newestPath.c_str());

        // CHECK B: Replay loads
        fprintf(report, "\n--- CHECK B: Replay loads ---\n");
        ReplayClip clip;
        if (!clip.load(newestPath)) {
            fprintf(report, "FAIL: Cannot load replay\n");
            fclose(report);
            return true;
        }
        fprintf(report, "PASS: tickCount=%u duration=%.1fs map=%s\n",
                clip.header.tickCount, (float)clip.header.tickCount / 60.0f,
                clip.header.mapName);

        // CHECK C: FFmpeg exists
        fprintf(report, "\n--- CHECK C: FFmpeg ---\n");
        std::string ffmpeg = defaultFfmpegPath();
        if (!std::filesystem::exists(ffmpeg)) {
            fprintf(report, "FAIL: ffmpeg not found at %s\n", ffmpeg.c_str());
            fclose(report);
            return true;
        }
        fprintf(report, "PASS: ffmpeg found at %s\n", ffmpeg.c_str());

        // CHECK D: ffmpeg -version
        fprintf(report, "\n--- CHECK D: ffmpeg -version ---\n");
        std::string versionCmd = "\"" + ffmpeg + "\" -version 2>&1";
        FILE* vp = _popen(versionCmd.c_str(), "r");
        if (vp) {
            char vbuf[256];
            if (fgets(vbuf, sizeof(vbuf), vp))
                fprintf(report, "PASS: %s", vbuf);
            _pclose(vp);
        } else {
            fprintf(report, "FAIL: cannot run ffmpeg -version\n");
        }

        // CHECK E: Load into gReplayPlayer
        fprintf(report, "\n--- CHECK E: Load into gReplayPlayer ---\n");
        {
            ReplayPlayer diagnosticPlayer;
            if (!diagnosticPlayer.loadFromJSON(newestPath)) {
                fprintf(report, "FAIL: Cannot load replay into ReplayPlayer\n");
            } else {
                fprintf(report, "PASS: totalTicks=%u\n", diagnosticPlayer.totalTicks());
                diagnosticPlayer.beginPlayback();
                fprintf(report, "PASS: beginPlayback OK. isPlaying=%d\n", (int)diagnosticPlayer.isPlaying());
                diagnosticPlayer.seekToTick(0);
                const ReplaySceneFrame* frame = diagnosticPlayer.currentSceneFrame();
                fprintf(report, "PASS: seekToTick(0). hasSceneFrame=%d\n", frame ? 1 : 0);
                if (frame)
                    fprintf(report, "PASS: actors=%zu\n", frame->actors.size());
            }
        }

        // CHECK F: Output path
        fprintf(report, "\n--- CHECK F: Output path ---\n");
        std::string outputPath = generateExportOutputPath();
        fprintf(report, "PASS: outputPath=%s\n", outputPath.c_str());

        fprintf(report, "\n=== DIAGNOSTIC COMPLETE ===\n");
        fclose(report);
        printf("[EXPORTDIAG] Report written to %s\n", reportPath.c_str());
        return true;
    }

    return false;
}
