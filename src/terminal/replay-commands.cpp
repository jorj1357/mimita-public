#include "replay-commands.h"
#include "terminal-state.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "render/lighting-config.h"

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
            REPLAY_RECORDER.registerAsset("texture:crosshair-ready", "texture", "assets/crosshair/crosshairready.png", {}, {}, "ui");
            REPLAY_RECORDER.registerAsset("texture:crosshair-delay", "texture", "assets/crosshair/crosshairdelay.png", {}, {}, "ui");
            REPLAY_RECORDER.registerAsset("texture:crosshair-reloading", "texture", "assets/crosshair/crosshairreloading.png", {}, {}, "ui");

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
            extern LightingConfig gLighting;
            replayLighting.directionalLight = gLighting.lightDir;
            replayLighting.ambientStrength = gLighting.ambientStrength;
            replayLighting.diffuseStrength = gLighting.diffuseStrength;
            replayLighting.edgeDarkness = gLighting.edgeDarkness;
            replayLighting.edgeWidth = gLighting.edgeWidth;
            replayLighting.aoDarkness = gLighting.aoDarkness;
            replayLighting.aoContrast = gLighting.aoContrast;
            replayLighting.textureContrast = gLighting.textureContrast;
            replayLighting.textureBrightness = gLighting.textureBrightness;
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
        }
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
}
