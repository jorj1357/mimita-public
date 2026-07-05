#include "replay-editor.h"
#include "replay.h"
#include "devtools/terminal.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <glm/glm.hpp>
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
    glm::quat qYaw = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat qPitch = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    return qYaw * qPitch;
}

// ── Command implementations ─────────────────────────────────

void registerReplayEditorCommands() {
    auto& t = Terminal::instance();

    // ── rple: load newest replay into editor ─────────────
    t.registerCommand({
        "rple",
        "Load the newest replay into the editor",
        "rple",
        [](const std::vector<std::string>&) {
            auto clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            std::string path = clips[0];
            if (!E.load(path)) {
                Terminal::instance().addLog("[ERROR] Failed to load: " + path);
                return;
            }
            // Set totalTicks from the loaded replay player
            if (REPLAY_PLAYER.totalTicks() > 0) {
                // We won't modify totalTicks here; the existing player has it.
                // Just use the editor as an overlay.
            }
            Terminal::instance().addLog("[RPLE] Editor loaded: " + path);
            Debug::log(Debug::Category::Replay, "[RPLE] Editor loaded: %s\n", path.c_str());
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

    // ── rpleinfo ─────────────────────────────────────────
    t.registerCommand({
        "rpleinfo",
        "Show editor info: replay, keyframes, bookmarks",
        "rpleinfo",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rpleinfo"); return; }
            char buf[512];
            int camKf = E.cameraKeyframeCount();
            int timeKf = E.timeKeyframeCount();
            int marks = E.bookmarkCount();
            std::snprintf(buf, sizeof(buf),
                "[RPLE] Replay: %s\n"
                "  Ticks: %d  Rate: %d  Duration: %.1fs\n"
                "  Camera keyframes: %d  Time keyframes: %d  Bookmarks: %d\n"
                "  Freecam: %s  Playing: %s  Tick: %.2f\n"
                "  Edit file: %s",
                E.replayPath().c_str(),
                E.totalTicks(), E.tickRate(), E.durationSec(),
                camKf, timeKf, marks,
                E.freecam ? "ON" : "OFF",
                E.playing ? "YES" : "NO",
                E.movieTick,
                E.editPath().c_str());
            Terminal::instance().addLog(buf);
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
            float speed = args.empty() ? 1.0f : std::max(0.01f, std::stof(args[0]));
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
            std::vector<ReplayKillfeedEvent> kills = REPLAY_PLAYER.takeTriggeredKillfeedEvents();
            // Re-trigger — they were consumed, but we can list from the clip
            // For now, just log what's available
            auto sounds = REPLAY_PLAYER.takeTriggeredSounds(); (void)sounds;
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
        "Save .rpledit edit file",
        "rplesave",
        [](const std::vector<std::string>&) {
            if (!E.isLoaded()) { requireEditor("rplesave"); return; }
            if (E.saveEdit())
                Terminal::instance().addLog("[RPLE] Saved: " + E.editPath());
            else
                Terminal::instance().addLog("[ERROR] Failed to save edit file");
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

    Debug::log(Debug::Category::Replay, "[RPLE] Editor commands registered\n");
}
