// 08 19 2026, 09 41
/* purpose
* Selects the process mode, initializes the matching runtime, and owns shutdown.
* Parses full-client and replay-export subprocess launch options.
* Runs the top-level client loop through subsystem entry points.
* Does NOT implement gameplay, rendering passes, or video encoding.
* Does NOT own network packet formats or server simulation.
* Does NOT contain feature-specific HUD drawing.
*/

// 6 12 2026 todo plz make main like literal 100 lines
// every file ideally is 100 lines or less, and just exposes 1 function 
//like main just calls functions from other files, and we condense etc
// bc this is so much lines arghhhhhhhh
// although idk i just want it to prefromr good and be simple ish enough  to aedit and code
// etc
// so  it might not be  big deal bc ai is so strong now  Ok Ai Andy 

#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <windows.h>
#include <nlohmann/json.hpp>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "camera.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "render/render-world.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "physics/physics-mini.h"
#include "physics/physics-debug-movement.h"
#include "physics/ray-utils.h"
#include "physics/movement/physics-collision.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-editor.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "game/game-cli.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/crash-handler.h"
#include "debug/transform-debug.h"
#include "debug/transform-debug-commands.h"
#include "debug/debug-diag.h"
#include "network/net_mode.h"
#include "network/udp-echo.h"
#include "network/multiplayer-context.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"

#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "ui/hitmarker.h"
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "replay/replay-camera.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-commands.h"
#include "video/video-settings.h"
#include "video/outro.h"
#include "video/frame-pacer.h"
#include "sim/sim-context.h"
#include "engine/engine-tick.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "void-death/void-death.h"
#include "crosshair/crosshair-commands.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"
#include "config/player-settings.h"
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
#include "render/lighting-config.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/module-host.h"
#include "host/host-runtime.h"
#include "host/host-transport-adapter.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"

// Optional startup behavior for AI/dev launches. Normal launches leave these
// flags false and retain the existing window/console behavior.
bool gAiLaunchMode = false;
bool gAiNoConsole = false;
bool gUnlockCursorOnLaunch = false;

namespace {

void writeAgentQueueEvent(const std::string& eventName,
                          const std::string& requestId,
                          const std::string& reason,
                          uint32_t tick,
                          const nlohmann::json& fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General,
        StructuredLevel::Important,
        eventName,
        requestId.empty() ? "agent-queue" : requestId,
        reason,
        tick,
        fields,
        __FILE__,
        __LINE__,
        __func__);
}

void processAgentCommandQueue(const std::string& queuePath,
                              bool gameplayReady,
                              uint32_t tick)
{
    static std::string activePath;
    static std::streamoff offset = 0;
    static std::vector<std::string> deferredLines;
    static std::unordered_set<std::string> reportedDeferredRequests;
    if (queuePath.empty())
        return;
    if (activePath != queuePath)
    {
        activePath = queuePath;
        offset = 0;
        deferredLines.clear();
        reportedDeferredRequests.clear();
    }

    std::ifstream queue(queuePath, std::ios::binary);
    if (queue.is_open())
    {
        queue.seekg(offset);
        std::string line;
        while (std::getline(queue, line))
        {
            const std::streampos next = queue.tellg();
            if (next >= 0)
                offset = static_cast<std::streamoff>(next);
            else
            {
                std::error_code error;
                offset = static_cast<std::streamoff>(
                    std::filesystem::file_size(queuePath, error));
            }
            if (!line.empty())
                deferredLines.push_back(std::move(line));
        }
    }

    std::vector<std::string> remaining;
    std::unordered_set<std::string> blockedScenarios;
    for (const std::string& deferredLine : deferredLines)
    {
        try
        {
            const nlohmann::json request = nlohmann::json::parse(deferredLine);
            const std::string requestId = request.value("request_id", "");
            const std::string command = request.value("command", "");
            const std::string scenarioId = request.value("scenario_id", "");
            if (requestId.empty() || command.empty())
            {
                writeAgentQueueEvent(
                    "dev-loop.command.invalid", requestId,
                    "agent queue entry missing request_id or command", tick,
                    {{"queue_path", queuePath}, {"line", deferredLine}});
                continue;
            }

            if (!scenarioId.empty() &&
                blockedScenarios.find(scenarioId) != blockedScenarios.end())
            {
                remaining.push_back(deferredLine);
                continue;
            }

            // Server lifecycle commands must remain available after leaving a
            // room, even while gameplay-only requests wait for a world.
            const bool startsLocalServer =
                command == "server_start_preset" ||
                command.rfind("server_start_preset ", 0) == 0;
            const bool gameplayIndependent =
                startsLocalServer || command == "server_leave";
            const uint64_t notBeforeMs = request.value("not_before_ms", uint64_t{0});
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            const char* deferredReason = nullptr;
            if (notBeforeMs > nowMs)
                deferredReason = "not_before_time";
            else if (!gameplayReady && !gameplayIndependent)
                deferredReason = "gameplay_not_ready";

            if (deferredReason)
            {
                if (reportedDeferredRequests.insert(requestId).second)
                    writeAgentQueueEvent(
                        "dev-loop.command.deferred", requestId,
                        "queued command is waiting for its required runtime state",
                        tick,
                        {{"command", command}, {"scenario_id", scenarioId},
                         {"reason", deferredReason},
                         {"gameplay_ready", gameplayReady},
                         {"not_before_ms", notBeforeMs}});
                remaining.push_back(deferredLine);
                if (!scenarioId.empty())
                    blockedScenarios.insert(scenarioId);
                continue;
            }

            nlohmann::json received = {
                {"request_id", requestId}, {"command", command},
                {"queue_path", queuePath}, {"client_tick", tick},
                {"scenario_id", scenarioId},
                {"step_id", request.value("step_id", "")},
                {"step_index", request.value("step_index", -1)},
                {"step_count", request.value("step_count", -1)},
                {"gameplay_ready", gameplayReady},
                {"not_before_ms", notBeforeMs}};
            writeAgentQueueEvent(
                "dev-loop.command.delivered", requestId,
                "client consumed queued command", tick, received);
            reportedDeferredRequests.erase(requestId);

            Terminal::instance().executeWithRequestId(command, requestId);

            if (!scenarioId.empty())
            {
                writeAgentQueueEvent(
                    "scenario.step", requestId,
                    "scenario command executed on client thread", tick,
                    received);
                const int stepIndex = request.value("step_index", -1);
                const int stepCount = request.value("step_count", -1);
                if (stepIndex >= 0 && stepCount > 0 && stepIndex + 1 >= stepCount)
                {
                    nlohmann::json finished = received;
                    // Delivery/execution is not the same as scenario assertion
                    // evaluation.  Keep the journal honest until the control
                    // client has compared the requested pose invariants.
                    finished["assertions_evaluated"] = false;
                    finished["execution_complete"] = true;
                    writeAgentQueueEvent(
                        "scenario.finished", requestId,
                        "all scenario commands returned", tick, finished);
                }
            }
        }
        catch (const std::exception& error)
        {
            writeAgentQueueEvent(
                "dev-loop.command.invalid", "",
                "agent queue JSON parse failed", tick,
                {{"queue_path", queuePath}, {"error", error.what()},
                 {"line", deferredLine}});
        }
    }
    deferredLines = std::move(remaining);
}

struct AgentCommandFileQueue
{
    std::mutex mutex;
    std::deque<std::string> lines;
    std::string path;
    bool watcherStarted = false;
};

AgentCommandFileQueue& agentCommandFileQueue()
{
    // Deliberately process-lifetime: the watcher is detached and the resident
    // host owns this queue for the whole process.
    static AgentCommandFileQueue* queue = new AgentCommandFileQueue();
    return *queue;
}

void startAgentCommandWatcher(const std::string& queuePath)
{
    AgentCommandFileQueue& queue = agentCommandFileQueue();
    if (queue.watcherStarted) return;
    queue.path = queuePath;
    queue.watcherStarted = true;
    std::thread([&queue]() {
        std::streamoff offset = 0;
        std::filesystem::file_time_type lastWrite{};
        std::uintmax_t lastSize = 0;
        bool haveSignature = false;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::error_code signatureError;
            const std::uintmax_t fileSize =
                std::filesystem::file_size(queue.path, signatureError);
            if (signatureError) continue;
            const auto fileWrite =
                std::filesystem::last_write_time(queue.path, signatureError);
            if (signatureError) continue;
            if (haveSignature && fileSize == lastSize && fileWrite == lastWrite)
                continue;
            lastSize = fileSize;
            lastWrite = fileWrite;
            haveSignature = true;
            if (offset > static_cast<std::streamoff>(fileSize)) offset = 0;

            std::ifstream file(queue.path, std::ios::binary);
            if (!file.is_open()) continue;
            file.seekg(offset);
            std::vector<std::string> appended;
            std::string line;
            while (std::getline(file, line)) {
                const std::streampos next = file.tellg();
                offset = next >= 0 ? static_cast<std::streamoff>(next)
                                   : static_cast<std::streamoff>(fileSize);
                if (!line.empty()) appended.push_back(std::move(line));
            }
            if (appended.empty()) continue;
            std::lock_guard<std::mutex> lock(queue.mutex);
            for (std::string& item : appended)
                queue.lines.push_back(std::move(item));
        }
    }).detach();
}

void processAgentCommandQueueForDll(const std::string& queuePath,
                                    uint32_t tick)
{
    if (queuePath.empty()) return;
    startAgentCommandWatcher(queuePath);

    std::deque<std::string> pending;
    {
        AgentCommandFileQueue& queue = agentCommandFileQueue();
        std::lock_guard<std::mutex> lock(queue.mutex);
        pending.swap(queue.lines);
    }
    for (const std::string& line : pending) {
        try {
            const nlohmann::json request = nlohmann::json::parse(line);
            const std::string command = request.value("command", "");
            if (command.empty()) continue;
            mimita::host::AgentCommandV1 event{};
            std::snprintf(event.command, sizeof(event.command), "%s", command.c_str());
            std::snprintf(event.requestId, sizeof(event.requestId), "%s",
                          request.value("request_id", "").c_str());
            std::snprintf(event.scenarioId, sizeof(event.scenarioId), "%s",
                          request.value("scenario_id", "").c_str());
            event.stepIndex = request.value("step_index", 0u);
            event.stepCount = request.value("step_count", 0u);
            event.notBeforeMs = request.value("not_before_ms", uint64_t{0});
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "dev-loop.command.host-read", event.requestId,
                "host-owned frame consumed queued agent command", tick,
                {{"command", command}}, __FILE__, __LINE__, __func__);
            const bool dispatched =
                mimita::host::HostRuntime::instance().dispatchAgentCommand(event);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "dev-loop.command.host-dispatched", event.requestId,
                dispatched ? "agent command dispatched to active generation"
                           : "agent command dispatch failed",
                tick, {{"command", command}, {"dispatched", dispatched}},
                __FILE__, __LINE__, __func__);
        } catch (const std::exception& error) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "dev-loop.command.invalid", "agent-queue",
                "host-owned game command JSON parse failed", tick,
                {{"queue_path", queuePath}, {"error", error.what()}},
                __FILE__, __LINE__, __func__);
        }
    }
}

} // namespace

// todo sort 6 7 2026 alphabetical
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"
#include "terminal/debug-commands.h"
#include "auth/auth-system.h"
#include "terminal/player-commands.h"
#include "terminal/weapon-commands.h"
#include "terminal/npc-commands.h"
#include "terminal/duel-commands.h"
#include "terminal/editor-commands.h"
#include "terminal/network-commands.h"
#include "perf/perf.h"
#include "perf/perf-frame.h"
#include "replay/replay-export.h"
#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "game/gamemode-manager.h"
#include "debug/log-manager.h"
#include "camera/camera-commands.h"
#include "devtools/dev-log-commands.h"
#include "devtools/dev-overlay-commands.h"
#include "gui/gui-editor-commands.h"
#include "render/postfx-commands.h"
#include "render/lighting-commands.h"
#include "video/video-commands.h"
#include "audio/music-commands.h"
#include "main-init.h"
#include "replay/replay-export-subprocess.h"
#include <windows.h>

// 6 9 2026 sort and be more aweosme
// duelamanger should be  a game manager, with specific modes in it
// not all in main todo 
DuelManager gDuelManager;
GamemodeManager gGamemodeManager;
FramePacer gFramePacer;

// Global game objects (pointers set by main() for terminal command access)
Player* gpPlayer = nullptr;
Camera* gpCamera = nullptr;
World* gpWorld = nullptr;
NpcSystem* gpNpcSystem = nullptr;
WeaponSystem* gpWeapons = nullptr;
bool* gpFreecamEnabled = nullptr;
glm::vec3* gpDeathPosition = nullptr;
int* gpSelectedEditorObject = nullptr;
bool* gpEditorMode = nullptr;
std::string* gpActiveGameMode = nullptr;
std::string* gpActiveMapPath = nullptr;
bool* gpWorldLoaded = nullptr;

ReplayRingBuffer* gpReplayRecorder = nullptr;
ReplayPlayer* gpReplayPlayer = nullptr;
std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels = nullptr;
std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels = nullptr;
ReplayFactory* gpReplayFactory = nullptr;
ReplayBrowser* gpReplayBrowser = nullptr;
ReplayTimeline* gpReplayTimeline = nullptr;
ReplayCameraMgr* gpReplayCameraMgr = nullptr;
std::unordered_map<std::string, ActorChatState>* gpReplayChatStates = nullptr;
std::vector<std::string>* gpReplayClipsCache = nullptr;

// REPLAY EXPORT UI FILTER
// Set to false if you want replay controls/debug overlays
// visible in exported videos.
bool gReplayExportRenderMode = false;
std::unordered_map<int, std::string>* gpCommandBinds = nullptr;
std::unordered_map<int, bool>* gpBindPrev = nullptr;

DuelConfig* gpDuelConfig = nullptr;
MimitaNet::MultiplayerContext* gpMpContext = nullptr;

GameState* gpGameState = nullptr;
ChatHistory* gpChatHistory = nullptr;

// mainmenu debug flag (toggle with mainmenu_debug command)
bool gMainmenuDebug = false;
// replay cinematic mode (toggle with L key during playback)
bool gReplayCinematicMode = false;

// --export-replay subprocess mode globals
static bool gExportSubprocessMode = false;
static bool gExportVisible = true;
static bool gExportVerbose = false;
static std::string gExportClipPath;
static std::string gExportOutputPath;
static int gExportWidth = 1920;
static int gExportHeight = 1080;
// network debug flags (toggled by net_debug_* commands)
bool gNetPresentationDebug = false;
bool gNetDebugEntities = false;
// room code HUD display toggle (toggled by roomcodeshow command)
bool gRoomCodeShow = true;

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ai-launch") == 0)
            gAiLaunchMode = true;
        else if (strcmp(argv[i], "--ai-no-console") == 0)
            gAiNoConsole = true;
        else if (strcmp(argv[i], "--unlock-cursor") == 0)
            gUnlockCursorOnLaunch = true;
    }

    if (gAiNoConsole) {
        if (HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
    }

    // ── Parse --export-replay subprocess flags BEFORE other CLI handling ─
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--export-replay") == 0 && i + 1 < argc) {
            gExportSubprocessMode = true;
            gExportClipPath = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            gExportOutputPath = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            gExportWidth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            gExportHeight = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--visible") == 0) {
            gExportVisible = true;
        } else if (strcmp(argv[i], "--replay-export-verbose") == 0) {
            gExportVerbose = true;
        }
    }

    // In subprocess mode, hide the window unless --visible is specified (debug).
    // glfwInit is idempotent; Renderer::init also calls it but the hint persists.
    if (gExportSubprocessMode) {
        glfwInit();
        glfwWindowHint(GLFW_VISIBLE, gExportVisible ? GLFW_TRUE : GLFW_FALSE);
        printf("[MAIN] export subprocess mode: clip=%s output=%s %dx%d visible=%d\n",
               gExportClipPath.c_str(), gExportOutputPath.c_str(),
               gExportWidth, gExportHeight, gExportVisible);
    }

    if (handleGameCLI(argc, argv)) return 0;

    printf("[BUILD] compiled on %s at %s\n", __DATE__, __TIME__);
    printf("[BUILD] commit=64b085c\n");

    MimitaNet::LaunchOptions launchOptions = MimitaNet::parseLaunchOptions(argc, argv);
    installCrashHandler();
    const char* runId = std::getenv("MIMITA_RUN_ID");
    const char* instanceId = std::getenv("MIMITA_INSTANCE_ID");
    if (launchOptions.server || (runId && runId[0] && instanceId && instanceId[0]))
        setCrashHandlerShowDialog(false);
    if (launchOptions.server && launchOptions.client)
    {
        printf("[MAIN] choose only one mode: --server or --client\n");
        MimitaNet::printLaunchUsage();
        return 1;
    }
    if (launchOptions.udpEcho)
    {
        printf("[BOOT MODE] mode=udp-echo-server graphicsInitialized=0 uiInitialized=0\n");
        return MimitaNet::runUdpEchoServer(launchOptions);
    }
    const auto configureWholeGameHost = []() {
        if (std::getenv("MIMITA_GAME_PROBE") ||
            std::getenv("MIMITA_GAME_OWN_FRAME")) {
            return;
        }
        std::string dllPath;
        if (const char* configured = std::getenv("MIMITA_GAME_DLL")) {
            dllPath = configured;
        } else {
            std::ifstream current("build/game/current.json");
            if (current.is_open()) {
                try {
                    nlohmann::json metadata;
                    current >> metadata;
                    dllPath = metadata.value("dll", std::string());
                } catch (const std::exception&) {
                    dllPath.clear();
                }
            }
        }
        if (!dllPath.empty() && std::filesystem::exists(dllPath)) {
            _putenv_s("MIMITA_GAME_DLL", dllPath.c_str());
            _putenv_s("MIMITA_GAME_PROBE", "1");
            _putenv_s("MIMITA_GAME_OWN_FRAME", "1");
            _putenv_s("MIMITA_GAME_REQUIRE_HOT", "1");
        }
        // No generation means the client/server fails closed below. Resident
        // gameplay is no longer a fallback.
    };

    if (launchOptions.server)
    {
        printf("[BOOT MODE] mode=headless-server graphicsInitialized=0 uiInitialized=0\n");
        LogManager::instance().setLogType("Server");
        LogManager::instance().init();
        configureWholeGameHost();
        int ret = MimitaNet::runServer(launchOptions);
        LogManager::instance().shutdown();
        return ret;
    }
    if (launchOptions.client)
    {
        // Standalone --client is retired. Normal clients use the full client
        // path; the hot server and full client are the supported surfaces.
        printf("[BOOT MODE] standalone --client is retired; use the full client\n");
        return 1;
    }

    printf("[BOOT MODE] mode=full-client graphicsInitialized=1 uiInitialized=1\n");
    if (gExportSubprocessMode) {
        LogManager::instance().setLogType("ReplayExport");
    } else {
        LogManager::instance().setLogType("Gameterminal");
    }
    LogManager::instance().init();

    // In subprocess mode, log where the export log file was written
    if (gExportSubprocessMode) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS-START] pid=%lu cwd=%s argv=",
            (unsigned long)GetCurrentProcessId(),
            std::filesystem::current_path().string().c_str());
        for (int i = 0; i < 20 && i < __argc; i++)
            Debug::warn(Debug::Category::Replay, " %s", __argv[i]);
        Debug::warn(Debug::Category::Replay, "\n");
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS-LOG] path=%s\n",
            LogManager::instance().path().c_str());
    }

    // Export subprocess mode is a bounded production diagnostic. The resident
    // entry loads the generation and calls its export entry.
    if (gExportSubprocessMode) {
        if (gExportClipPath.empty()) {
            printf("[MAIN] --export-replay requires a clip path\n");
            return 1;
        }
        configureWholeGameHost();
        if (!mimita::host::HostRuntime::instance().startFromEnvironment() ||
            !mimita::host::HostRuntime::instance().gameExportAvailable()) {
            fprintf(stderr, "[HOTRELOAD] replay export requires a game generation\n");
            return 1;
        }
        const int status = mimita::host::HostRuntime::instance().gameRunExport(
            gExportClipPath.c_str(), gExportOutputPath.c_str(),
            gExportWidth, gExportHeight, gExportVisible ? 1 : 0,
            gExportVerbose ? 1 : 0);
        mimita::host::HostRuntime::instance().shutdown();
        return status;
    }

    configureWholeGameHost();

    const bool hotStarted =
        mimita::host::HostRuntime::instance().startFromEnvironment();
    const char* requireHot = std::getenv("MIMITA_GAME_REQUIRE_HOT");
    if (requireHot && std::strcmp(requireHot, "1") == 0 && !hotStarted) {
        const char* configuredDll = std::getenv("MIMITA_GAME_DLL");
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "game.client.hot-required", "client",
            "client refused to start without the requested hot generation", 0,
            {{"dll", configuredDll ? configuredDll : ""},
             {"reason", "candidate-rejected-or-missing"}},
            __FILE__, __LINE__, __func__);
        fprintf(stderr, "[HOTRELOAD] required generation failed to load: %s\n",
                configuredDll ? configuredDll : "(none)");
        return 1;
    }

    // The whole-game frame path returns below and never reaches the legacy
    // auto-join block. Queue the launcher request for the DLL before the
    // frame-ownership branch so both paths see the same join.
    const auto queueStartupConnectForDll = [&] {
        if (!launchOptions.connectExplicit && !launchOptions.roomCodeExplicit) {
            return;
        }
        MultiplayerConnectInfo info;
        info.shouldConnect = true;
        info.directAddress = launchOptions.connect;
        info.roomCode = launchOptions.roomCode;
        info.playerName = launchOptions.name;
        info.mapName = launchOptions.mapName;
        mimita::host::StartupConnectV1 request{};
        request.shouldConnect = 1;
        std::snprintf(request.directAddress, sizeof(request.directAddress), "%s",
                      info.directAddress.c_str());
        std::snprintf(request.roomCode, sizeof(request.roomCode), "%s",
                      info.roomCode.c_str());
        std::snprintf(request.playerName, sizeof(request.playerName), "%s",
                      info.playerName.c_str());
        std::snprintf(request.mapName, sizeof(request.mapName), "%s",
                      info.mapName.c_str());
        const std::size_t commandCount =
            std::min<std::size_t>(launchOptions.startupCommands.size(), 4);
        request.startupCommandCount = static_cast<std::uint32_t>(commandCount);
        for (std::size_t index = 0; index < commandCount; ++index) {
            std::snprintf(request.startupCommands[index],
                          sizeof(request.startupCommands[index]), "%s",
                          launchOptions.startupCommands[index].c_str());
        }
        mimita::host::HostRuntime::instance().queueStartupConnect(request);
    };
    queueStartupConnectForDll();

    if (mimita::host::HostRuntime::instance().ownsGameFrame()) {
        mimita::host::GameStartupV1 startup{};
        startup.version = 1;
        startup.structSize = sizeof(startup);
        startup.width = 1024;
        startup.height = 768;
        startup.title = "mimita.exe";
        std::snprintf(startup.sessionToken, sizeof(startup.sessionToken), "%s",
                      launchOptions.sessionToken.c_str());
        startup.unlockCursor = gUnlockCursorOnLaunch ? 1u : 0u;
        if (mimita::host::HostRuntime::instance().bootstrapGame(startup)) {
            while (!mimita::host::HostRuntime::instance().gameShouldClose()) {
                mimita::host::HostRuntime::instance().driveFrame(1.0 / 60.0, nullptr);
                processAgentCommandQueueForDll(
                    launchOptions.agentCommandFile,
                    static_cast<std::uint32_t>(mimita::host::HostRuntime::instance().activeGeneration()));
                mimita::host::HostRuntime::instance().poll();
            }
            mimita::host::HostRuntime::instance().shutdown();
            return 0;
        }
        mimita::host::HostRuntime::instance().shutdown();
        return 1;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Errors,
        "game.client.hot-required", "client",
        "client refused to start without a frame-owning hot generation", 0,
        {{"reason", "no-frame-owner"},
         {"dll", std::getenv("MIMITA_GAME_DLL")
                     ? std::getenv("MIMITA_GAME_DLL") : ""}},
        __FILE__, __LINE__, __func__);
    fprintf(stderr,
            "[HOTRELOAD] client requires a frame-owning game generation\n");
    mimita::host::HostRuntime::instance().shutdown();
    return 1;
}
