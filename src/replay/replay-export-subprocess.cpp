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

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] ========== SUBPROCESS START ==========\n");
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip=%s\n", clipPath);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] output=%s\n", outputPath);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] requested size=%dx%d\n", width, height);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] cwd=%s\n", fs::current_path().string().c_str());

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
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 1: Loading clip from %s\n", clipPath);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip file exists=%d size=%llu\n",
        (int)fs::exists(clipPath),
        fs::exists(clipPath) ? (unsigned long long)fs::file_size(clipPath) : 0ULL);
    if (!REPLAY_PLAYER.loadFromJSON(clipPath)) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: cannot load clip %s\n", clipPath);
        return;
    }
    const auto& hdr = REPLAY_PLAYER.header();
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip loaded OK: ticks=%u rate=%u map='%s'\n",
        hdr.tickCount, hdr.tickRate, hdr.mapName);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] clip sceneFrames=%zu soundEvents=%zu\n",
        REPLAY_PLAYER.totalTicks(), REPLAY_PLAYER.soundEvents().size());

    // ── 2. Load the world from the clip's map path ────────────────────
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 2: Loading world\n");
    ReplayClip tempClip;
    bool worldLoadOk = false;
    if (tempClip.load(clipPath) && !tempClip.mapPath.empty()) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] world map path='%s'\n", tempClip.mapPath.c_str());
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] world file exists=%d\n",
            (int)fs::exists(tempClip.mapPath));
        worldLoadOk = loadWorldFromGLB(THE_WORLD, tempClip.mapPath.c_str());
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] world load result=%d\n", (int)worldLoadOk);
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
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] tempClip.load=%d mapPath='%s'\n",
            (int)tempClip.load(clipPath), tempClip.mapPath.c_str());
    }

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 3: Waiting for weapon models\n");
    if (!waitForReplayWeaponModels(tempClip)) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: weapon model wait timed out\n");
        return;
    }
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] weapon models ready\n");

    // Always own the world state to block the default-map fallback
    // in engine-tick-state.cpp:160 (GAME_PLAYING && !WORLD_LOADED triggers it).
    WORLD_LOADED = true;
    if (gpActiveMapPath) {
        *gpActiveMapPath = tempClip.mapPath;
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] world state: WORLD_LOADED=1 activeMapPath='%s'\n",
            tempClip.mapPath.c_str());
    }

    // Set GAME_PLAYING AFTER map state is fully resolved so the render
    // pipeline does not start until the world is ready.
    if (gpGameState) *gpGameState = GAME_PLAYING;

    // ── 3. Setup editor (camera keyframes if present) ─────────────────
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 3: Setting up editor\n");
    gReplayEditor.load(clipPath);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] editor loaded=%d cameraKFs=%d modeKFs=%d timeKFs=%d\n",
        (int)gReplayEditor.isLoaded(),
        gReplayEditor.cameraKeyframeCount(),
        gReplayEditor.cameraModeKeyframeCount(),
        gReplayEditor.timeKeyframeCount());
    if (gReplayEditor.isLoaded() && gReplayEditor.cameraKeyframeCount() > 0) {
        gReplayEditor.freecam = true;
        REPLAY_PLAYER.cameraController().setMode("freecam");
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] editor loaded, freecam enabled (%d camera KFs)\n",
            gReplayEditor.cameraKeyframeCount());
    }

    // ── 4. Setup export job ───────────────────────────────────────────
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 4: Setting up export job\n");
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
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] job: totalTicks=%u capWidth=%d capHeight=%d\n",
        gJob.totalTicks, gJob.capWidth, gJob.capHeight);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] job: ffmpegPath='%s'\n", gJob.ffmpegPath.c_str());
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] job: outputPath='%s'\n", gJob.outputPath.c_str());

    // Create temp directories
    std::error_code ec;
    fs::create_directories(fs::path("replays") / "exports" / "_tmp", ec);

    // Create raw file for ffmpeg encoding path
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] creating raw temp file\n");
    gJob.rawTempPath = (fs::path("replays") / "exports" / "_tmp" / "subprocess_raw.rgb").string();
    gJob.rawFile = fopen(gJob.rawTempPath.c_str(), "wb");
    if (!gJob.rawFile) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] FAILED: cannot create raw file %s (errno=%d)\n",
            gJob.rawTempPath.c_str(), errno);
        return;
    }
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] raw file created: %s\n", gJob.rawTempPath.c_str());

    // Create the offscreen capture FBO
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] creating export FBO %dx%d\n", width, height);
    if (!replayExportTargetInit(width, height)) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] WARNING: export FBO unavailable, using window read\n");
    } else {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] export FBO created OK\n");
    }

    // ── 5. Begin replay playback ──────────────────────────────────────
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] STAGE 5: Beginning replay playback\n");
    REPLAY_PLAYER.beginPlayback();
    REPLAY_PLAYER.seekToTick(0);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] playback started: isPlaying=%d currentTick=%u totalTicks=%u\n",
        (int)REPLAY_PLAYER.isPlaying(), REPLAY_PLAYER.currentTick(), REPLAY_PLAYER.totalTicks());

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] ========== CAPTURE LOOP START ==========\n");
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] capture: %u ticks %dx%d pid=%lu\n",
        gJob.totalTicks, width, height, (unsigned long)GetCurrentProcessId());
    glfwSetWindowTitle(engine.window(), "MiMITA Replay Export - Recording...");

    // ── 6. Capture loop ───────────────────────────────────────────────
    // Each engineTick runs the full game loop (replay update, render, capture).
    // The capture happens inside updateReplayExport which reads pixels and
    // writes raw frames. Once all frames are captured, the state moves to
    // Encoding (ffmpeg subprocess) or Done.
    int loopIterations = 0;
    while (gJob.state == ReplayExportJob::Capturing ||
           gJob.state == ReplayExportJob::Encoding) {
        engineTick(engine);
        loopIterations++;

        // Log every 60 iterations (roughly once per second of export time)
        if (loopIterations % 60 == 0) {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT-SUBPROCESS] loop iteration=%d state=%d capturedTicks=%u exportTick=%.1f totalTicks=%u\n",
                loopIterations, (int)gJob.state, gJob.capturedTicks, gJob.exportTick, gJob.totalTicks);
        }

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
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] ========== CAPTURE LOOP END ==========\n");
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS] final state=%d capturedTicks=%u totalTicks=%u loopIterations=%d\n",
        (int)gJob.state, gJob.capturedTicks, gJob.totalTicks, loopIterations);
    replayExportTimingLogSummary();

    if (gJob.state == ReplayExportJob::Done) {
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] EXPORT COMPLETE: %s (%.1f MB)\n",
            gJob.outputPath.c_str(),
            (double)gJob.mp4FileBytes / (1024.0 * 1024.0));
        Debug::warn(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] outro=%s\n",
            gJob.mfOutroMissing ? "MISSING" : "OK");
        glfwSetWindowTitle(engine.window(),
            "MiMITA Replay Export - Complete!");
    } else if (gJob.state == ReplayExportJob::Failed) {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] EXPORT FAILED: %s\n", gJob.errorMsg.c_str());
        glfwSetWindowTitle(engine.window(),
            "MiMITA Replay Export - Failed");
    } else {
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SUBPROCESS] UNEXPECTED final state=%d\n", (int)gJob.state);
    }

    // Brief pause so user can see the result before window closes
    glfwPollEvents();
    glfwSwapBuffers(engine.window());
    Sleep(2000);

    // ── 8. Cleanup ────────────────────────────────────────────────────
    replayExportTargetDestroy();
    if (gJob.rawFile) { fclose(gJob.rawFile); gJob.rawFile = nullptr; }
}
