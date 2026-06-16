#include "replay-commands.h"
#include "terminal-state.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "replay/replay-export.h"
#include "replay/replay-factory.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "render/lighting-config.h"
#include "debug/debug-log.h"

#define CMDTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)

static void playReplayByPath(const std::string& path) {
    if (!REPLAY_PLAYER.loadFromJSON(path)) {
        Terminal::instance().addLog("[ERROR] failed to load replay: " + path);
        return;
    }
    REPLAY_PLAYER.preloadAssets();
    REPLAY_PLAYER.beginPlayback();

    {
        uint32_t tickCount = REPLAY_PLAYER.totalTicks();
        float duration = (float)tickCount / 60.0f;
        char buf[128];
        snprintf(buf, sizeof(buf), "[REPLAY] Frames: %u", tickCount);
        Terminal::instance().addLog(buf);
        snprintf(buf, sizeof(buf), "[REPLAY] Tick Rate: 60");
        Terminal::instance().addLog(buf);
        snprintf(buf, sizeof(buf), "[REPLAY] Duration: %.1f sec", duration);
        Terminal::instance().addLog(buf);
        printf("[REPLAY] loaded %s  frames=%u  duration=%.1fs\n",
               path.c_str(), tickCount, duration);
    }

    {
        ReplayClip timelineClip;
        if (timelineClip.load(path)) {
            REPLAY_TIMELINE.setFrames(timelineClip.sceneFrames, timelineClip.soundEvents);
        }
    }

    GAME_STATE = GAME_PLAYING;
    printf("[REPLAY] playing %s\n", path.c_str());
    Terminal::instance().addLog("[REPLAY] playing " + path);
}

void registerReplayCommands()
{
    Terminal::instance().registerCommand({
        "replay.record", "Start replay recording", "replay.record",
        [](const std::vector<std::string>&) {
            if (REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("[REPLAY] Already recording");
                return;
            }
            if (ACTIVE_MAP_PATH.empty()) {
                Terminal::instance().addLog("[REPLAY] No active map is loaded");
                return;
            }
            REPLAY_RECORDER.beginRecording(0.0f, "mimita");

            const std::string mapPath = ACTIVE_MAP_PATH;
            const std::string playerPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
            const std::string revolverPath = "assets/objects/weapons/mimita-revolver-v1.glb";
            REPLAY_RECORDER.registerAsset("map:active", "map_glb", mapPath, {}, "basic", "world");
            REPLAY_RECORDER.registerAsset("model:player", "actor_glb", playerPath, {}, "basic", "player");
            REPLAY_RECORDER.registerAsset("model:revolver", "weapon_glb", revolverPath, {}, "basic", "weapon");
            const std::string shotgunPath = "assets/objects/weapons/mimita-shotgun-v1.glb";
            REPLAY_RECORDER.registerAsset("model:shotgun", "weapon_glb", shotgunPath, {}, "basic", "weapon");
            REPLAY_RECORDER.registerAsset("texture:outfit", "texture", GetPlayerSettings().outfitPath, {}, {}, "outfit");
            ReplayWorldMetadata replayWorld;
            replayWorld.mapAssetId = "map:active";
            replayWorld.mapPath = mapPath;
            for (const Mesh::Batch& batch : THE_WORLD.mesh.batches) {
                const std::string materialName = batch.materialName.empty() ? "default" : batch.materialName;
                bool alreadyRegistered = false;
                for (const ReplayMaterialReference& material : replayWorld.materials) {
                    if (material.materialName == materialName) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                if (!alreadyRegistered)
                    replayWorld.materials.push_back({materialName, "", "basic"});
            }
            REPLAY_RECORDER.setWorldMetadata(replayWorld);

            ReplayLightingState replayLighting;
            auto& lc = LightingConfig::instance();
            replayLighting.directionalLight = lc.lightDir();
            replayLighting.ambientStrength = lc.ambientStrength();
            replayLighting.diffuseStrength = lc.diffuseStrength();
            replayLighting.edgeDarkness = lc.edgeDarkness();
            replayLighting.edgeWidth = lc.edgeWidth();
            replayLighting.aoDarkness = lc.aoDarkness();
            replayLighting.aoContrast = lc.aoContrast();
            replayLighting.textureContrast = lc.textureContrast();
            replayLighting.textureBrightness = lc.textureBrightness();
            REPLAY_RECORDER.setLighting(replayLighting);

            Terminal::instance().addLog("[REPLAY] Recording started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.stop", "Stop replay recording or playback", "replay.stop",
        [](const std::vector<std::string>&) {
            if (REPLAY_RECORDER.isRecording()) {
                REPLAY_RECORDER.stopRecording();
                const std::string path = generateReplayExportPath();
                const bool exported = REPLAY_RECORDER.exportToJSON(path);
                Terminal::instance().addLog(
                    exported
                        ? "[REPLAY] Recording stopped and saved to " + path
                        : "[ERROR] Replay stopped but export failed: " + path
                );
            }
            if (REPLAY_PLAYER.isPlaying()) {
                REPLAY_PLAYER.stopPlayback();
                Terminal::instance().addLog("[REPLAY] Playback stopped");
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay.export", "Export replay to file", "replay.export <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: replay.export <path>");
                return;
            }
            std::string path = args[0];
            if (path.find('.') == std::string::npos)
                path += ".json";
            const bool exported = REPLAY_RECORDER.exportToJSON(path);
            Terminal::instance().addLog(
                exported ? "[REPLAY] Exported to " + path
                         : "[ERROR] Failed to export replay to " + path
            );
        }
    });

    Terminal::instance().registerCommand({
        "replay.load", "Load replay file", "replay.load <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: replay.load <path>");
                return;
            }
            std::string path = args[0];
            bool ok = REPLAY_PLAYER.loadFromJSON(path);
            Terminal::instance().addLog(ok ? "[REPLAY] Loaded " + path : "[ERROR] Failed to load " + path);
        }
    });

    Terminal::instance().registerCommand({
        "replay.play", "Start replay playback", "replay.play",
        [](const std::vector<std::string>&) {
            if (REPLAY_PLAYER.totalTicks() == 0) {
                Terminal::instance().addLog("[ERROR] No replay loaded");
                return;
            }
            REPLAY_PLAYER.preloadAssets();
            REPLAY_PLAYER.beginPlayback();
            GAME_STATE = GAME_PLAYING;
            Terminal::instance().addLog("[REPLAY] Playback started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.info", "Show replay info", "replay.info",
        [](const std::vector<std::string>&) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Recording: %d  Playback: %d  Ticks: %u",
                     (int)REPLAY_RECORDER.isRecording(), (int)REPLAY_PLAYER.isPlaying(),
                     REPLAY_PLAYER.totalTicks());
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "replay_list", "List saved replays newest first (optionally with index)", "replay.list",
        [](const std::vector<std::string>&) {
            REPLAY_CLIPS_CACHE = listReplayClips();
            if (REPLAY_CLIPS_CACHE.empty()) {
                Terminal::instance().addLog("[REPLAY] no saved replays");
                return;
            }
            for (size_t i = 0; i < REPLAY_CLIPS_CACHE.size(); ++i) {
                char buf[512];
                snprintf(buf, sizeof(buf), "[REPLAY] %zu. %s", i + 1,
                         REPLAY_CLIPS_CACHE[i].c_str());
                Terminal::instance().addLog(buf);
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_browser", "Toggle replay browser overlay", "replay_browser",
        [](const std::vector<std::string>&) {
            REPLAY_BROWSER.toggle();
            if (REPLAY_BROWSER.isOpen())
                REPLAY_BROWSER.refresh();
        }
    });

    Terminal::instance().registerCommand({
        "replay.play", "Play a replay by index from replay.list, or newest if no arg",
        "replay.play [index]",
        [](const std::vector<std::string>& args) {
            if (REPLAY_CLIPS_CACHE.empty())
                REPLAY_CLIPS_CACHE = listReplayClips();
            if (REPLAY_CLIPS_CACHE.empty()) {
                Terminal::instance().addLog("[ERROR] no replays found");
                return;
            }
            size_t index = 0;
            if (!args.empty()) {
                char* end = nullptr;
                long parsed = std::strtol(args[0].c_str(), &end, 10);
                if (end == args[0].c_str() || parsed < 1) {
                    Terminal::instance().addLog("[ERROR] invalid index, use replay.list first");
                    return;
                }
                index = (size_t)(parsed - 1);
            }
            if (index >= REPLAY_CLIPS_CACHE.size()) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[ERROR] index %zu out of range (max %zu)",
                         index + 1, REPLAY_CLIPS_CACHE.size());
                Terminal::instance().addLog(buf);
                return;
            }
            playReplayByPath(REPLAY_CLIPS_CACHE[index]);
        }
    });

    Terminal::instance().registerCommand({
        "replay_save_last_kill", "Save five seconds before and three seconds after the last kill",
        "replay_save_last_kill",
        [](const std::vector<std::string>&) {
            std::string factoryPath;
            if (REPLAY_FACTORY.saveLastKill(&factoryPath)) {
                Terminal::instance().addLog("[REPLAY] saved clip " + factoryPath);
                return;
            }
            std::string path;
            if (!REPLAY_CLIP_SAVER.saveLastKill(&path)) {
                Terminal::instance().addLog(
                    "[ERROR] no captured kill is available to save");
                return;
            }
            Terminal::instance().addLog(
                path == "pending post-kill capture"
                    ? "[REPLAY] clip queued; capturing three seconds after kill"
                    : "[REPLAY] saved clip " + path);
        }
    });

    Terminal::instance().registerCommand({
        "replay_save_instant", "Save the last ~60 seconds as an instant replay file",
        "replay_save_instant",
        [](const std::vector<std::string>&) {
            if (!REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("[ERROR] No replay recording active");
                return;
            }
            std::string path = generateInstantReplayPath();
            if (!REPLAY_RECORDER.exportToJSON(path)) {
                Terminal::instance().addLog("[ERROR] Failed to save instant replay");
                return;
            }
            size_t frameCount = REPLAY_RECORDER.frames().size();
            size_t sceneCount = REPLAY_RECORDER.sceneFrames().size();
            float duration = (float)frameCount / 60.0f;
            char buf[128];
            Terminal::instance().addLog("[REPLAY] Saved instant replay");
            snprintf(buf, sizeof(buf), "[REPLAY] Frames saved: %zu", frameCount);
            Terminal::instance().addLog(buf);
            snprintf(buf, sizeof(buf), "[REPLAY] Scene frames: %zu", sceneCount);
            Terminal::instance().addLog(buf);
            snprintf(buf, sizeof(buf), "[REPLAY] Duration: %.1f sec", duration);
            Terminal::instance().addLog(buf);
            Terminal::instance().addLog("[REPLAY] File: " + path);
            printf("[REPLAY] Saved instant replay: %s  frames=%zu  duration=%.1fs\n",
                   path.c_str(), frameCount, duration);
        },
        std::string(), CommandCategory::Uncategorized, {"rpls"}
    });

    Terminal::instance().registerCommand({
        "replay_watch_instant", "Load and watch the most recent instant replay",
        "replay_watch_instant",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[REPLAY] Loading latest replay...");
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            playReplayByPath(clips.front());
            REPLAY_PLAYER.pause();
            char buf[128];
            snprintf(buf, sizeof(buf), "[REPLAY] Loaded replay  Frames: %u  Duration: %.1f sec",
                     REPLAY_PLAYER.totalTicks(),
                     (float)REPLAY_PLAYER.totalTicks() / 60.0f);
            Terminal::instance().addLog(buf);
            printf("[REPLAY] Loaded replay: %s  ticks=%u\n",
                   clips.front().c_str(), REPLAY_PLAYER.totalTicks());
        }
    });

    Terminal::instance().registerCommand({
        "replay_stop", "Stop in-engine replay playback", "replay_stop",
        [](const std::vector<std::string>&) {
            REPLAY_PLAYER.stopPlayback();
            Terminal::instance().addLog("[REPLAY] playback stopped");
        }
    });

    Terminal::instance().registerCommand({
        "replay_pause", "Pause in-engine replay playback", "replay_pause",
        [](const std::vector<std::string>&) {
            REPLAY_PLAYER.pause();
            Terminal::instance().addLog("[REPLAY] paused");
        }
    });

    Terminal::instance().registerCommand({
        "replay_resume", "Resume in-engine replay playback", "replay_resume",
        [](const std::vector<std::string>&) {
            REPLAY_PLAYER.resume();
            Terminal::instance().addLog("[REPLAY] resumed");
        }
    });

    Terminal::instance().registerCommand({
        "replay_timescale", "Set replay playback speed", "replay_timescale <float>",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                return;
            REPLAY_PLAYER.setTimescale(std::stof(args[0]));
            printf("[REPLAY] timescale %.2f\n", REPLAY_PLAYER.timescale());
            Terminal::instance().addLog(
                "[REPLAY] timescale " + std::to_string(REPLAY_PLAYER.timescale()));
        }
    });

    Terminal::instance().registerCommand({
        "replay_fov", "Override replay camera FOV", "replay_fov <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                return;
            REPLAY_PLAYER.cameraController().setFov(std::stof(args[0]));
            Terminal::instance().addLog(
                "[REPLAY] fov " +
                std::to_string(REPLAY_PLAYER.cameraController().fov()));
        }
    });

    Terminal::instance().registerCommand({
        "replay_camera", "Set replay camera mode: fp/victim/orbit/freecam", "replay_camera <mode>",
        [](const std::vector<std::string>& args) {
            if (args.empty() ||
                !REPLAY_PLAYER.cameraController().setMode(args[0])) {
                Terminal::instance().addLog(
                    "[ERROR] Usage: replay_camera <fp|victim|orbit|freecam>");
                return;
            }
            printf("[REPLAY] camera mode %s\n",
                   REPLAY_PLAYER.cameraController().modeName());
            Terminal::instance().addLog(
                std::string("[REPLAY] camera mode ") +
                REPLAY_PLAYER.cameraController().modeName());
        }
    });

    Terminal::instance().registerCommand({
        "replay_freecam", "Enable or disable replay freecam", "replay_freecam <0|1>",
        [](const std::vector<std::string>& args) {
            const bool enabled = !args.empty() && args[0] != "0";
            REPLAY_PLAYER.cameraController().setMode(enabled ? "freecam" : "fp");
            Terminal::instance().addLog(
                enabled ? "[REPLAY] camera mode freecam"
                        : "[REPLAY] camera mode fp");
        }
    });

    Terminal::instance().registerCommand({
        "replay_orbit", "Enable or disable replay orbit camera", "replay_orbit <0|1>",
        [](const std::vector<std::string>& args) {
            const bool enabled = !args.empty() && args[0] != "0";
            REPLAY_PLAYER.cameraController().setMode(enabled ? "orbit" : "fp");
            Terminal::instance().addLog(
                enabled ? "[REPLAY] camera mode orbit"
                        : "[REPLAY] camera mode fp");
        }
    });

    Terminal::instance().registerCommand({
        "rpl_load_newest", "Find and play the newest replay file", "rpl_load_newest",
        [](const std::vector<std::string>&) {
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] no replays found");
                return;
            }
            playReplayByPath(clips.front());
        }
    });

    Terminal::instance().registerCommand({
        "replay_toggle_pause", "Toggle replay pause", "replay_toggle_pause",
        [](const std::vector<std::string>&) {
            if (REPLAY_PLAYER.isPaused()) REPLAY_PLAYER.resume();
            else REPLAY_PLAYER.pause();
            printf("[REPLAY] %s\n", REPLAY_PLAYER.isPaused() ? "paused" : "resumed");
        }
    });

    Terminal::instance().registerCommand({
        "replay_seek_tick", "Seek to a specific tick", "replay_seek_tick <tick>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) return;
            int tick = std::stoi(args[0]);
            REPLAY_PLAYER.seekToTick((uint32_t)std::max(0, tick));
            printf("[REPLAY] seeked to tick %d\n", tick);
            Terminal::instance().addLog("[REPLAY] seeked to tick " + std::to_string(tick));
        }
    });

    Terminal::instance().registerCommand({
        "replay_seek_percent", "Seek to a percentage of the replay", "replay_seek_percent <0-100>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) return;
            float pct = std::stof(args[0]) / 100.0f;
            uint32_t tick = (uint32_t)(pct * REPLAY_PLAYER.totalTicks());
            REPLAY_PLAYER.seekToTick(tick);
            printf("[REPLAY] seeked to %.0f%% (tick %u)\n", pct * 100.0f, tick);
            Terminal::instance().addLog("[REPLAY] seeked to " + std::to_string(int(pct * 100.0f)) + "% (tick " + std::to_string(tick) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "replay_rewind_1s", "Rewind replay by 1 second (60 ticks)", "replay_rewind_1s",
        [](const std::vector<std::string>&) {
            uint32_t tick = REPLAY_PLAYER.currentTick();
            uint32_t newTick = tick > 60 ? tick - 60 : 0;
            REPLAY_PLAYER.seekToTick(newTick);
            printf("[REPLAY] rewound 1s to tick %u\n", newTick);
            Terminal::instance().addLog("[REPLAY] rewound to tick " + std::to_string(newTick));
        }
    });

    Terminal::instance().registerCommand({
        "replay_forward_1s", "Skip replay forward by 1 second (60 ticks)", "replay_forward_1s",
        [](const std::vector<std::string>&) {
            uint32_t tick = REPLAY_PLAYER.currentTick();
            uint32_t totalTicks = REPLAY_PLAYER.totalTicks();
            uint32_t newTick = std::min(tick + 60, totalTicks);
            REPLAY_PLAYER.seekToTick(newTick);
            printf("[REPLAY] skipped 1s to tick %u\n", newTick);
            Terminal::instance().addLog("[REPLAY] skipped to tick " + std::to_string(newTick));
        }
    });

    Terminal::instance().registerCommand({
        "export_debug_mode", "Toggle ffmpeg visible cmd window debug mode (on/off)", "export_debug_mode [on|off]",
        [](const std::vector<std::string>& args) {
            CMDTRACE("export_debug_mode command ENTERED");
            if (args.empty()) {
                bool current = isFfmpegDebugMode();
                setFfmpegDebugMode(!current);
            } else {
                setFfmpegDebugMode(args[0] == "on" || args[0] == "1");
            }
            Terminal::instance().addLog(
                std::string("[FFMPEG DEBUG] ") + (isFfmpegDebugMode() ? "ON (visible cmd window)" : "OFF (_popen)"));
        }
    });

    Terminal::instance().registerCommand({
        "export_test_ffmpeg", "Test ffmpeg by running 'ffmpeg -version' in a visible cmd window", "export_test_ffmpeg",
        [](const std::vector<std::string>&) {
            CMDTRACE("export_test_ffmpeg command ENTERED");
            std::string ffmpeg = defaultFfmpegPath();
            CMDTRACE("ffmpeg path=%s", ffmpeg.c_str());
            if (!std::filesystem::exists(ffmpeg)) {
                CMDTRACE("ffmpeg NOT FOUND at: %s", ffmpeg.c_str());
                Terminal::instance().addLog("[ERROR] ffmpeg not found at: " + ffmpeg);
                return;
            }
            CMDTRACE("ffmpeg exists OK");
            Terminal::instance().addLog("[FFMPEG TEST] launching in visible window: " + ffmpeg);
            std::string cmd = "\"" + ffmpeg + "\" -version";
            std::string launchArgs = makeCmdKArgs(cmd);
            CMDTRACE("ShellExecuteA params EXACT=%s %s", "cmd.exe", launchArgs.c_str());
            CMDTRACE("Calling ShellExecuteA...");
            HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", launchArgs.c_str(), NULL, SW_SHOWNORMAL);
            INT_PTR result = (INT_PTR)h;
            if (result <= 32) {
                DWORD err = GetLastError();
                CMDTRACE("ShellExecuteA FAILED GetLastError=%lu", (unsigned long)err);
                Terminal::instance().addLog("[ERROR] ShellExecuteA failed with code " + std::to_string(err));
            } else {
                CMDTRACE("ShellExecuteA SUCCESS (cmd window should be open)");
            }
        }
    });

    Terminal::instance().registerCommand({
        "export_test_exact", "Run the exact export ffmpeg command in a visible cmd window (replaces stdin with testsrc)",
        "export_test_exact",
        [](const std::vector<std::string>&) {
            CMDTRACE("export_test_exact command ENTERED");
            std::string ffmpeg = defaultFfmpegPath();
            if (!std::filesystem::exists(ffmpeg)) {
                CMDTRACE("ffmpeg NOT FOUND");
                return;
            }
            std::string outputPath = generateExportOutputPath();
            std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
            std::error_code ec;
            std::filesystem::create_directories(outDir, ec);
            std::string nativeOutput = std::filesystem::path(outputPath).make_preferred().string();
            std::string cmd = "\"" + ffmpeg + "\" -y -f lavfi -i testsrc=duration=2:size=1280x720:rate=60 "
                "-c:v libx264 -preset fast -pix_fmt yuv420p -crf 18"
                " \"" + nativeOutput + "\"";
            CMDTRACE("command: %s", cmd.c_str());
            std::string launchArgs = makeCmdKArgs(cmd);
            CMDTRACE("ShellExecuteA params EXACT=%s %s", "cmd.exe", launchArgs.c_str());
            HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", launchArgs.c_str(), NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h <= 32) {
                DWORD err = GetLastError();
                CMDTRACE("ShellExecuteA FAILED GetLastError=%lu", (unsigned long)err);
            } else {
                CMDTRACE("ShellExecuteA SUCCESS");
            }
        }
    });

    Terminal::instance().registerCommand({
        "export_test_exact_pipe", "Run the exact export ffmpeg command WITH -i - (stdin pipe) in a visible cmd window",
        "export_test_exact_pipe",
        [](const std::vector<std::string>&) {
            CMDTRACE("export_test_exact_pipe command ENTERED");
            std::string ffmpeg = defaultFfmpegPath();
            if (!std::filesystem::exists(ffmpeg)) {
                CMDTRACE("ffmpeg NOT FOUND");
                return;
            }
            std::string outputPath = generateExportOutputPath();
            std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
            std::error_code ec;
            std::filesystem::create_directories(outDir, ec);
            std::string nativeOutput = std::filesystem::path(outputPath).make_preferred().string();
            std::string cmd = "\"" + ffmpeg + "\" -y -f rawvideo -pixel_format rgb24 "
                "-video_size 1280x720 -framerate 60 -i - -c:v libx264 -preset fast -pix_fmt yuv420p "
                "-crf 18 \"" + nativeOutput + "\"";
            CMDTRACE("EXACT EXPORT COMMAND: %s", cmd.c_str());
            CMDTRACE("output path: %s", nativeOutput.c_str());
            std::string launchArgs = makeCmdKArgs(cmd);
            CMDTRACE("ShellExecuteA params EXACT=%s %s", "cmd.exe", launchArgs.c_str());
            HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", launchArgs.c_str(), NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h <= 32) {
                DWORD err = GetLastError();
                CMDTRACE("ShellExecuteA FAILED GetLastError=%lu", (unsigned long)err);
            } else {
                CMDTRACE("ShellExecuteA SUCCESS - cmd window shows ffmpeg waiting for stdin");
            }
        }
    });

    Terminal::instance().registerCommand({
        "export_test_output", "Test ffmpeg by generating a test MP4 in the export directory", "export_test_output",
        [](const std::vector<std::string>&) {
            CMDTRACE("export_test_output command ENTERED");
            std::string ffmpeg = defaultFfmpegPath();
            CMDTRACE("ffmpeg path=%s", ffmpeg.c_str());
            if (!std::filesystem::exists(ffmpeg)) {
                CMDTRACE("ffmpeg NOT FOUND at: %s", ffmpeg.c_str());
                Terminal::instance().addLog("[ERROR] ffmpeg not found at: " + ffmpeg);
                return;
            }
            CMDTRACE("ffmpeg exists OK");
            std::string outputPath = generateExportOutputPath();
            CMDTRACE("outputPath=%s", outputPath.c_str());
            std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
            std::error_code ec;
            std::filesystem::create_directories(outDir, ec);
            if (ec) {
                CMDTRACE("cannot create output dir: %s", ec.message().c_str());
                Terminal::instance().addLog("[ERROR] cannot create output dir: " + outDir.string());
                return;
            }
            CMDTRACE("output dir created OK");
            std::string nativeOutput = std::filesystem::path(outputPath).make_preferred().string();
            std::string cmd = "\"" + ffmpeg + "\" -f lavfi -i testsrc=duration=1:size=1280x720:rate=60 -pix_fmt yuv420p \"" + nativeOutput + "\"";
            CMDTRACE("ffmpeg command: %s", cmd.c_str());
            Terminal::instance().addLog("[FFMPEG TEST OUTPUT] command: " + cmd);
            Terminal::instance().addLog("[FFMPEG TEST OUTPUT] output: " + nativeOutput);
            std::string launchArgs = makeCmdKArgs(cmd);
            CMDTRACE("ShellExecuteA params EXACT=%s %s", "cmd.exe", launchArgs.c_str());
            HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", launchArgs.c_str(), NULL, SW_SHOWNORMAL);
            INT_PTR result = (INT_PTR)h;
            if (result <= 32) {
                DWORD err = GetLastError();
                CMDTRACE("ShellExecuteA FAILED GetLastError=%lu", (unsigned long)err);
                Terminal::instance().addLog("[ERROR] ShellExecuteA failed with code " + std::to_string(err));
            } else {
                CMDTRACE("ShellExecuteA SUCCESS (cmd window should be open)");
            }
        }
    });

    Terminal::instance().registerCommand({
        "export_diagnose", "Run replay export diagnostics, write report to replays/exports/",
        "export_diagnose",
        [](const std::vector<std::string>&) {
            CMDTRACE("export_diagnose command ENTERED");
            std::string logPath = "replays/exports/export-debug.log";
            std::error_code ec;
            std::filesystem::create_directories("replays/exports", ec);
            FILE* log = fopen(logPath.c_str(), "w");
            if (!log) {
                Terminal::instance().addLog("[ERROR] Cannot write log to " + logPath);
                return;
            }
            auto logLine = [log](const char* fmt, ...) {
                va_list args;
                va_start(args, fmt);
                vfprintf(log, fmt, args);
                fprintf(log, "\n");
                va_end(args);
                fflush(log);
            };

            logLine("=== EXPORT DIAGNOSTIC LOG ===");
            logLine("Timestamp: %lld", (long long)std::time(nullptr));

            // 1. Find newest replay
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                logLine("FAIL: No replays found");
                CMDTRACE("FAIL: No replays found");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: no replays found");
                return;
            }
            std::string path = clips.front();
            logLine("Newest replay: %s", path.c_str());

            // 2. Try loading
            ReplayClip clip;
            if (!clip.load(path)) {
                logLine("FAIL: Cannot load clip");
                CMDTRACE("FAIL: Cannot load clip");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: cannot load clip");
                return;
            }
            logLine("Clip loaded: ticks=%u duration=%.1f", clip.header.tickCount, (float)clip.header.tickCount / 60.0f);

            // 3. Check ffmpeg
            std::string ffmpeg = defaultFfmpegPath();
            if (!std::filesystem::exists(ffmpeg)) {
                logLine("FAIL: ffmpeg not found at %s", ffmpeg.c_str());
                CMDTRACE("FAIL: ffmpeg not found");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: ffmpeg not found");
                return;
            }
            logLine("ffmpeg found: %s", ffmpeg.c_str());

            // 4. Try loading into ReplayPlayer
            ReplayPlayer testPlayer;
            if (!testPlayer.loadFromJSON(path)) {
                logLine("FAIL: ReplayPlayer.loadFromJSON failed");
                CMDTRACE("FAIL: ReplayPlayer.loadFromJSON failed");
            } else {
                logLine("ReplayPlayer loaded: totalTicks=%u", testPlayer.totalTicks());
                testPlayer.beginPlayback();
                logLine("Playback started: isPlaying=%d", (int)testPlayer.isPlaying());
                testPlayer.seekToTick(0);
                const ReplaySceneFrame* frame = testPlayer.currentSceneFrame();
                logLine("seekToTick(0): hasFrame=%d actors=%zu",
                        frame ? 1 : 0, frame ? frame->actors.size() : 0);
            }

            // 5. Start actual export
            CMDTRACE("Starting actual export...");
            logLine("Calling startReplayExport...");
            if (!startReplayExport(path, 1280, 720)) {
                logLine("FAIL: startReplayExport returned false");
                CMDTRACE("FAIL: startReplayExport returned false");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: startReplayExport failed");
                return;
            }
            logLine("startReplayExport OK, state=Capturing");

            // 6. Render a few frames
            logLine("Rendering 10 test frames...");
            for (int i = 0; i < 10; i++) {
                const ReplayExportJob& job = getReplayExportJob();
                logLine("Frame %d: state=%d capturedTicks=%u totalTicks=%u",
                        i, (int)job.state, job.capturedTicks, job.totalTicks);
                if (job.state != ReplayExportJob::Capturing)
                    break;
                // updateReplayExport will be called by main loop
                // We can't easily force it here, so just log the state
            }
            logLine("Export state: %d", (int)getReplayExportJob().state);

            fclose(log);
            CMDTRACE("Diagnostic log written to %s", logPath.c_str());
            Terminal::instance().addLog("[DIAGNOSE] Log written to " + logPath);
        }
    });

    Terminal::instance().registerCommand({
        "replay_export_mp4", "Open replay picker, or export path directly", "replay_export_mp4 [path]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[REPLAY] Scanning replays...");
                Terminal::instance().startExportPicker();
                return;
            }
            std::string path = args[0];
            if (!std::filesystem::exists(path)) {
                Terminal::instance().addLog("[ERROR] File not found: " + path);
                return;
            }
            if (startReplayExport(path, 1280, 720)) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_export_latest", "Export the newest replay to MP4", "replay_export_latest",
        [](const std::vector<std::string>&) {
            CMDTRACE("replay_export_latest ENTERED");
            std::vector<std::string> clips = listReplayClips();
            CMDTRACE("listReplayClips returned %zu clips", clips.size());
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            std::string path = clips.front();
            CMDTRACE("selected newest replay: %s", path.c_str());
            CMDTRACE("calling startReplayExport(\"%s\", 1280, 720)", path.c_str());
            bool result = startReplayExport(path, 1280, 720);
            CMDTRACE("startReplayExport returned %d", (int)result);
            if (result) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
            }
        },
        std::string(), CommandCategory::Uncategorized, {"rplx"}
    });

    Terminal::instance().registerCommand({
        "replay_export_last_duel", "Export the most recent duel replay to MP4", "replay_export_last_duel",
        [](const std::vector<std::string>&) {
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            // Filter for duel-related files (contain duel in the clip path or are mclip files from duel)
            std::string* found = nullptr;
            for (auto& c : clips) {
                if (c.find("duel") != std::string::npos ||
                    c.find("mclip") != std::string::npos) {
                    found = &c;
                    break;
                }
            }
            if (!found) found = &clips.front();
            std::string path = *found;
            Terminal::instance().addLog("[REPLAY EXPORT] exporting last duel: " + path);
            if (startReplayExport(path, 1280, 720)) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_export_finalkill", "Export the most recent final kill replay to MP4", "replay_export_finalkill",
        [](const std::vector<std::string>&) {
            std::vector<ReplayClipInfo> clips = scanSavedClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No clips found");
                return;
            }
            // Use the first (newest) clip
            std::string path = clips.front().path;
            Terminal::instance().addLog("[REPLAY EXPORT] exporting final kill: " + path);
            if (startReplayExport(path, 1280, 720)) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_exit", "Safe exit from replay, clear all replay resources", "replay_exit",
        [](const std::vector<std::string>&) {
            printf("[REPLAY] replay_exit called\n");
            if (REPLAY_PLAYER.isPlaying()) {
                REPLAY_PLAYER.stopPlayback();
                printf("[REPLAY] playback stopped\n");
            }
            if (!REPLAY_ACTOR_MODELS.empty()) {
                REPLAY_ACTOR_MODELS.clear();
                printf("[REPLAY] actor models cleared\n");
            }
            if (!REPLAY_WEAPON_MODELS.empty()) {
                REPLAY_WEAPON_MODELS.clear();
                printf("[REPLAY] weapon models cleared\n");
            }
            if (!REPLAY_CHAT_STATES.empty()) {
                REPLAY_CHAT_STATES.clear();
                printf("[REPLAY] chat states cleared\n");
            }
            Terminal::instance().addLog("[REPLAY] all replay resources cleaned");
        }
    });

    Terminal::instance().registerCommand({
        "replay_state", "Print current replay state info", "replay_state",
        [](const std::vector<std::string>&) {
            char buf[512];
            const ReplayPlayer& p = REPLAY_PLAYER;
            snprintf(buf, sizeof(buf),
                "ReplayState: %s\nGameState: %s\nLoaded: %s\nFrame: %u/%u\n"
                "Playing: %s Paused: %s Duration: %.1fs\nActors: %zu Weapons: %zu",
                p.isPlaying() ? "WatchingReplay" : GAME_STATE == GAME_MENU ? "Menu" : "None",
                GAME_STATE == GAME_PLAYING ? "PLAYING" : GAME_STATE == GAME_MENU ? "MENU" : "PAUSED",
                p.totalTicks() > 0 ? "yes" : "no",
                p.currentTick(), p.totalTicks(),
                p.isPlaying() ? "yes" : "no",
                p.isPaused() ? "yes" : "no",
                p.totalTicks() > 0 ? (float)p.totalTicks() / 60.0f : 0.0f,
                (unsigned long)REPLAY_ACTOR_MODELS.size(),
                (unsigned long)REPLAY_WEAPON_MODELS.size());
            Terminal::instance().addLog(buf);
            printf("[REPLAY STATE] %s\n", buf);
        }
    });

    Terminal::instance().registerCommand({
        "replay_debug", "Print all replay resource debug info", "replay_debug",
        [](const std::vector<std::string>&) {
            const ReplayPlayer& p = REPLAY_PLAYER;
            printf("[REPLAY DEBUG] isPlaying=%d isPaused=%d currentTick=%u totalTicks=%u timescale=%.2f\n",
                   (int)p.isPlaying(), (int)p.isPaused(),
                   p.currentTick(), p.totalTicks(), p.timescale());
            printf("[REPLAY DEBUG] actorModels=%zu weaponModels=%zu chatStates=%zu\n",
                   REPLAY_ACTOR_MODELS.size(), REPLAY_WEAPON_MODELS.size(),
                   REPLAY_CHAT_STATES.size());
            printf("[REPLAY DEBUG] recording=%d clipSaver.hasLastKill=%d\n",
                   (int)REPLAY_RECORDER.isRecording(),
                   (int)REPLAY_CLIP_SAVER.hasLastKill());
            printf("[REPLAY DEBUG] replayExportActive=%d\n",
                   (int)isReplayExportActive());
            Terminal::instance().addLog("[REPLAY] debug info printed to console");
        }
    });

    Terminal::instance().registerCommand({
        "healthbar_audit", "Audit healthbar state: count alive/dead/orphaned entries", "healthbar_audit",
        [](const std::vector<std::string>&) {
            uint32_t total = 0, alive = 0, dead = 0;
            for (const auto& kv : REPLAY_ACTOR_MODELS) {
                total++;
                if (kv.second && kv.second->dead) dead++;
                else alive++;
            }
            printf("[HEALTHBAR AUDIT]\n");
            printf("  replayActorModels=%u\n", total);
            printf("  alive=%u\n", alive);
            printf("  dead=%u\n", dead);
            if (const ReplaySceneFrame* frame = REPLAY_PLAYER.currentSceneFrame()) {
                printf("  sceneFrameActors=%zu\n", frame->actors.size());
                size_t frameAlive = 0, frameDead = 0;
                for (const auto& a : frame->actors) {
                    if (a.dead) frameDead++;
                    else frameAlive++;
                }
                printf("  frameAlive=%zu frameDead=%zu\n", frameAlive, frameDead);
            } else {
                printf("  sceneFrameActors=0 (no current frame)\n");
            }
            printf("  replayExportActive=%d\n", (int)isReplayExportActive());
            Terminal::instance().addLog("[HEALTHBAR] audit printed to console");
        }
    });

    Terminal::instance().registerCommand({
        "replay_hitmarker_reload", "Reload config/audio/replay-hitmarkers.json",
        "replay_hitmarker_reload",
        [](const std::vector<std::string>&) {
            pollReplayHitmarkerConfig();
            Terminal::instance().addLog("[REPLAY HITMARKER] config reloaded");
        }
    });
}
