#include "replay-editor.h"
#include "replay.h"
#include "devtools/terminal.h"
#include "camera.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

extern float CAMERA_SENS;
#include "config/player-settings.h"

// ── Helpers ─────────────────────────────────────────────────

static ReplayEditor& E = gReplayEditor;

static void requireEditor(const char* cmd) {
    if (!E.isLoaded()) {
        Terminal::instance().addLog("[ERROR] No replay editor loaded. Use rple or rpleload first.");
    }
}

static glm::quat eulerToQuat(float yawDeg, float pitchDeg) {
    // Build front from yaw/pitch using gameplay camera formula,
    // then use quatLookAt — consistent with K-key keyframes and
    // the interpolation extraction rot * (0,0,-1).
    glm::vec3 front;
    front.x = std::cos(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    front.y = std::sin(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    front.z = std::sin(glm::radians(pitchDeg));
    return glm::quatLookAt(glm::normalize(front), glm::vec3(0,0,1));
}

// ── Command implementations ─────────────────────────────────

void registerReplayEditorCommands() {
    auto& t = Terminal::instance();

    // ── rple: load newest replay or resume session ───────
    t.registerCommand({
        "rple",
        "Load the newest replay into the editor (or resume session if one exists)",
        "rple",
        [](const std::vector<std::string>&) {
            auto clips = listReplayClips();

            if (ReplayEditor::hasSession() && !clips.empty()) {
                Terminal::instance().addLog("Replay editor session found.");
                Terminal::instance().addLog("  rpleresume   Resume previous session");
                Terminal::instance().addLog("  rplelatest   Load newest replay");
                Terminal::instance().addLog("  rpleclose    Clear session");
                return;
            }

            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            Terminal::instance().execute("rplelatest");
        }
    });

    // ── rpleresume: resume previous session ──────────────
    t.registerCommand({
        "rpleresume",
        "Resume the last replay editor session",
        "rpleresume",
        [](const std::vector<std::string>&) {
            if (!ReplayEditor::hasSession()) {
                Terminal::instance().addLog("[ERROR] No editor session found");
                return;
            }
            std::string path;
            {
                std::ifstream f(ReplayEditor::sessionPath());
                if (!f.is_open()) {
                    Terminal::instance().addLog("[ERROR] Cannot read session file");
                    return;
                }
                nlohmann::json j;
                try { f >> j; } catch (...) { Terminal::instance().addLog("[ERROR] Corrupt session file"); return; }
                path = j.value("lastReplay", "");
                if (path.empty()) {
                    Terminal::instance().addLog("[ERROR] Session has no replay path");
                    return;
                }
            }

            if (!std::filesystem::exists(path)) {
                Terminal::instance().addLog("[ERROR] Replay file no longer exists: " + path);
                return;
            }

            if (!REPLAY_PLAYER.loadFromJSON(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load replay: " + path);
                return;
            }
            REPLAY_PLAYER.preloadAssets();
            REPLAY_PLAYER.beginPlayback();
            GAME_STATE = GAME_PLAYING;

            if (!E.load(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load editor: " + path);
                return;
            }

            // Override editor state from session
            E.loadSession();
            REPLAY_PLAYER.seekToTick((int)E.movieTick);

            if (E.freecam)
                REPLAY_PLAYER.cameraController().setMode("freecam");

            Terminal::instance().addLog("[RPLE] Resumed session: " + path);
            Terminal::instance().addLog(std::string("  Tick: ") + std::to_string((int)E.movieTick));
            Terminal::instance().addLog(std::string("  Paused: ") + (E.playing ? "no" : "yes"));
        }
    });

    // ── rplelatest: load newest replay ───────────────────
    t.registerCommand({
        "rplelatest",
        "Load the newest replay into the editor",
        "rplelatest",
        [](const std::vector<std::string>&) {
            auto clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            std::string path = clips[0];
            if (!REPLAY_PLAYER.loadFromJSON(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load replay: " + path);
                return;
            }
            REPLAY_PLAYER.preloadAssets();
            REPLAY_PLAYER.beginPlayback();
            GAME_STATE = GAME_PLAYING;
            if (!E.load(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load editor: " + path);
                return;
            }
            E.playing = true;
            E.movieTick = 0.0f;
            Debug::log(Debug::Category::Replay,
                "[RPLE RELOAD] path=%s playing=%d totalTicks=%d\n",
                path.c_str(), (int)REPLAY_PLAYER.isPlaying(), REPLAY_PLAYER.totalTicks());
            Terminal::instance().addLog("[RPLE] Editor loaded: " + path);
            Terminal::instance().addLog(
                "Replay Editor Ready.\n"
                "  F           Toggle freecam\n"
                "  Left/Right  Seek +/-60 ticks\n"
                "  Space       Play/Pause\n"
                "  K           Create keyframe\n"
                "Type rplehelp for complete walkthrough.");
            Debug::log(Debug::Category::Replay, "[RPLE] Editor loaded: %s\n", path.c_str());
        }
    });

    // ── rplff: replay factory (save 15s clip + open editor) ──
    t.registerCommand({
        "rplff",
        "Save a 15-second instant replay and open it in the editor (rpls + rplelatest)",
        "rplff",
        [](const std::vector<std::string>&) {
            if (!REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("[ERROR] No replay recording active");
                return;
            }
            std::string path = saveInstantReplay(REPLAY_RECORDER, 15);
            if (path.empty()) {
                Terminal::instance().addLog("[ERROR] Failed to save instant replay");
                return;
            }
            Terminal::instance().addLog("[RPLE] Replay saved: " + path);
            Terminal::instance().execute("rplelatest");
        }
    });

    // ── rpleclose: clear session state ───────────────────
    t.registerCommand({
        "rpleclose",
        "Clear the replay editor session file",
        "rpleclose",
        [](const std::vector<std::string>&) {
            std::error_code ec;
            std::filesystem::remove(ReplayEditor::sessionPath(), ec);
            Terminal::instance().addLog("[RPLE] Session cleared");
        }
    });

    // ── rpleload: load specific replay ───────────────────
    t.registerCommand({
        "rpleload",
        "Load a specific replay into the editor by path or index",
        "rpleload <path|index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rpleload <path|index>");
                return;
            }
            std::string path = args[0];
            // Check if it's a numeric index
            bool isNumeric = !path.empty() && path.find_first_not_of("0123456789") == std::string::npos;
            if (isNumeric) {
                int idx = std::stoi(path);
                auto clips = listReplayClips();
                if (idx < 0 || idx >= (int)clips.size()) {
                    Terminal::instance().addLog("[ERROR] Index out of range (0-" +
                        std::to_string(clips.size() - 1) + ")");
                    return;
                }
                path = clips[idx];
            }
            if (!E.load(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load: " + path);
                return;
            }
            Terminal::instance().addLog("[RPLE] Editor loaded: " + path);
        }
    });

    // ── rpleinfo / rplei ─────────────────────────────────
    auto infoFn = [](const std::vector<std::string>&) {
        if (!E.isLoaded()) { requireEditor("rpleinfo"); return; }
        char buf[512];
        int camKf = E.cameraKeyframeCount();
        int camModeKf = E.cameraModeKeyframeCount();
        int timeKf = E.timeKeyframeCount();
        int marks = E.bookmarkCount();
        std::snprintf(buf, sizeof(buf),
            "[RPLE] Replay: %s\n"
            "  Ticks: %d  Rate: %d  Duration: %.1fs\n"
            "  Camera KF: %d  Camera Mode KF: %d  Time KF: %d  Bookmarks: %d\n"
            "  Freecam: %s  Playing: %s  Tick: %.2f\n"
            "  Edit file: %s",
            E.replayPath().c_str(),
            E.totalTicks(), E.tickRate(), E.durationSec(),
            camKf, camModeKf, timeKf, marks,
            E.freecam ? "ON" : "OFF",
            E.playing ? "YES" : "NO",
            E.movieTick,
            E.editPath().c_str());
        Terminal::instance().addLog(buf);
    };
    t.registerCommand({"rpleinfo", "Show editor info", "rpleinfo", infoFn});
    t.registerCommand({"rplei",
        "Show editor info: ticks, time, keyframes, events, audio",
        "rplei",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplei"); return; }
            char buf[512];
            int camKf = E.cameraKeyframeCount();
            int camModeKf = E.cameraModeKeyframeCount();
            int timeKf = E.timeKeyframeCount();
            std::snprintf(buf, sizeof(buf),
                "Total ticks: %d\n"
                "Total time: %02d:%02d\n"
                "Tick rate: %d\n"
                "Camera keyframes: %d\n"
                "Camera mode keyframes: %d\n"
                "Playback speed keyframes: %d\n"
                "Bookmarks: %d",
                E.totalTicks(),
                (int)(E.durationSec()) / 60, (int)(E.durationSec()) % 60,
                E.tickRate(), camKf, camModeKf, timeKf, E.bookmarkCount());
            Terminal::instance().addLog(buf);

            // Audio tracks
            if (E.audioTrackCount() > 0) {
                Terminal::instance().addLog("\nAudio:");
                for (int i = 0; i < E.audioTrackCount(); ++i) {
                    const auto& at = E.audioTrack(i);
                    char ab[256];
                    std::snprintf(ab, sizeof(ab),
                        "%d. %s\n"
                        "   start tick: %d  volume: %.1f  enabled: %s",
                        i + 1, at.path.c_str(), at.startTick, at.volume,
                        at.enabled ? "yes" : "no");
                    Terminal::instance().addLog(ab);
                }
            } else {
                Terminal::instance().addLog("\nAudio: none");
            }

            // Print killfeed events from loaded replay
            if (REPLAY_PLAYER.totalTicks() > 0) {
                Terminal::instance().addLog("\nEvents:");
                const auto& events = REPLAY_PLAYER.soundEvents();
                int eventCount = 0;
                for (const auto& ev : events) {
                    if (ev.soundPath.find("kill") != std::string::npos ||
                        ev.soundPath.find("death") != std::string::npos ||
                        ev.soundPath.find("spawn") != std::string::npos) {
                        char eb[128];
                        std::snprintf(eb, sizeof(eb), "  %s at tick %d pos=(%.0f %.0f %.0f)",
                            ev.soundPath.c_str(), ev.tick,
                            ev.position.x, ev.position.y, ev.position.z);
                        Terminal::instance().addLog(eb);
                        eventCount++;
                    }
                }
                if (eventCount == 0) {
                    Terminal::instance().addLog("  (no notable events found)");
                }
            }
        }
    });

    // ── rple_sl: load audio track ────────────────────────
    t.registerCommand({
        "rple_sl",
        "Load a music/audio file for the replay timeline",
        "rple_sl <path>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rple_sl"); return; }
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rple_sl <full path to .wav or .mp3>");
                Terminal::instance().addLog("Example: rple_sl C:\\path\\to\\song.wav");
                return;
            }
            std::string path = args[0];
            Debug::log(Debug::Category::Replay, "[RPLE] rple_sl command start\n");
            Debug::log(Debug::Category::Replay, "[RPLE] rple_sl path received: %s\n", path.c_str());

            // Reconstruct path from all args if there are spaces
            for (size_t i = 1; i < args.size(); ++i)
                path += " " + args[i];

            // Strip surrounding quotes if present
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                path = path.substr(1, path.size() - 2);

            Debug::log(Debug::Category::Replay, "[RPLE] rple_sl resolved path: %s\n", path.c_str());

            if (!std::filesystem::exists(path)) {
                Terminal::instance().addLog("[ERROR] File not found: " + path);
                Debug::log(Debug::Category::Replay, "[RPLE] rple_sl file NOT FOUND: %s\n", path.c_str());
                return;
            }

            std::string ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav" && ext != ".mp3") {
                Terminal::instance().addLog("[ERROR] Unsupported format: " + ext + " (only .wav and .mp3)");
                Debug::log(Debug::Category::Replay, "[RPLE] rple_sl unsupported type: %s\n", ext.c_str());
                return;
            }

            Debug::log(Debug::Category::Replay, "[RPLE] rple_sl file type OK: %s\n", ext.c_str());

            // Stop any existing preview
            stopReplayMusicPreview();

            // Set the audio track in editor
            E.setAudioTrack(path, 1.0f);

            // Start preview playback at current tick
            int currentTick = (int)E.movieTick;
            float startSec = (float)currentTick / 60.0f;
            if (playReplayMusicPreview(path, 1.0f)) {
                seekReplayMusicPreview(startSec);
                Terminal::instance().addLog("[RPLE] Audio track loaded and preview started");
                Debug::log(Debug::Category::Replay, "[RPLE] rple_sl preview start OK, tick=%d sec=%.2f\n",
                           currentTick, startSec);
            } else {
                Terminal::instance().addLog("[RPLE] Audio track path saved (preview unavailable)");
                Debug::log(Debug::Category::Replay, "[RPLE] rple_sl load OK but preview FAILED: %s\n", path.c_str());
            }

            // Autosave is called inside setAudioTrack
        }
    });

    // ── rple_sl_clear: remove audio track ────────────────
    t.registerCommand({
        "rple_sl_clear",
        "Remove loaded audio track from timeline",
        "rple_sl_clear",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rple_sl_clear"); return; }
            stopReplayMusicPreview();
            E.clearAudioTracks();
            Terminal::instance().addLog("[RPLE] Audio track cleared");
        }
    });

    // ── rple_sl_vol: set audio track volume ──────────────
    t.registerCommand({
        "rple_sl_vol",
        "Set audio track volume (0.0 - 2.0)",
        "rple_sl_vol <volume>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rple_sl_vol"); return; }
            if (args.empty() || E.audioTrackCount() == 0) {
                Terminal::instance().addLog("[ERROR] No audio track loaded. Use rple_sl first.");
                return;
            }
            float vol = std::clamp(std::stof(args[0]), 0.0f, 2.0f);
            ReplayEditorAudioTrack* track = E.mutableAudioTrack(0);
            if (track) {
                track->volume = vol;
                E.autosave();
                char buf[64];
                std::snprintf(buf, sizeof(buf), "[RPLE] Audio track volume set to %.2f", vol);
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rpleplay ─────────────────────────────────────────
    t.registerCommand({
        "rpleplay",
        "Start replay playback in editor",
        "rpleplay",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpleplay"); return; }
            E.playing = true;
            if (REPLAY_PLAYER.totalTicks() > 0 && E.movieTick < 0.5f)
                REPLAY_PLAYER.seekToTick(0);
            Terminal::instance().addLog("[RPLE] Playing");
        }
    });

    // ── rplepause ────────────────────────────────────────
    t.registerCommand({
        "rplepause",
        "Pause replay playback",
        "rplepause",
        [](const std::vector<std::string>&) {
            E.playing = false;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Paused at tick %.0f", E.movieTick);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rpletoggle ───────────────────────────────────────
    t.registerCommand({
        "rpletoggle",
        "Toggle play/pause",
        "rpletoggle",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpletoggle"); return; }
            E.playing = !E.playing;
            Terminal::instance().addLog(E.playing ? "[RPLE] Playing" : "[RPLE] Paused");
        }
    });

    // ── rpleseek ─────────────────────────────────────────
    t.registerCommand({
        "rpleseek",
        "Seek to a specific replay tick",
        "rpleseek <tick>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rpleseek"); return; }
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rpleseek <tick>"); return; }
            int tick = std::clamp(std::stoi(args[0]), 0, E.totalTicks());
            E.seekToTick(tick);
            if (REPLAY_PLAYER.totalTicks() > 0)
                REPLAY_PLAYER.seekToTick(tick);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Seeked to tick %d", tick);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rpletj ───────────────────────────────────────────
    t.registerCommand({
        "rpletj",
        "Jump to exact replay tick",
        "rpletj <tick>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rpletj"); return; }
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rpletj <tick>"); return; }
            int tick = std::clamp(std::stoi(args[0]), 0, E.totalTicks());
            E.seekToTick(tick);
            if (REPLAY_PLAYER.totalTicks() > 0)
                REPLAY_PLAYER.seekToTick(tick);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Jumped to tick %d", tick);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rpleseeksec ──────────────────────────────────────
    t.registerCommand({
        "rpleseeksec",
        "Seek to a specific time in seconds",
        "rpleseeksec <seconds>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rpleseeksec"); return; }
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rpleseeksec <seconds>"); return; }
            float sec = std::max(0.0f, std::stof(args[0]));
            int tick = (int)(sec * E.tickRate());
            tick = std::clamp(tick, 0, E.totalTicks());
            E.seekToTick(tick);
            if (REPLAY_PLAYER.totalTicks() > 0)
                REPLAY_PLAYER.seekToTick(tick);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Seeked to %.1fs (tick %d)", sec, tick);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rples (keyboard seek mode) ───────────────────────
    // Lightweight — just a flag; actual keyboard handling in engine-tick-camera.cpp
    static bool gKeyboardSeekMode = false;
    t.registerCommand({
        "rples",
        "Enable/disable keyboard seek mode (Left=-60, Right=+60, Space=play/pause)",
        "rples [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                gKeyboardSeekMode = !gKeyboardSeekMode;
            else
                gKeyboardSeekMode = args[0] == "1";
            Terminal::instance().addLog(std::string("[RPLE] Keyboard seek mode: ") +
                (gKeyboardSeekMode ? "ON" : "OFF"));
        }
    });

    // ── rplefc (free camera toggle) ──────────────────────
    t.registerCommand({
        "rplefc",
        "Toggle free camera in editor",
        "rplefc",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplefc"); return; }
            E.freecam = !E.freecam;
            if (E.freecam) {
                // Store previous camera mode
                E.mPrevCameraMode = REPLAY_PLAYER.cameraController().modeName();
                // Store current camera state
                E.freecamPos = THE_CAMERA.pos;
                E.freecamRot = eulerToQuat(THE_CAMERA.yaw, THE_CAMERA.pitch);
                E.freecamFov = THE_CAMERA.fov;
                // Set camera controller to freecam mode
                REPLAY_PLAYER.cameraController().setMode("freecam");
                Debug::log(Debug::Category::Replay,
                    "[rplefc] Command executed: entering freecam, prev mode=%s\n",
                    E.mPrevCameraMode.c_str());
                Debug::log(Debug::Category::Replay,
                    "[rplefc] Replay Freecam Enabled: sens=%.3f mouseMode=disabled speed=%.1f\n",
                    CAMERA_SENS, GetPlayerSettings().freecamSpeed);
            } else {
                // Restore previous camera mode
                std::string restoreMode = E.mPrevCameraMode.empty() ? "tp" : E.mPrevCameraMode;
                REPLAY_PLAYER.cameraController().setMode(restoreMode);
                E.mPrevCameraMode.clear();
                Debug::log(Debug::Category::Replay,
                    "[rplefc] Command executed: exiting freecam, restoring mode=%s\n",
                    restoreMode.c_str());
            }
            Terminal::instance().addLog(std::string("[RPLE] Free camera: ") +
                (E.freecam ? "ON" : "OFF"));
        }
    });

    // ── rplefc_pos ───────────────────────────────────────
    t.registerCommand({
        "rplefc_pos",
        "Print current free camera position",
        "rplefc_pos",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplefc_pos"); return; }
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Camera pos: (%.2f %.2f %.2f)",
                E.freecamPos.x, E.freecamPos.y, E.freecamPos.z);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplefc_rot ───────────────────────────────────────
    t.registerCommand({
        "rplefc_rot",
        "Print current free camera rotation (yaw/pitch)",
        "rplefc_rot",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplefc_rot"); return; }
            glm::vec3 euler = glm::eulerAngles(E.freecamRot);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Camera rot: yaw=%.1f pitch=%.1f roll=%.1f",
                glm::degrees(euler.z), glm::degrees(euler.y), glm::degrees(E.freecamRoll));
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplefc_roll ──────────────────────────────────────
    t.registerCommand({
        "rplefc_roll",
        "Set camera roll in degrees",
        "rplefc_roll <degrees>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplefc_roll"); return; }
            float roll = args.empty() ? 0.0f : std::stof(args[0]);
            E.freecamRoll = roll;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Camera roll set to %.1f", roll);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplefc_fov ───────────────────────────────────────
    t.registerCommand({
        "rplefc_fov",
        "Set camera FOV",
        "rplefc_fov <value>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplefc_fov"); return; }
            float fov = args.empty() ? 70.0f : std::clamp(std::stof(args[0]), 10.0f, 160.0f);
            E.freecamFov = fov;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] FOV set to %.0f", fov);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplefc_skf (set camera keyframe) ────────────────
    t.registerCommand({
        "rplefc_skf",
        "Create camera keyframe at current replay tick",
        "rplefc_skf",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplefc_skf"); return; }
            uint32_t replayTick = REPLAY_PLAYER.currentTick();
            int tick = (int)replayTick;
            glm::vec3 pos = E.freecam ? E.freecamPos : THE_CAMERA.pos;
            glm::quat rot = E.freecam ? E.freecamRot : eulerToQuat(THE_CAMERA.yaw, THE_CAMERA.pitch);

            Debug::log(Debug::Category::Replay,
                "\n============================\n"
                "KEYFRAME CREATE\n"
                "  Replay Loaded:     %s\n"
                "  Replay Playing:    %s\n"
                "  Replay CurrentTick: %u\n"
                "  Replay TotalTicks: %u\n"
                "  Editor Tick:       %.1f\n"
                "  Stored Tick:       %d\n"
                "  Camera Pos:        (%.1f %.1f %.1f)\n"
                "  Camera Rot:        (%.2f %.2f %.2f %.2f)\n"
                "  Camera Forward:    (%.2f %.2f %.2f)\n"
                "============================\n",
                E.isLoaded() ? "YES" : "NO",
                REPLAY_PLAYER.isPlaying() ? "YES" : "NO",
                replayTick,
                REPLAY_PLAYER.totalTicks(),
                E.movieTick,
                tick,
                pos.x, pos.y, pos.z,
                rot.x, rot.y, rot.z, rot.w,
                THE_CAMERA.front.x, THE_CAMERA.front.y, THE_CAMERA.front.z);

            if (tick != (int)E.movieTick) {
                Debug::warn(Debug::Category::Replay,
                    "[RPLE] WARNING: Stored tick %d differs from editor tick %.1f. "
                    "Using replay player tick as authoritative source.\n",
                    tick, E.movieTick);
            }

            E.addCameraKeyframe(tick, pos, rot, E.freecamRoll, E.freecamFov, E.defaultInterp);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Camera keyframe at tick %d (%.1f %.1f %.1f)",
                tick, pos.x, pos.y, pos.z);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplekf_l: list ALL keyframes ─────────────────────
    t.registerCommand({
        "rplekf_l",
        "List every keyframe (camera, camera-mode, time)",
        "rplekf_l",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplekf_l"); return; }
            int n = E.cameraKeyframeCount();
            if (n == 0) Terminal::instance().addLog("[RPLE] No camera keyframes");
            else for (int i = 0; i < n; ++i) {
                const auto& kf = E.cameraKeyframe(i);
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "[RPLE] KF %d: tick=%d type=camera pos=(%.1f %.1f %.1f) roll=%.1f fov=%.0f interp=%s",
                    i, kf.tick, kf.position.x, kf.position.y, kf.position.z,
                    kf.roll, kf.fov, interpName(kf.interp));
                Terminal::instance().addLog(buf);
            }

            int cmN = E.cameraModeKeyframeCount();
            if (cmN == 0) Terminal::instance().addLog("[RPLE] No camera mode keyframes");
            else for (int i = 0; i < cmN; ++i) {
                const auto& kf = E.cameraModeKeyframe(i);
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[RPLE] KF %d: tick=%d type=camera-mode mode=%s",
                    i, kf.tick, camModeName(kf.mode));
                Terminal::instance().addLog(buf);
            }

            int tN = E.timeKeyframeCount();
            if (tN == 0) Terminal::instance().addLog("[RPLE] No time keyframes");
            else for (int i = 0; i < tN; ++i) {
                const auto& kf = E.timeKeyframe(i);
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[RPLE] KF %d: tick=%d type=time speed=%.2fx interp=%s",
                    i, kf.tick, kf.speed, interpName(kf.interp));
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rplekf_interp: change interpolation mode ─────────
    t.registerCommand({
        "rplekf_interp",
        "Change interpolation mode of a camera keyframe",
        "rplekf_interp <index> <linear|easein|easeout|easeinout|smooth|cut>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplekf_interp"); return; }
            if (args.size() < 2) {
                Terminal::instance().addLog("[ERROR] Usage: rplekf_interp <index> <mode>");
                return;
            }
            int idx = std::stoi(args[0]);
            if (idx < 0 || idx >= E.cameraKeyframeCount()) {
                Terminal::instance().addLog("[ERROR] Keyframe index out of range");
                return;
            }
            KeyframeInterp mode = interpFromString(args[1]);
            E.setCameraKeyframeInterp(idx, mode);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "[RPLE] Keyframe %d interp set to %s", idx, interpName(mode));
            Terminal::instance().addLog(buf);
            E.autosave();
        }
    });

    // ── rplekf_d: delete nearest camera keyframe ─────────
    t.registerCommand({
        "rplekf_d",
        "Delete nearest/current camera keyframe",
        "rplekf_d",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplekf_d"); return; }
            int tick = (int)E.movieTick;
            int idx = E.findNearestCameraKeyframe(tick);
            if (idx < 0) {
                Terminal::instance().addLog("[RPLE] No camera keyframes to delete");
                return;
            }
            const auto& kf = E.cameraKeyframe(idx);
            if (E.deleteCameraKeyframe(idx)) {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "[RPLE] Deleted camera keyframe %d at tick %d (nearest to %d)",
                    idx, kf.tick, tick);
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rplekf_da: delete ALL keyframes ──────────────────
    static bool gPendingDeleteAll = false;
    t.registerCommand({
        "rplekf_da",
        "Delete ALL keyframes (call again to confirm)",
        "rplekf_da [yes|1]",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplekf_da"); return; }
            int total = E.cameraKeyframeCount() + E.cameraModeKeyframeCount() + E.timeKeyframeCount();
            if (total == 0) {
                Terminal::instance().addLog("[RPLE] No keyframes to delete");
                gPendingDeleteAll = false;
                return;
            }
            bool confirmed = (!args.empty() && (args[0] == "1" || args[0] == "yes" || args[0] == "y"));
            if (confirmed || gPendingDeleteAll) {
                E.clearCameraKeyframes();
                E.clearCameraModeKeyframes();
                E.clearTimeKeyframes();
                Terminal::instance().addLog("[RPLE] Deleted all keyframes");
                gPendingDeleteAll = false;
            } else {
                gPendingDeleteAll = true;
                char promptBuf[128];
                std::snprintf(promptBuf, sizeof(promptBuf),
                    "Delete all keyframes (%d total)?\n"
                    "Call rplekf_da again, or rplekf_da yes to confirm", total);
                Terminal::instance().addLog(std::string(promptBuf));
            }
        }
    });
    t.registerCommand({
        "rplekf_da_no",
        "Cancel pending delete-all confirmation",
        "rplekf_da_no",
        [](const std::vector<std::string>&) {
            gPendingDeleteAll = false;
            Terminal::instance().addLog("[RPLE] Delete canceled");
        }
    });

    // ── rplefc_list ──────────────────────────────────────
    t.registerCommand({
        "rplefc_list",
        "List all camera keyframes",
        "rplefc_list",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplefc_list"); return; }
            int n = E.cameraKeyframeCount();
            if (n == 0) { Terminal::instance().addLog("[RPLE] No camera keyframes"); return; }
            char buf[256];
            for (int i = 0; i < n; ++i) {
                const auto& kf = E.cameraKeyframe(i);
                std::snprintf(buf, sizeof(buf), "[RPLE] %d: tick=%d pos=(%.1f %.1f %.1f) fov=%.0f interp=%s",
                    i, kf.tick, kf.position.x, kf.position.y, kf.position.z,
                    kf.fov, interpName(kf.interp));
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rplefc_del ───────────────────────────────────────
    t.registerCommand({
        "rplefc_del",
        "Delete camera keyframe by index",
        "rplefc_del <index>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplefc_del"); return; }
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rplefc_del <index>"); return; }
            int idx = std::stoi(args[0]);
            if (E.deleteCameraKeyframe(idx))
                Terminal::instance().addLog("[RPLE] Deleted camera keyframe " + args[0]);
            else
                Terminal::instance().addLog("[ERROR] Index out of range");
        }
    });

    // ── rplefc_clear ─────────────────────────────────────
    t.registerCommand({
        "rplefc_clear",
        "Delete every camera keyframe",
        "rplefc_clear",
        [](const std::vector<std::string>&) {
            E.clearCameraKeyframes();
            Terminal::instance().addLog("[RPLE] Cleared all camera keyframes");
        }
    });

    // ── rplefc_goto ──────────────────────────────────────
    t.registerCommand({
        "rplefc_goto",
        "Jump replay to camera keyframe tick",
        "rplefc_goto <index>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplefc_goto"); return; }
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rplefc_goto <index>"); return; }
            int idx = std::stoi(args[0]);
            if (idx < 0 || idx >= E.cameraKeyframeCount()) {
                Terminal::instance().addLog("[ERROR] Index out of range");
                return;
            }
            int tick = E.cameraKeyframe(idx).tick;
            E.seekToTick(tick);
            if (REPLAY_PLAYER.totalTicks() > 0)
                REPLAY_PLAYER.seekToTick(tick);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Jumped to camera keyframe %d (tick %d)", idx, tick);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplefc_interp ────────────────────────────────────
    t.registerCommand({
        "rplefc_interp",
        "Set default interpolation mode for new camera keyframes",
        "rplefc_interp <linear|easein|easeout|easeinout|smooth|cut>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(std::string("[RPLE] Current interp: ") +
                    interpName(E.defaultInterp));
                return;
            }
            E.defaultInterp = interpFromString(args[0]);
            Terminal::instance().addLog(std::string("[RPLE] Default interp set to: ") +
                interpName(E.defaultInterp));
        }
    });

    // ── rpletime_skf ─────────────────────────────────────
    t.registerCommand({
        "rpletime_skf",
        "Create playback speed keyframe at current tick",
        "rpletime_skf <speed>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rpletime_skf"); return; }
            float speed = args.empty() ? 1.0f : std::clamp(std::stof(args[0]), 0.01f, 100.0f);
            int tick = (int)E.movieTick;
            E.addTimeKeyframe(tick, speed, E.defaultInterp);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLE] Time keyframe at tick %d: %.2fx", tick, speed);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rpletime_list ────────────────────────────────────
    t.registerCommand({
        "rpletime_list",
        "List all playback speed keyframes",
        "rpletime_list",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpletime_list"); return; }
            int n = E.timeKeyframeCount();
            if (n == 0) { Terminal::instance().addLog("[RPLE] No time keyframes"); return; }
            char buf[128];
            for (int i = 0; i < n; ++i) {
                const auto& kf = E.timeKeyframe(i);
                std::snprintf(buf, sizeof(buf), "[RPLE] %d: tick=%d speed=%.2fx interp=%s",
                    i, kf.tick, kf.speed, interpName(kf.interp));
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rpletime_del ─────────────────────────────────────
    t.registerCommand({
        "rpletime_del",
        "Delete playback speed keyframe by index",
        "rpletime_del <index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rpletime_del <index>"); return; }
            int idx = std::stoi(args[0]);
            if (E.deleteTimeKeyframe(idx))
                Terminal::instance().addLog("[RPLE] Deleted time keyframe " + args[0]);
            else
                Terminal::instance().addLog("[ERROR] Index out of range");
        }
    });

    // ── rpletime_clear ───────────────────────────────────
    t.registerCommand({
        "rpletime_clear",
        "Delete every playback speed keyframe",
        "rpletime_clear",
        [](const std::vector<std::string>&) {
            E.clearTimeKeyframes();
            Terminal::instance().addLog("[RPLE] Cleared all time keyframes");
        }
    });

    // ── rpleevents ───────────────────────────────────────
    t.registerCommand({
        "rpleevents",
        "Print event timeline from replay",
        "rpleevents",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpleevents"); return; }
            // Basic events from the replay player
            std::vector<ReplayKillfeedEvent> kills;
            REPLAY_PLAYER.takeTriggeredKillfeedEvents(kills);
            // Re-trigger — they were consumed, but we can list from the clip
            // For now, just log what's available
            std::vector<ReplaySoundEvent> sounds;
            REPLAY_PLAYER.takeTriggeredSounds(sounds); (void)sounds;
            char buf[256];
            std::snprintf(buf, sizeof(buf), "[RPLE] %d total ticks, %d keyframes, %d bookmarks",
                E.totalTicks(), E.cameraKeyframeCount(), E.bookmarkCount());
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplemark ─────────────────────────────────────────
    t.registerCommand({
        "rplemark",
        "Create a bookmark at current tick",
        "rplemark <label>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplemark"); return; }
            std::string label;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) label += " ";
                label += args[i];
            }
            int tick = (int)E.movieTick;
            E.addBookmark(tick, label);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Bookmark at tick %d: %s", tick, label.c_str());
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplemarks ────────────────────────────────────────
    t.registerCommand({
        "rplemarks",
        "List all bookmarks",
        "rplemarks",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplemarks"); return; }
            int n = E.bookmarkCount();
            if (n == 0) { Terminal::instance().addLog("[RPLE] No bookmarks"); return; }
            char buf[128];
            for (int i = 0; i < n; ++i) {
                const auto& bm = E.bookmark(i);
                std::snprintf(buf, sizeof(buf), "[RPLE] %d: tick=%d %s", i, bm.tick, bm.label.c_str());
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── rplemark_del ─────────────────────────────────────
    t.registerCommand({
        "rplemark_del",
        "Delete bookmark by index",
        "rplemark_del <index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[ERROR] Usage: rplemark_del <index>"); return; }
            int idx = std::stoi(args[0]);
            if (E.deleteBookmark(idx))
                Terminal::instance().addLog("[RPLE] Deleted bookmark " + args[0]);
            else
                Terminal::instance().addLog("[ERROR] Index out of range");
        }
    });

    // ── rplesave ─────────────────────────────────────────
    t.registerCommand({
        "rplesave",
        "Save .rple.json edit file",
        "rplesave",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplesave"); return; }
            if (E.saveEdit()) {
                Terminal::instance().addLog("[RPLE] Saved: " + E.editPath());
                E.autosave();
            } else {
                Terminal::instance().addLog("[ERROR] Failed to save edit file");
            }
        }
    });

    // ── rpleundo: Ctrl+Z through autosaves ──────────────
    t.registerCommand({
        "rpleundo",
        "Undo last editor change (restore previous autosave)",
        "rpleundo",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpleundo"); return; }
            if (E.undoLastAutosave())
                Terminal::instance().addLog("[RPLE] Undo: restored previous state");
            else
                Terminal::instance().addLog("[RPLE] Undo: no autosave history available");
        }
    });

    // ── rple_exit: exit replay editor ────────────────────
    t.registerCommand({
        "rple_exit",
        "Exit the replay editor and stop playback",
        "rple_exit",
        [](const std::vector<std::string>&) {
            stopReplayMusicPreview();
            if (gReplayEditor.isLoaded())
                gReplayEditor.unload();
            if (REPLAY_PLAYER.isPlaying())
                REPLAY_PLAYER.stopPlayback();
            REPLAY_ACTOR_MODELS.clear();
            REPLAY_WEAPON_MODELS.clear();
            REPLAY_CHAT_STATES.clear();
            Terminal::instance().addLog("[RPLE] Editor exited");
            Debug::log(Debug::Category::Replay, "[RPLE EXIT] cleared replay editor state\n");
        }
    });

    // ── rpleloadedit ─────────────────────────────────────
    t.registerCommand({
        "rpleloadedit",
        "Load .rpledit edit file for current replay",
        "rpleloadedit",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpleloadedit"); return; }
            if (E.loadEdit())
                Terminal::instance().addLog("[RPLE] Loaded edit: " + E.editPath());
            else
                Terminal::instance().addLog("[ERROR] No edit file found: " + E.editPath());
        }
    });

    // ── rplehelp ─────────────────────────────────────────
    t.registerCommand({
        "rplehelp",
        "Print the complete replay editor walkthrough",
        "rplehelp",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "=========================================\n"
                " MIMITA REPLAY EDITOR\n"
                "=========================================\n"
                "\n"
                "WORKFLOW:\n"
                "  Play \xE2\x86\x92 F3 (save replay) \xE2\x86\x92 rple \xE2\x86\x92 edit \xE2\x86\x92 rplx (export)\n"
                "\n"
                "KEYBOARD CONTROLS (editor open):\n"
                "  F           Toggle freecam\n"
                "  WASD+QE     Move freecam\n"
                "  Mouse       Look\n"
                "  Shift       Fast (3x)\n"
                "  Ctrl        Slow (0.3x)\n"
                "  Space       Play / Pause\n"
                "  Left Arrow  -60 ticks (1s back)\n"
                "  Right Arrow +60 ticks (1s forward)\n"
                "  Shift+Up    Next keyframe\n"
                "  Shift+Down  Previous keyframe\n"
                "  K           Create keyframe\n"
                "  Ctrl+Z      Undo (via autosave)\n"
                "\n"
                "COMMANDS:\n"
                "  rple          Load newest replay + start editor\n"
                "  rplei         Print editor info + events\n"
                "  rpletj <tick> Jump to exact tick\n"
                "  rplekf_l      List all keyframes\n"
                "  rplekf_d      Delete nearest camera keyframe\n"
                "  rplekf_da     Delete ALL keyframes\n"
                "  rplesave      Save editor project\n"
                "  rpleundo      Undo last change\n"
                "  rple_exit     Exit replay editor\n"
                "  rplx          Export to MP4\n"
                "\n"
                "For all commands: rplecmds\n"
                "=========================================");
        }
    });

    // ── rplecmds ─────────────────────────────────────────
    t.registerCommand({
        "rplecmds",
        "List all replay editor commands by category",
        "rplecmds",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "Replay Editor Commands:\n"
                "\n"
                "--- Basics ---\n"
                "  rple          Load newest replay + start editor\n"
                "  rpleload      Load a specific replay\n"
                "  rplehelp      Show walkthrough\n"
                "  rplestatus    Show editor state\n"
                "  rplecmds      List commands\n"
                "  rplei         Print editor info + events\n"
                "\n"
                "--- Playback ---\n"
                "  rpleplay      Start playback\n"
                "  rplepause     Pause\n"
                "  rpletoggle    Toggle play/pause\n"
                "  rpleseek      Seek to tick\n"
                "  rpleseeksec   Seek to seconds\n"
                "  rpletj        Jump to exact tick\n"
                "\n"
                "--- Free Camera ---\n"
                "  rplefc        Toggle freecam (or press F)\n"
                "  rplefc_pos    Print camera position\n"
                "  rplefc_rot    Print camera rotation\n"
                "  rplefc_roll   Set roll\n"
                "  rplefc_fov    Set FOV\n"
                "\n"
                "--- Keyframes ---\n"
                "  rplefc_skf    Create camera keyframe\n"
                "  rplefc_list   List camera keyframes\n"
                "  rplefc_del    Delete camera keyframe\n"
                "  rplefc_clear  Clear camera keyframes\n"
                "  rplefc_goto   Jump to keyframe\n"
                "  rplefc_interp Set interpolation mode\n"
                "  rplekf_l      List ALL keyframes\n"
                "  rplekf_d      Delete nearest camera keyframe\n"
                "  rplekf_da     Delete ALL keyframes (call twice)\n"
                "  rplekf_da_no  Cancel pending delete-all\n"
                "  rple_sl       Load music for timeline + export\n"
                "\n"
                "--- Camera Mode Keyframes ---\n"
                "  (Create with K \xE2\x86\x92 option 2)\n"
                "\n"
                "--- Speed ---\n"
                "  rpletime_skf  Create speed keyframe\n"
                "  rpletime_list List speed keyframes\n"
                "  rpletime_del  Delete speed keyframe\n"
                "  rpletime_clear Clear speed keyframes\n"
                "\n"
                "--- Audio Track ---\n"
                "  rple_sl       Load music/audio file for timeline\n"
                "  rple_sl_vol   Set audio track volume\n"
                "  rple_sl_clear Remove audio track\n"
                "\n"
                "--- Bookmarks ---\n"
                "  rplemark      Create bookmark\n"
                "  rplemarks     List bookmarks\n"
                "  rplemark_del  Delete bookmark\n"
                "\n"
                "--- Save/Load ---\n"
                "  rplesave      Save .rple.json edit file\n"
                "  rpleloadedit  Load .rple.json edit file\n"
                "  rpleundo      Undo last change (Ctrl+Z)\n"
                "  rple_exit     Exit replay editor cleanly\n"
                "\n"
                "--- Aliases ---\n"
                "  replay_editor       = rple\n"
                "  replay_freecam      = rplefc\n"
                "  replay_keyframe     = rplefc_skf\n"
                "  replay_export       = rplx\n"
                "  replay_info         = rpleinfo\n"
                "  replay_status       = rplestatus\n");
        }
    });

    // ── rplestatus ───────────────────────────────────────
    t.registerCommand({
        "rplestatus",
        "Show current replay editor state",
        "rplestatus",
        [](const std::vector<std::string>&) {
            char buf[512];
            const char* cameraMode = "N/A";
            if (REPLAY_PLAYER.isPlaying()) {
                if (REPLAY_PLAYER.cameraController().mode() == ReplayCameraMode::Freecam)
                    cameraMode = "Freecam";
                else if (gReplayEditor.cameraKeyframeCount() > 0)
                    cameraMode = "Interpolating";
                else
                    cameraMode = REPLAY_PLAYER.cameraController().modeName();
            }
            int kfCount = gReplayEditor.cameraKeyframeCount();
            int cmKfCount = gReplayEditor.cameraModeKeyframeCount();
            int timeKfCount = gReplayEditor.timeKeyframeCount();
            int bkCount = gReplayEditor.bookmarkCount();
            int asCount = gReplayEditor.autosaveCount();
            std::snprintf(buf, sizeof(buf),
                "\n=== Replay Editor Status ===\n"
                "  Replay Loaded:     %s\n"
                "  Editor Active:     %s\n"
                "  Playing:           %s\n"
                "  Freecam:           %s\n"
                "  MovieTick:         %.0f / %d\n"
                "  Camera KF:         %d\n"
                "  Camera Mode KF:    %d\n"
                "  Speed KF:          %d\n"
                "  Bookmarks:         %d\n"
                "  Camera Mode:       %s\n"
                "  Autosaves:         %d\n"
                "  Edit File:         %s\n"
                "=============================",
                REPLAY_PLAYER.totalTicks() > 0 ? "YES" : "NO",
                gReplayEditor.isLoaded() ? "YES" : "NO",
                gReplayEditor.playing ? "YES" : "NO",
                gReplayEditor.freecam ? "ON" : "OFF",
                gReplayEditor.movieTick, gReplayEditor.totalTicks(),
                kfCount, cmKfCount, timeKfCount, bkCount,
                cameraMode, asCount,
                gReplayEditor.editPath().c_str());
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplekf_sa: select all keyframes by type ──────────
    t.registerCommand({
        "rplekf_sa",
        "Select all keyframes: 1=cammode 2=campos 3=pbspeed 4=all",
        "rplekf_sa <1|2|3|4>",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplekf_sa"); return; }
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rplekf_sa <1|2|3|4>");
                Terminal::instance().addLog("  1 = select all cammode");
                Terminal::instance().addLog("  2 = select all campos");
                Terminal::instance().addLog("  3 = select all pbspeed");
                Terminal::instance().addLog("  4 = select all keyframes");
                return;
            }
            int type = std::stoi(args[0]);
            KeyframeFilter filter;
            const char* label;
            switch (type) {
                case 1: filter = KeyframeFilter::Cammode; label = "camera mode"; break;
                case 2: filter = KeyframeFilter::Campos;  label = "camera position"; break;
                case 3: filter = KeyframeFilter::Pbspeed; label = "playback speed"; break;
                case 4: filter = KeyframeFilter::All;     label = "all"; break;
                default:
                    Terminal::instance().addLog("[ERROR] Invalid type. Use 1-4.");
                    return;
            }
            E.selectAll(filter);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Selected %d %s keyframes", E.selectedCount(), label);
            Terminal::instance().addLog(buf);
        }
    });

    // ── rplekf_sd: deselect all keyframes ────────────────
    t.registerCommand({
        "rplekf_sd",
        "Deselect all keyframes",
        "rplekf_sd",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplekf_sd"); return; }
            E.clearSelection();
            Terminal::instance().addLog("[RPLE] Selection cleared");
        }
    });

    // ── rplekf_si: set interpolation on selected keyframes
    t.registerCommand({
        "rplekf_si",
        "Set interpolation mode on selected keyframes. Shows prompt if no arg.",
        "rplekf_si [linear|easein|easeout|easeinout|smooth|cut|bezier|exponential]",
        [](const std::vector<std::string>& args) {
            if (!E.isLoaded()) { requireEditor("rplekf_si"); return; }
            if (!E.hasSelection()) {
                Terminal::instance().addLog("[ERROR] No keyframes selected. Use rplekf_sa first.");
                return;
            }
            if (args.empty()) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "[RPLE] %d keyframes selected. Set interpolation with: rplekf_si <linear|easein|easeout|easeinout|smooth|cut|bezier|exponential>", E.selectedCount());
                Terminal::instance().addLog(buf);
                return;
            }
            KeyframeInterp mode = interpFromString(args[0]);
            if (E.setInterpOnSelected(mode)) {
                E.autosave();
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[RPLE] Set %d selected keyframes to %s", E.selectedCount(), interpName(mode));
                Terminal::instance().addLog(buf);
            }
        }
    });

    // ── Friendly aliases ─────────────────────────────────
    t.registerCommand({
        "replay_editor",
        "Alias for rple: load newest replay into editor",
        "replay_editor",
        [](const std::vector<std::string>& args) {
            // Forward to rple behavior (same as rple with no args)
            auto clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            std::string path = clips[0];
            if (!gReplayEditor.load(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load: " + path);
                return;
            }
            Terminal::instance().addLog("[RPLE] Editor loaded: " + path);
            Terminal::instance().addLog(
                "Replay Editor Ready.\n"
                "Type rplehelp for a complete walkthrough.");
        }
    });

    t.registerCommand({
        "replay_freecam",
        "Alias for rplefc: toggle replay freecam",
        "replay_freecam [0|1]",
        [](const std::vector<std::string>& args) {
            bool enable = args.empty() ? !gReplayEditor.freecam : (args[0] == "1");
            if (enable && !gReplayEditor.freecam) {
                // Same logic as rplefc
                gReplayEditor.freecam = true;
                gReplayEditor.mPrevCameraMode = REPLAY_PLAYER.cameraController().modeName();
                gReplayEditor.freecamPos = THE_CAMERA.pos;
                gReplayEditor.freecamRot = eulerToQuat(THE_CAMERA.yaw, THE_CAMERA.pitch);
                gReplayEditor.freecamFov = THE_CAMERA.fov;
                REPLAY_PLAYER.cameraController().setMode("freecam");
                Debug::log(Debug::Category::Replay,
                    "[replay_freecam] enabled, prev mode=%s\n",
                    gReplayEditor.mPrevCameraMode.c_str());
            } else if (!enable && gReplayEditor.freecam) {
                std::string restore = gReplayEditor.mPrevCameraMode.empty() ? "tp" : gReplayEditor.mPrevCameraMode;
                REPLAY_PLAYER.cameraController().setMode(restore);
                gReplayEditor.freecam = false;
                gReplayEditor.mPrevCameraMode.clear();
            }
            Terminal::instance().addLog(std::string("[RPLE] Free camera: ") +
                (gReplayEditor.freecam ? "ON" : "OFF"));
        }
    });

    t.registerCommand({
        "replay_keyframe",
        "Alias for rplefc_skf: create camera keyframe at current tick",
        "replay_keyframe",
        [](const std::vector<std::string>&) {
            if (!gReplayEditor.isLoaded()) { requireEditor("replay_keyframe"); return; }
            uint32_t tick = REPLAY_PLAYER.currentTick();
            glm::vec3 pos = gReplayEditor.freecam ? gReplayEditor.freecamPos : THE_CAMERA.pos;
            glm::quat rot = gReplayEditor.freecam ? gReplayEditor.freecamRot
                : eulerToQuat(THE_CAMERA.yaw, THE_CAMERA.pitch);
            gReplayEditor.addCameraKeyframe((int)tick, pos, rot,
                gReplayEditor.freecamRoll, gReplayEditor.freecamFov, gReplayEditor.defaultInterp);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[RPLE] Camera keyframe at tick %u (%.1f %.1f %.1f)",
                tick, pos.x, pos.y, pos.z);
            Terminal::instance().addLog(buf);
        }
    });

    t.registerCommand({
        "replay_info",
        "Alias for rpleinfo: show editor info",
        "replay_info",
        [](const std::vector<std::string>&) {
            if (!gReplayEditor.isLoaded()) { requireEditor("replay_info"); return; }
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[RPLE] Replay: %s\n"
                "  Ticks: %d  Rate: %d  Duration: %.1fs\n"
                "  Camera keyframes: %d  Time keyframes: %d  Bookmarks: %d\n"
                "  Freecam: %s  Playing: %s  Tick: %.2f\n"
                "  Edit file: %s",
                gReplayEditor.replayPath().c_str(),
                gReplayEditor.totalTicks(), gReplayEditor.tickRate(), gReplayEditor.durationSec(),
                gReplayEditor.cameraKeyframeCount(), gReplayEditor.timeKeyframeCount(), gReplayEditor.bookmarkCount(),
                gReplayEditor.freecam ? "ON" : "OFF",
                gReplayEditor.playing ? "YES" : "NO",
                gReplayEditor.movieTick,
                gReplayEditor.editPath().c_str());
            Terminal::instance().addLog(buf);
        }
    });

    // ── rple_mi: import music ────────────────────────────
    t.registerCommand({
        "rple_mi",
        "Import music for replay editor timeline",
        "rple_mi <path>",
        [](const std::vector<std::string>& args) {
            if (!gReplayEditor.isLoaded()) { requireEditor("rple_mi"); return; }
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rple_mi <path to audio file>");
                return;
            }
            std::string path = args[0];
            for (size_t i = 1; i < args.size(); ++i)
                path += " " + args[i];
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                path = path.substr(1, path.size() - 2);
            if (!std::filesystem::exists(path)) {
                Terminal::instance().addLog("[ERROR] File not found: " + path);
                return;
            }
            std::string ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav" && ext != ".mp3" && ext != ".ogg" && ext != ".flac" &&
                ext != ".m4a" && ext != ".aac") {
                Terminal::instance().addLog("[ERROR] Unsupported format: " + ext);
                return;
            }

            // Get audio file duration and info
            float duration = getAudioFileDuration(path);
            std::vector<int16_t> pcm;
            uint32_t sampleRate = 0, channels = 0;
            decodeAudioToPCM(path, pcm, sampleRate, channels, 48000, 2);

            int totalSec = (int)duration;
            int h = totalSec / 3600;
            int m = (totalSec % 3600) / 60;
            double sec = totalSec % 60 + (double)((int)(duration * 1000.0) % 1000) / 1000.0;

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[ReplayMusic] loaded:\n"
                "  path=%s\n"
                "  length=%02d:%02d:%06.3f\n"
                "  sample_rate=%u\n"
                "  channels=%u",
                path.c_str(), h, m, sec, sampleRate, channels);
            Terminal::instance().addLog(buf);

            // Set music metadata with defaults
            ReplayEditorMusic* music = gReplayEditor.mutableMusic();
            music->path = path;
            music->offsetSeconds = 0.0;
            music->cropStartSeconds = 0.0;
            music->cropEndSeconds = (double)duration;
            music->speedMultiplier = 1.0;
            music->volume = 1.0;

            // Backward compat: also set legacy audio track
            gReplayEditor.setAudioTrack(path, 1.0f);

            // Start preview at default position, then pause
            stopReplayMusicPreview();
            if (playReplayMusicPreview(path, 1.0f)) {
                seekReplayMusicPreview(0.0f);
                setReplayMusicPreviewSpeed(1.0f);
                setReplayMusicPreviewVolume(1.0f);
                pauseReplayMusicPreview();
            }

            gReplayEditor.autosave();
            Debug::log(Debug::Category::Replay, "[ReplayMusic] imported: path=%s length=%.2f\n",
                       path.c_str(), duration);
        }
    });

    // ── rple_mo: music offset ────────────────────────────
    t.registerCommand({
        "rple_mo",
        "Set music playback offset — song start position at replay tick 0",
        "rple_mo <hh:mm:ss>",
        [](const std::vector<std::string>& args) {
            if (!gReplayEditor.isLoaded()) { requireEditor("rple_mo"); return; }
            ReplayEditorMusic* music = gReplayEditor.mutableMusic();
            if (music->path.empty()) {
                Terminal::instance().addLog("[ERROR] No music imported. Use rple_mi first.");
                return;
            }

            if (args.empty()) {
                float dur = getReplayMusicPreviewDuration();
                if (dur <= 0.0f) dur = (float)music->cropEndSeconds;
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "[ReplayMusic] Current offset: %.3f sec\n"
                    "  Song length: %.3f sec\n"
                    "  Usage: rple_mo hh:mm:ss",
                    music->offsetSeconds, dur);
                Terminal::instance().addLog(buf);
                return;
            }

            std::string timeStr = args[0];
            int hh = 0, mm = 0;
            double ss = 0.0;
            int parsed = std::sscanf(timeStr.c_str(), "%d:%d:%lf", &hh, &mm, &ss);
            if (parsed < 2) {
                Terminal::instance().addLog("[ERROR] Invalid format. Use hh:mm:ss");
                return;
            }
            if (parsed == 2) {
                ss = (double)mm;
                mm = hh;
                hh = 0;
            }
            double offsetSec = (double)hh * 3600.0 + (double)mm * 60.0 + ss;
            if (offsetSec < 0.0) {
                Terminal::instance().addLog("[ERROR] Offset cannot be negative");
                return;
            }

            float dur = getReplayMusicPreviewDuration();
            if (dur > 0.0f && offsetSec > (double)dur) {
                Terminal::instance().addLog("[ERROR] Offset exceeds song length");
                return;
            }

            // Immediately apply offset and start playback from that position
            music->offsetSeconds = offsetSec;

            double songPos = music->offsetSeconds + music->cropStartSeconds;
            seekReplayMusicPreview((float)songPos);
            setReplayMusicPreviewSpeed((float)music->speedMultiplier);
            setReplayMusicPreviewVolume((float)music->volume);
            resumeReplayMusicPreview();
            gReplayEditor.playing = true;

            int h = (int)offsetSec / 3600;
            int m2 = ((int)offsetSec % 3600) / 60;
            double s = offsetSec - (double)((int)offsetSec) + (double)((int)offsetSec % 60);
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[ReplayMusic] offset set to %02d:%02d:%06.3f — playing from song time %.2f",
                h, m2, s, songPos);
            Terminal::instance().addLog(buf);
            Debug::log(Debug::Category::Replay, "[ReplayMusic] offset=%.2f\n", music->offsetSeconds);

            gReplayEditor.autosave();
        }
    });

    // ── rple_mv: music volume ────────────────────────────
    t.registerCommand({
        "rple_mv",
        "Set music volume (0=mute, 0.01-10, default 1.0)",
        "rple_mv <volume>",
        [](const std::vector<std::string>& args) {
            if (!gReplayEditor.isLoaded()) { requireEditor("rple_mv"); return; }
            ReplayEditorMusic* music = gReplayEditor.mutableMusic();
            if (music->path.empty()) {
                Terminal::instance().addLog("[ERROR] No music imported. Use rple_mi first.");
                return;
            }
            if (args.empty()) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[ReplayMusic] Current volume: %.2f", music->volume);
                Terminal::instance().addLog(buf);
                return;
            }
            char* end = nullptr;
            double val = std::strtod(args[0].c_str(), &end);
            if (end == args[0].c_str() || !std::isfinite(val)) {
                Terminal::instance().addLog("[ERROR] Invalid volume. Use a number (0-10).");
                return;
            }
            val = std::clamp(val, 0.0, 10.0);
            music->volume = val;
            setReplayMusicPreviewVolume((float)val);
            gReplayEditor.autosave();
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[ReplayMusic] volume=%.2f", val);
            Terminal::instance().addLog(buf);
            Debug::log(Debug::Category::Replay, "[ReplayMusic] volume=%.2f\n", val);
        }
    });

    // ── rple_mpbs: music playback speed multiplier ───────
    t.registerCommand({
        "rple_mpbs",
        "Set music playback speed multiplier (0.01-100, default 1.0)",
        "rple_mpbs <speed>",
        [](const std::vector<std::string>& args) {
            if (!gReplayEditor.isLoaded()) { requireEditor("rple_mpbs"); return; }
            if (args.empty()) {
                ReplayEditorMusic* music = gReplayEditor.mutableMusic();
                char buf[128];
                std::snprintf(buf, sizeof(buf), "[ReplayMusic] Current speed multiplier: %.2f", music->speedMultiplier);
                Terminal::instance().addLog(buf);
                return;
            }
            char* end = nullptr;
            double val = std::strtod(args[0].c_str(), &end);
            if (end == args[0].c_str() || !std::isfinite(val) || val <= 0.0) {
                Terminal::instance().addLog("[ERROR] Invalid speed. Use a positive number (0.01-100).");
                return;
            }
            val = std::clamp(val, 0.01, 100.0);
            ReplayEditorMusic* music = gReplayEditor.mutableMusic();
            music->speedMultiplier = val;
            gReplayEditor.autosave();
            char buf[128];
            std::snprintf(buf, sizeof(buf), "[ReplayMusic] speed_multiplier=%.2f", val);
            Terminal::instance().addLog(buf);
            Debug::log(Debug::Category::Replay, "[ReplayMusic] speed_multiplier=%.2f\n", val);
        }
    });

    // ── rple_mc: music crop ──────────────────────────────
    t.registerCommand({
        "rple_mc",
        "Set music crop range (hh:mm:ss-hh:mm:ss)",
        "rple_mc <start-end>",
        [](const std::vector<std::string>& args) {
            if (!gReplayEditor.isLoaded()) { requireEditor("rple_mc"); return; }
            ReplayEditorMusic* music = gReplayEditor.mutableMusic();
            if (music->path.empty()) {
                Terminal::instance().addLog("[ERROR] No music imported. Use rple_mi first.");
                return;
            }
            if (args.empty()) {
                float dur = getReplayMusicPreviewDuration();
                if (dur <= 0.0f) dur = (float)music->cropEndSeconds;
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "[ReplayMusic] Song length: %.3f sec\n"
                    "  Current crop: %.3f - %.3f sec\n"
                    "  Usage: rple_mc hh:mm:ss-hh:mm:ss",
                    dur, music->cropStartSeconds, music->cropEndSeconds);
                Terminal::instance().addLog(buf);
                return;
            }

            std::string range = args[0];
            auto dashPos = range.find('-');
            if (dashPos == std::string::npos) {
                Terminal::instance().addLog("[ERROR] Invalid format. Use hh:mm:ss-hh:mm:ss");
                return;
            }
            std::string startStr = range.substr(0, dashPos);
            std::string endStr = range.substr(dashPos + 1);

            auto parseTime = [](const std::string& s) -> double {
                int hh = 0, mm = 0;
                double ss = 0.0;
                int n = std::sscanf(s.c_str(), "%d:%d:%lf", &hh, &mm, &ss);
                if (n == 2) { ss = (double)mm; mm = hh; hh = 0; n = std::sscanf(s.c_str(), "%d:%lf", &mm, &ss); }
                if (n < 1) return -1.0;
                return (double)hh * 3600.0 + (double)mm * 60.0 + ss;
            };

            double startSec = parseTime(startStr);
            double endSec = parseTime(endStr);
            if (startSec < 0.0 || endSec < 0.0) {
                Terminal::instance().addLog("[ERROR] Invalid time format. Use hh:mm:ss-hh:mm:ss");
                return;
            }

            float dur = getReplayMusicPreviewDuration();
            if (dur <= 0.0f) dur = (float)music->cropEndSeconds;
            if (startSec >= endSec) {
                Terminal::instance().addLog("[ERROR] crop_start must be before crop_end");
                return;
            }
            if (endSec > (double)dur) {
                Terminal::instance().addLog("[ERROR] crop_end exceeds song length");
                return;
            }

            music->cropStartSeconds = startSec;
            music->cropEndSeconds = endSec;
            gReplayEditor.autosave();

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[ReplayMusic] crop=%.2f -> %.2f", startSec, endSec);
            Terminal::instance().addLog(buf);
            Debug::log(Debug::Category::Replay, "[ReplayMusic] crop=%.2f -> %.2f\n", startSec, endSec);
        }
    });

    Debug::log(Debug::Category::Replay, "[RPLE] Editor commands registered\n");
}
