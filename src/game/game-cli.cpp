// 07 21 2026, 17 20
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
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <chrono>
#include <thread>
#include <windows.h>
#include <GLFW/glfw3.h>
#include "engine/engine.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "camera.h"
#include "terminal/terminal-state.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-factory.h"
#include "game/duel.h"
#include "game/game-state.h"
#include "physics/movement/physics-collision.h"
#include "debug/debug-log.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/ice/ice-test.h"
#include "network/coordinator-client.h"

extern DuelManager gDuelManager;
extern bool gMainmenuDebug;

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

    // 5. Disconnect multiplayer
    if (MP_CONTEXT.active) {
        printf("[MAINMENU] cleaning network\n");
        Debug::log(Debug::Category::General, "[MAINMENU] disconnecting multiplayer");
        MimitaNet::mpShutdown(MP_CONTEXT);
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
    GLFWwindow* win = glfwGetCurrentContext();
    if (win)
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // 10. Transition to menu
    printf("[MAINMENU] switching scene\n");
    GAME_STATE = GAME_MENU;

    logPhase("GUI Load");
    Debug::log(Debug::Category::General, "[MAINMENU] success");
}

bool handleGameCLI(int argc, char** argv)
{
    if (argc <= 1) return false;

    const std::string command = argv[1];
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
        IceTestOptions opts;
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

    if (std::string(argv[1]) == "--snapshot-chunk-selftest") {
        std::string report;
        const bool ok = MimitaNet::runSnapshotChunkSelfTest(&report);
        printf("%s", report.c_str());
        printf("[SNAPSHOT CHUNK SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        std::exit(ok ? 0 : 1);
    }

    if (std::string(argv[1]) == "--collision-selftest") {
        std::string summary;
        const bool ok = collisionStressSelfTest(&summary);
        printf("%s", summary.c_str());
        printf("[COLLISION SELFTEST] %s\n", ok ? "PASS" : "FAIL");
        return true;
    }

    if (std::string(argv[1]) == "--collision-stress") {
        const std::string caseName = argc > 2 ? argv[2] : "wedge5";
        printf("%s\n", collisionStressRun(caseName).c_str());
        return true;
    }

    if (std::string(argv[1]) == "--ice-test") {
        printf("[ICE TEST] starting ICE candidate gathering test\n");

        IceConfiguration iceConfig = loadIceConfig();

        if (iceConfig.turn.password.empty())
        {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig))
        {
            printf("[ICE TEST] agent initialization failed\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        printf("[ICE TEST] agent initialized, starting gather...\n");

        if (!agent.gatherCandidates())
        {
            printf("[ICE TEST] gather failed\n");
            printf("[ICE TEST] FAIL\n");
            return true;
        }

        // Wait for gathering to complete (or timeout)
        const int maxWaitMs = 10000;
        int waited = 0;
        while (waited < maxWaitMs)
        {
            if (agent.state() == IceAgentState::GatheringComplete ||
                agent.state() == IceAgentState::Failed)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }

        auto candidates = agent.candidates();

        int hostCount = 0, srflxCount = 0, relayCount = 0;
        for (const auto& c : candidates)
        {
            switch (c.type)
            {
            case IceCandidateType::Host:            hostCount++; break;
            case IceCandidateType::ServerReflexive:  srflxCount++; break;
            case IceCandidateType::Relay:           relayCount++; break;
            default: break;
            }
        }

        printf("\n[ICE TEST] === CANDIDATE SUMMARY ===\n");
        printf("[ICE TEST] Host candidates:  %d\n", hostCount);
        printf("[ICE TEST] SRFLX candidates: %d\n", srflxCount);
        printf("[ICE TEST] Relay candidates: %d\n", relayCount);
        printf("[ICE TEST] Total candidates: %zu\n", candidates.size());
        printf("[ICE TEST] State: %d\n", static_cast<int>(agent.state()));

        bool ok = (hostCount >= 1 && srflxCount >= 1 && relayCount >= 1);
        printf("[ICE TEST] %s\n", ok ? "PASS" : "FAIL");

        if (!ok)
        {
            printf("[ICE TEST] Missing candidate types - check STUN/TURN configuration.\n");
            if (srflxCount == 0)
                printf("[ICE TEST]   No server-reflexive candidates - is STUN reachable?\n");
            if (relayCount == 0)
                printf("[ICE TEST]   No relay candidates - is TURN configured correctly?\n");
        }

        agent.shutdown();
        return true;
    }

    // ── ICE Host Test ────────────────────────────────────────
    if (std::string(argv[1]) == "--ice-host-test") {
        printf("[ICE TEST HOST START] processId=%lu\n", (unsigned long)GetCurrentProcessId());
        fflush(stdout);

        IceConfiguration iceConfig = loadIceConfig();
        if (iceConfig.turn.password.empty()) {
            printf("[ICE TEST] TURN password not set. Set MIMITA_TURN_PASSWORD env var.\nFAIL\n");
            return true;
        }

        IceAgent agent;
        if (!agent.initialize(iceConfig)) { printf("[ICE TEST] agent init failed\nFAIL\n"); return true; }
        if (!agent.gatherCandidates()) { printf("[ICE TEST] gather failed\nFAIL\n"); return true; }

        // Wait for gathering to finish
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

        // Register with coordinator and upload description
        std::string sessionId = "host_" + std::to_string(GetCurrentProcessId());
        std::string sdp = agent.localSdp();

        printf("[ICE DEBUG] Host SDP first 100 chars: %.*s\n", 100, sdp.c_str());
        fflush(stdout);

        auto hostResult = MimitaNet::coordinatorIceHost(sessionId, sdp);
        if (!hostResult.ok) { printf("[ICE TEST] coordinator register failed\nFAIL\n"); return true; }

        printf("[ICE TEST ROOM] code=%s\n", hostResult.roomCode.c_str());
        fflush(stdout);
        printf("[ICE SIGNAL UPLOAD] role=host code=%s descBytes=%zu\n", hostResult.roomCode.c_str(), sdp.size());
        fflush(stdout);

        // Poll for client
        printf("[ICE TEST] waiting for joiner...\n");
        fflush(stdout);
        std::string clientDesc;
        bool gotClient = false;
        waited = 0;
        while (waited < 60000) {
            auto pollResult = MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
            if (pollResult.ok && pollResult.status == "client_ready" && !pollResult.clientIceDescription.empty()) {
                clientDesc = pollResult.clientIceDescription;
                gotClient = true;
                printf("[ICE SIGNAL RECEIVE] role=host code=%s descBytes=%zu\n", hostResult.roomCode.c_str(), clientDesc.size());
                printf("[ICE DEBUG] Client SDP first 100: %.*s\n", 100, clientDesc.c_str());
                fflush(stdout);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            waited += 500;
        }
        if (!gotClient) { printf("[ICE TEST] client poll timeout\nFAIL\n"); return true; }

        // Set remote description
        if (!agent.setRemoteDescription(clientDesc)) {
            printf("[ICE TEST] setRemoteDescription failed\nFAIL\n"); return true;
        }

        // Wait for connected state
        printf("[ICE TEST] waiting for ICE connection...\n");
        waited = 0;
        bool connected = false;
        while (waited < 15000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::StateChanged) {
                    printf("[ICE STATE] role=host old=%d new=%d\n",
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
        printf("[ICE CONNECTED] role=host durationMs=%d\n", waited);

        // Small stabilization delay
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        printf("[ICE TEST] waiting for data from joiner...\n");
        waited = 0;
        std::string recvd;
        while (waited < 10000) {
            std::vector<IceEvent> events;
            agent.pollEvents(events);
            for (auto& ev : events) {
                if (ev.type == IceEventType::Recv) {
                    recvd.assign(ev.data.data(), ev.data.size());
                }
            }
            if (!recvd.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waited += 50;
        }
        if (recvd.empty()) { printf("[ICE TEST] no data received\nFAIL\n"); return true; }
        printf("[ICE TEST RECEIVE] role=host bytes=%zu payload=%.*s\n", recvd.size(), (int)recvd.size(), recvd.c_str());

        // Reply
        const char* reply = "hello from host";
        if (!agent.send(reply, strlen(reply) + 1)) {
            printf("[ICE TEST] host send failed\nFAIL\n"); return true;
        }
        printf("[ICE TEST SEND] role=host bytes=%zu\n", strlen(reply) + 1);

        MimitaNet::coordinatorIceDone(hostResult.roomCode);
        printf("[ICE TEST RESULT] role=host pass=1\nPASS\n");
        agent.shutdown();
        return true;
    }

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

    if (std::string(argv[1]) == "--replay-selftest") {
        ReplayClip clip;
        clip.header.tickRate = 60;
        clip.header.tickCount = 2;
        clip.mapPath = "assets/maps/mimita-duels-map-v3.glb";
        clip.killerId = "player";
        clip.victimId = "npc_100";
        clip.killTick = 30;

        ReplayActorState actorA;
        actorA.id = "player";
        actorA.name = "player";
        actorA.type = "player";
        actorA.position = {0.0f, 0.0f, 0.0f};
        actorA.weaponName = "revolver";
        ReplaySceneFrame frameA;
        frameA.tick = 0;
        frameA.actors.push_back(actorA);

        ReplayActorState actorB = actorA;
        actorB.position = {10.0f, 0.0f, 0.0f};
        ReplaySceneFrame frameB;
        frameB.tick = 60;
        frameB.time = 1.0f;
        frameB.actors.push_back(actorB);
        clip.sceneFrames = {frameA, frameB};

        const std::filesystem::path path =
            std::filesystem::path("build") / "replay-selftest.mclip.json";
        ReplayPlayer playerTest;
        const bool saved = clip.save(path.string());
        const bool loaded = saved && playerTest.loadFromJSON(path.string());
        playerTest.setTimescale(0.25f);
        playerTest.beginPlayback();
        playerTest.update(1.0f);
        const ReplaySceneFrame* interpolated =
            playerTest.currentSceneFrame();
        const bool interpolationOk =
            interpolated && !interpolated->actors.empty() &&
            std::fabs(interpolated->actors.front().position.x - 2.5f) < 0.01f;
        const bool camerasOk =
            playerTest.cameraController().setMode("fp") &&
            playerTest.cameraController().setMode("victim") &&
            playerTest.cameraController().setMode("orbit") &&
            playerTest.cameraController().setMode("freecam");
        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        printf("[REPLAY SELFTEST] save=%d load=%d interpolation=%d cameras=%d\n",
               (int)saved, (int)loaded, (int)interpolationOk, (int)camerasOk);
        return true;
    }

    if (std::string(argv[1]) == "--replay-export-selftest") {
        printf("[REPLAY EXPORT SELFTEST] Starting...\n");

        int failures = 0;
        int total = 0;
        auto check = [&](bool cond, const char* name) {
            total++;
            if (!cond) { printf("  FAIL: %s\n", name); failures++; }
            else { printf("  PASS: %s\n", name); }
        };

        // 1. Create a tiny replay file
        printf("\n--- Creating test replay ---\n");
        ReplayClip clip;
        clip.header.tickRate = 60;
        clip.header.tickCount = 10;
        clip.mapPath = "assets/maps/mimita-duels-map-v3.glb";
        clip.killerId = "player";
        clip.victimId = "npc_100";
        clip.killTick = 5;

        ReplayActorState actorA;
        actorA.id = "player";
        actorA.name = "player";
        actorA.type = "player";
        actorA.position = {0.0f, 0.0f, 0.0f};
        actorA.weaponName = "revolver";
        for (uint32_t i = 0; i < 10; i++) {
            ReplaySceneFrame f;
            f.tick = (int)i;
            f.time = (float)i / 60.0f;
            actorA.position.x = (float)i * 2.0f;
            f.actors.push_back(actorA);
            clip.sceneFrames.push_back(f);
        }

        const std::filesystem::path selftestDir =
            std::filesystem::path("build") / "replay-export-selftest";
        std::error_code ec;
        std::filesystem::create_directories(selftestDir, ec);
        std::filesystem::path replayPath = selftestDir / "selftest.mclip.json";
        bool saved = clip.save(replayPath.string());
        check(saved, "create test replay file");
        if (!saved) { printf("[REPLAY EXPORT SELFTEST] FAILED: cannot create replay\n"); return true; }

        // 2. Verify replay loads and advances
        printf("\n--- Verifying replay advances ---\n");
        ReplayPlayer player;
        bool loaded = player.loadFromJSON(replayPath.string());
        check(loaded, "load test replay into player");
        if (!loaded) { printf("[REPLAY EXPORT SELFTEST] FAILED: cannot load replay\n"); return true; }

        player.beginPlayback();
        check(player.isPlaying(), "beginPlayback sets isPlaying");

        player.seekToTick(0);
        const ReplaySceneFrame* frame0 = player.currentSceneFrame();
        check(frame0 != nullptr && !frame0->actors.empty(), "seekToTick(0) returns scene frame with actors");
        check(frame0 && frame0->actors[0].position.x == 0.0f, "actor0.x at tick 0 == 0.0");

        player.seekToTick(5);
        player.update(0.0f);
        const ReplaySceneFrame* frame5 = player.currentSceneFrame();
        check(frame5 != nullptr, "seekToTick(5) returns scene frame");
        check(frame5 && frame5->actors[0].position.x > 0.0f, "actor0.x at tick 5 > 0.0 (advancing)");

        player.seekToTick(9);
        player.update(0.0f);
        const ReplaySceneFrame* frame9 = player.currentSceneFrame();
        check(frame9 != nullptr, "seekToTick(9) returns scene frame");

        check(player.currentTick() > 0, "currentTick > 0 (replay advances)");

        // 3. Verify ffmpeg exists
        printf("\n--- Verifying ffmpeg ---\n");
        std::string ffmpegPath = defaultFfmpegPath();
        bool ffmpegExists = std::filesystem::exists(ffmpegPath);
        check(ffmpegExists, "ffmpeg exists at default path");
        if (!ffmpegExists) {
            printf("[REPLAY EXPORT SELFTEST] ffmpeg not found, skipping ffmpeg tests\n");
        } else {
            // 4. Create a synthetic raw file and encode it with ffmpeg
            printf("\n--- Creating synthetic raw file ---\n");
            int rawW = 320, rawH = 240, rawFrames = 3;
            std::filesystem::path rawPath = selftestDir / "selftest_raw.rgb";
            std::filesystem::path mp4Path = selftestDir / "selftest_output.mp4";
            std::filesystem::path stderrPath = selftestDir / "selftest_ffmpeg_stderr.txt";

            // Write 3 frames of raw RGB data (solid red/green/blue)
            FILE* rawFile = fopen(rawPath.string().c_str(), "wb");
            bool rawOpened = (rawFile != nullptr);
            check(rawOpened, "open synthetic raw file");
            if (rawOpened) {
                for (int f = 0; f < rawFrames; f++) {
                    std::vector<uint8_t> rawFrame(rawW * rawH * 3);
                    uint8_t r = (f == 0) ? 255 : (f == 1) ? 0 : 0;
                    uint8_t g = (f == 0) ? 0 : (f == 1) ? 255 : 0;
                    uint8_t b = (f == 0) ? 0 : (f == 1) ? 0 : 255;
                    for (size_t i = 0; i < rawFrame.size(); i += 3) {
                        rawFrame[i] = r;
                        rawFrame[i+1] = g;
                        rawFrame[i+2] = b;
                    }
                    fwrite(rawFrame.data(), 1, rawFrame.size(), rawFile);
                }
                fclose(rawFile);
            }

            uint64_t rawSize = std::filesystem::file_size(rawPath, ec);
            check(rawSize > 0, "raw file exists and size > 0");
            printf("    raw file bytes=%llu\n", (unsigned long long)rawSize);

            // 5. Run ffmpeg on synthetic raw file
            printf("\n--- Running ffmpeg on synthetic raw file ---\n");
            std::string rawStr = rawPath.make_preferred().string();
            std::string mp4Str = mp4Path.make_preferred().string();

            // Write a batch file to avoid cmd.exe quoting issues with std::system()
            std::filesystem::path batPath = selftestDir / "encode.bat";
            std::string batContent = "@echo off\r\n"
                "\"" + ffmpegPath + "\" -y -f rawvideo -pixel_format rgb24 "
                "-video_size " + std::to_string(rawW) + "x" + std::to_string(rawH) + " "
                "-framerate 60 -i \"" + rawStr + "\" "
                "-c:v libx264 -preset fast -pix_fmt yuv420p "
                "-crf 23 \"" + mp4Str + "\" "
                "-loglevel error\r\n"
                "exit /b %ERRORLEVEL%\r\n";
            FILE* batFile = fopen(batPath.string().c_str(), "w");
            if (batFile) {
                fwrite(batContent.c_str(), 1, batContent.size(), batFile);
                fclose(batFile);
            }
            int ffmpegResult = std::system(batPath.string().c_str());
            printf("    ffmpeg exit code=%d\n", ffmpegResult);

            // 6. Verify mp4 exists and has size > 0
            printf("\n--- Verifying mp4 output ---\n");
            bool mp4Exists = std::filesystem::exists(mp4Path, ec);
            check(mp4Exists, "mp4 file exists");
            bool mp4SizeOk = false;
            if (mp4Exists) {
                uint64_t mp4Size = std::filesystem::file_size(mp4Path, ec);
                mp4SizeOk = mp4Size > 0;
                check(mp4SizeOk, "mp4 file size > 0");
                printf("    mp4 bytes=%llu\n", (unsigned long long)mp4Size);
            } else {
                check(false, "mp4 file size > 0");
            }
        }

        // Cleanup
        printf("\n--- Cleanup ---\n");
        std::filesystem::remove_all(selftestDir, ec);

        printf("\n[REPLAY EXPORT SELFTEST] Results: %d/%d passed, %d failed\n",
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
