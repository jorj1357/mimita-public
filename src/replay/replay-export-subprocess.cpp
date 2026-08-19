// 08 19 2026, 09 50
/* purpose
* Runs a replay export as a standalone subprocess: loads a clip and map,
* renders each frame offscreen, encodes to MP4, appends the outro, and exits.
* This runs in a SEPARATE mimita.exe process so the main game keeps playing.
* Does NOT own the main game loop, networking, or live gameplay.
* Does NOT own replay recording or editor command registration.
* Does NOT choose which HUD elements the centralized UI pass renders.
*/
#include "replay/replay-export-subprocess.h"

#include <cstdio>
#include <string>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_set>

#include "engine/engine.h"
#include "engine/engine-tick.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "terminal/terminal-state.h"
#include "game/game-state.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-export-target.h"
#include "replay/replay-editor.h"
#include "debug/debug-log.h"
#include "video/outro.h"
#include "render/post-fx.h"
#include "combat/weapon-model-cache.h"

extern bool WORLD_LOADED;
extern ReplayExportJob gJob;

static bool waitForReplayWeaponModels(const ReplayClip& clip)
{
    std::vector<std::shared_ptr<PendingWeaponModel>> assets;
    std::unordered_set<std::string> paths;
    for (const ReplaySceneFrame& frame : clip.sceneFrames) {
        for (const ReplayActorState& actor : frame.actors) {
            if (!actor.weaponModelPath.empty() && paths.insert(actor.weaponModelPath).second)
                assets.push_back(WeaponModelCache::instance().request(actor.weaponModelPath));
        }
    }

    if (assets.empty())
        return true;

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] waiting for %zu replay weapon model(s)\n", assets.size());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    for (;;) {
        WeaponModelCache::instance().finalizeWeaponModelsIfReady();
        bool allReady = true;
        for (const auto& asset : assets) {
            if (!asset || !asset->ready.load()) {
                allReady = false;
                continue;
            }
            if (!asset->loadOk) {
                Debug::error(Debug::Category::Replay,
                    "[EXPORT-SUBPROCESS] FAILED: replay weapon model could not load %s\n",
                    asset->path.c_str());
                return false;
            }
            if (!asset->gpuUploaded)
                allReady = false;
        }
        if (allReady) {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT-SUBPROCESS] replay weapon models ready\n");
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            Debug::error(Debug::Category::Replay,
                "[EXPORT-SUBPROCESS] FAILED: timed out waiting for replay weapon models\n");
            return false;
        }
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void runExportSubprocess(Engine& engine, const char* clipPath, const char* outputPath,
                         int width, int height)
{
    namespace fs = std::filesystem;
    if (width <= 0 || height <= 0) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: invalid render size %dx%d\n", width, height);
        return;
    }

    // Normal client initialization applies the user's video settings. Export
    // owns a separate process, so restore its window and render targets to the
    // requested capture aspect before any replay frame is rendered.
    // Hidden GLFW windows can report a stale/zero framebuffer size until they
    // have been shown and processed once.  The 3D renderer already uses the
    // requested dimensions explicitly, so make the window's UI viewport
    // converge to the same drawable size before the first engine tick.
    glfwShowWindow(engine.window());
    glfwSetWindowSize(engine.window(), width, height);
    glfwPollEvents();
    int framebufferWidth = width;
    int framebufferHeight = height;
    glfwGetFramebufferSize(engine.window(), &framebufferWidth, &framebufferHeight);
    engine.renderer->width = std::max(framebufferWidth, 1);
    engine.renderer->height = std::max(framebufferHeight, 1);
    PostFX::instance().initFBO(engine.renderer->width, engine.renderer->height);

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip=%s output=%s requested=%dx%d framebuffer=%dx%d\n",
        clipPath, outputPath, width, height,
        engine.renderer->width, engine.renderer->height);

    // ── 1. Load the clip ──────────────────────────────────────────────
    if (!REPLAY_PLAYER.loadFromJSON(clipPath)) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: cannot load clip %s\n", clipPath);
        return;
    }
    const auto& hdr = REPLAY_PLAYER.header();
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip loaded: ticks=%u rate=%u map=%s\n",
        hdr.tickCount, hdr.tickRate, hdr.mapName);

    // ── 2. Load the world from the clip's map path ────────────────────
    // The clip stores its map path; we must load it so the scene renders.
    // We explicitly own the map state: set WORLD_LOADED and activeMapPath
    // BEFORE the tick loop to prevent engine-tick-state.cpp from loading
    // the default map as fallback (which would show the wrong scene).
    ReplayClip tempClip;
    bool worldLoadOk = false;
    if (tempClip.load(clipPath) && !tempClip.mapPath.empty()) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] loading world: %s\n", tempClip.mapPath.c_str());
        worldLoadOk = loadWorldFromGLB(THE_WORLD, tempClip.mapPath.c_str());
        if (worldLoadOk) {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT-SUBPROCESS] world loaded OK\n");
        } else {
            Debug::error(Debug::Category::Replay,
                "[EXPORT-SUBPROCESS] FAILED to load world: %s\n", tempClip.mapPath.c_str());
        }
    } else {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] clip has no mapPath (empty or missing metadata)\n");
    }

    if (!waitForReplayWeaponModels(tempClip))
        return;

    // Always own the world state to block the default-map fallback
    // in engine-tick-state.cpp:160 (GAME_PLAYING && !WORLD_LOADED triggers it).
    WORLD_LOADED = true;
    if (gpActiveMapPath) {
        *gpActiveMapPath = tempClip.mapPath;
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] activeMapPath=%s worldLoaded=%d\n",
            tempClip.mapPath.c_str(), worldLoadOk);
    }

    // Set GAME_PLAYING AFTER map state is fully resolved so the render
    // pipeline does not start until the world is ready.
    if (gpGameState) *gpGameState = GAME_PLAYING;

    // ── 3. Setup editor (camera keyframes if present) ─────────────────
    gReplayEditor.load(clipPath);
    if (gReplayEditor.isLoaded() && gReplayEditor.cameraKeyframeCount() > 0) {
        gReplayEditor.freecam = true;
        REPLAY_PLAYER.cameraController().setMode("freecam");
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] editor loaded, freecam enabled (%d camera KFs)\n",
            gReplayEditor.cameraKeyframeCount());
    }

    // ── 4. Setup export job ───────────────────────────────────────────
    // Reuse the existing gJob state machine.
    gJob = ReplayExportJob{};
    gJob.state = ReplayExportJob::Capturing;
    gJob.jsonPath = clipPath;
    gJob.totalTicks = REPLAY_PLAYER.totalTicks();
    gJob.capturedTicks = 0;
    gJob.exportTick = 0.0f;
    gJob.capWidth = width;
    gJob.capHeight = height;
    gJob.outputWidth = width;
    gJob.outputHeight = height;
    gJob.ffmpegPath = defaultFfmpegPath();
    gJob.outputPath = outputPath;
    gJob.ffmpegExitCode = -1;
    gJob.frameWriteCount = 0;
    gJob.rawFileBytes = 0;
    gJob.mp4FileBytes = 0;
    gJob.startTimeSec = replayExportNowSec();
    replayExportTimingReset();

    // Create temp directories
    std::error_code ec;
    fs::create_directories(fs::path("replays") / "exports" / "_tmp", ec);

    // Create raw file for ffmpeg encoding path
    gJob.rawTempPath = (fs::path("replays") / "exports" / "_tmp" / "subprocess_raw.rgb").string();
    gJob.rawFile = fopen(gJob.rawTempPath.c_str(), "wb");
    if (!gJob.rawFile) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: cannot create raw file %s\n", gJob.rawTempPath.c_str());
        return;
    }

    // Create the offscreen capture FBO
    if (!replayExportTargetInit(width, height)) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] WARNING: export FBO unavailable, using window read\n");
    }

    // ── 5. Begin replay playback ──────────────────────────────────────
    REPLAY_PLAYER.beginPlayback();
    REPLAY_PLAYER.seekToTick(0);

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] capture start: %u ticks %dx%d pid=%lu\n",
        gJob.totalTicks, width, height, (unsigned long)GetCurrentProcessId());
    glfwSetWindowTitle(engine.window(), "MiMITA Replay Export - Recording...");

    // ── 6. Capture loop ───────────────────────────────────────────────
    // Each engineTick runs the full game loop (replay update, render, capture).
    // The capture happens inside updateReplayExport which reads pixels and
    // writes raw frames. Once all frames are captured, the state moves to
    // Encoding (ffmpeg subprocess) or Done.
    while (gJob.state == ReplayExportJob::Capturing ||
           gJob.state == ReplayExportJob::Encoding) {
        engineTick(engine);

        // Periodic progress report
        if (gJob.state == ReplayExportJob::Capturing && gJob.totalTicks > 0) {
            static int lastPct = -1;
            int pct = (int)((float)gJob.exportTick / (float)gJob.totalTicks * 100.0f);
            int report = (pct / 10) * 10;
            if (report > lastPct && report > 0) {
                lastPct = report;
                Debug::warn(Debug::Category::Replay,
                    "[EXPORT-SUBPROCESS] capture progress: %d%%\n", report);
            }
        }
    }

    // ── 7. Result ─────────────────────────────────────────────────────
    replayExportTimingLogSummary();

    if (gJob.state == ReplayExportJob::Done) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] complete: %s (%.1f MB)\n",
            gJob.outputPath.c_str(),
            (double)gJob.mp4FileBytes / (1024.0 * 1024.0));
        glfwSetWindowTitle(engine.window(),
            "MiMITA Replay Export - Complete!");
    } else if (gJob.state == ReplayExportJob::Failed) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: %s\n", gJob.errorMsg.c_str());
        glfwSetWindowTitle(engine.window(),
            "MiMITA Replay Export - Failed");
    }

    // Brief pause so user can see the result before window closes
    glfwPollEvents();
    glfwSwapBuffers(engine.window());
    Sleep(2000);

    // ── 8. Cleanup ────────────────────────────────────────────────────
    replayExportTargetDestroy();
    if (gJob.rawFile) { fclose(gJob.rawFile); gJob.rawFile = nullptr; }
}
