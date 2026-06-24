#include "terminal/replay-commands.h"
#include "terminal/terminal-state.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "replay/replay-export.h"
#include "replay/replay-factory.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "devtools/terminal.h"
#include "config/player-settings.h"

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

void registerReplayPlaybackCommands()
{
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
