// 08 16 2026, 01 35
/* purpose
* Registers replay export diagnostics, backend settings, and clip commands.
* Connects the instant replay recorder to background MP4 export and Explorer.
* Keeps the existing replay editor factory command independent from clip export.
* Does NOT capture frames, encode video, or own replay playback state.
* Does NOT hardcode encoder installation paths or bundle FFmpeg.
* Does NOT register global key bindings directly.
*/
#include "terminal/replay-commands.h"
#include "terminal/terminal-state.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "replay/replay-export.h"
#include "replay/replay-editor.h"
#include "replay/replay-factory.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "notifications/notifications.h"

#define CMDTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)

void registerReplayExportCommands()
{
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
                std::string("[FFMPEG DEBUG] ") + (isFfmpegDebugMode() ? "ON (visible cmd window)" : "OFF (background process)"));
        }
    });

#ifndef NDEBUG
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
#endif

#ifndef NDEBUG
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
#endif

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

            ReplayClip clip;
            if (!clip.load(path)) {
                logLine("FAIL: Cannot load clip");
                CMDTRACE("FAIL: Cannot load clip");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: cannot load clip");
                return;
            }
            logLine("Clip loaded: ticks=%u duration=%.1f", clip.header.tickCount, (float)clip.header.tickCount / 60.0f);

            std::string ffmpeg = defaultFfmpegPath();
            if (!std::filesystem::exists(ffmpeg)) {
                logLine("FAIL: ffmpeg not found at %s", ffmpeg.c_str());
                CMDTRACE("FAIL: ffmpeg not found");
                fclose(log);
                Terminal::instance().addLog("[DIAGNOSE] FAILED: ffmpeg not found");
                return;
            }
            logLine("ffmpeg found: %s", ffmpeg.c_str());

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

            logLine("Rendering 10 test frames...");
            for (int i = 0; i < 10; i++) {
                const ReplayExportJob& job = getReplayExportJob();
                logLine("Frame %d: state=%d capturedTicks=%u totalTicks=%u",
                        i, (int)job.state, job.capturedTicks, job.totalTicks);
                if (job.state != ReplayExportJob::Capturing)
                    break;
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
            printf("[RPLX] command received\n");

            std::string path;

            // Priority 1: Active replay editor session
            if (gReplayEditor.isLoaded() && !gReplayEditor.replayPath().empty()) {
                path = gReplayEditor.replayPath();
                printf("[RPLX] using active replay editor replay: %s\n", path.c_str());
                printf("[RPLX] activeEditorProjectPath=%s\n", gReplayEditor.editPath().c_str());
                gReplayEditor.autosave();
                printf("[RPLX] autosaved editor project\n");
            }

            // Priority 2: Replay editor session file
            if (path.empty() && ReplayEditor::hasSession()) {
                std::ifstream f(ReplayEditor::sessionPath());
                nlohmann::json j;
                if (f.is_open()) {
                    try { f >> j; } catch (...) {}
                }
                std::string sessionPath = j.value("lastReplay", "");
                if (!sessionPath.empty() && std::filesystem::exists(sessionPath)) {
                    path = sessionPath;
                    printf("[RPLX] using session replay: %s\n", path.c_str());
                }
            }

            // Priority 3: Newest valid replay clip
            if (path.empty()) {
                std::vector<std::string> clips = listReplayClips();
                CMDTRACE("listReplayClips returned %zu clips", clips.size());
                if (clips.empty()) {
                    Terminal::instance().addLog("[ERROR] No replays found");
                    printf("[RPLX] FAILED: no replay clips available\n");
                    return;
                }
                path = clips.front();
                printf("[RPLX] using newest replay clip: %s\n", path.c_str());
            }

            printf("[RPLX] calling startReplayExport(\"%s\", 1280, 720)\n", path.c_str());
            bool result = startReplayExport(path, 1280, 720);
            CMDTRACE("startReplayExport returned %d", (int)result);
            printf("[RPLX] startReplayExport returned %d\n", (int)result);
            if (result) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
                printf("[RPLX] FAILED: startReplayExport returned false\n");
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
            std::string path = clips.front().path;
            Terminal::instance().addLog("[REPLAY EXPORT] exporting final kill: " + path);
            if (startReplayExport(path, 1280, 720)) {
                Terminal::instance().addLog("[REPLAY EXPORT] started: " + path);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to start export");
            }
        }
    });

    // ── Export config commands ──────────────────────────────
    Terminal::instance().registerCommand({
        "rplfx", "Export the latest 15 seconds to MP4 without opening the editor", "rplfx",
        [](const std::vector<std::string>&) {
            if (isReplayExportActive()) {
                Terminal::instance().addLog("Clip is already exporting...");
                NotificationSystem::instance().push(
                    "CLIP EXPORT", "Clip is already exporting...", 180, {});
                return;
            }
            if (!REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("No replay recording active");
                return;
            }
            // Ensure the clip records the CURRENT map, not whatever map was
            // active when replay.record was last called.
            {
                ReplayWorldMetadata wm;
                wm.mapPath = ACTIVE_MAP_PATH;
                REPLAY_RECORDER.setWorldMetadata(wm);
            }
            std::string path = saveInstantReplay(REPLAY_RECORDER, 15);
            if (path.empty()) {
                NotificationSystem::instance().pushCritical(
                    "CLIP EXPORT FAILED", "Clip export failed. Check logs.", 600);
                return;
            }
            if (!startReplayExport(path, 1280, 720, true)) {
                NotificationSystem::instance().pushCritical(
                    "CLIP EXPORT FAILED", "Clip export failed. Check logs.", 600);
                return;
            }
            NotificationSystem::instance().pushImportant(
                "EXPORTING CLIP", "Exporting the latest 15 seconds...", 180);
            Debug::warn(Debug::Category::Replay,
                        "[CLIP EXPORT] source=%s windowTicks=%u output=%s",
                        path.c_str(), 15u * ReplayRingBuffer::TickRate,
                        getReplayExportResultPath().c_str());
        },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "replay_open_last_export", "Select the newest exported clip in Explorer",
        "replay_open_last_export",
        [](const std::vector<std::string>&) { openLastReplayExport(); },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "replay_export_verbose", "Toggle per-frame export debug logs (0=off, 1=on)", "replay_export_verbose <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(std::string("[EXPORT VERBOSE] ") +
                    (gReplayExportVerbose ? "ON" : "OFF"));
                return;
            }
            gReplayExportVerbose = args[0] != "0";
            Terminal::instance().addLog(std::string("[EXPORT VERBOSE] ") +
                (gReplayExportVerbose ? "ON" : "OFF"));
        },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "replay_export_timing", "Print export timing buckets from the last export", "replay_export_timing",
        [](const std::vector<std::string>&) {
            if (gExportTimingFrames == 0) {
                Terminal::instance().addLog("[EXPORT TIMING] no export timing data yet");
                return;
            }
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[EXPORT TIMING] frames=%u avg(total=%.1f seek=%.1f update=%.1f weapon=%.1f "
                "audio=%.1f render=%.1f read=%.1f copy=%.1f enc=%.1f wait=%.1f)ms",
                gExportTimingFrames,
                gExportTimingTotals.totalMs / gExportTimingFrames,
                gExportTimingTotals.seekMs / gExportTimingFrames,
                gExportTimingTotals.updateMs / gExportTimingFrames,
                gExportTimingTotals.weaponEventsMs / gExportTimingFrames,
                gExportTimingTotals.audioEventsMs / gExportTimingFrames,
                gExportTimingTotals.renderMs / gExportTimingFrames,
                gExportTimingTotals.readPixelsMs / gExportTimingFrames,
                gExportTimingTotals.copyMs / gExportTimingFrames,
                gExportTimingTotals.encoderMs / gExportTimingFrames,
                gExportTimingTotals.waitMs / gExportTimingFrames);
            Terminal::instance().addLog(buf);
        },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "export_mf_diag", "List installed H.264 encoders and current encoder mode", "export_mf_diag",
        [](const std::vector<std::string>&) { exportMfDiag(); },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "export_config",
        "Show or set export config (resolution, CRF, bitrate, encoderMode). Use: export_config <key> <value>",
        "export_config [width|height|crf|bitrate|volume|encoderMode|encoder] <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "[EXPORT CONFIG] encoder=%s encoderMode=%s %dx%d CRF=%d bitrate=%dK volume=%.2f",
                    gExportConfig.encoder.c_str(), gExportConfig.encoderMode.c_str(),
                    gExportConfig.exportWidth, gExportConfig.exportHeight,
                    gExportConfig.exportCrf, gExportConfig.exportBitrate,
                    gExportConfig.audioVolumeMultiplier);
                Terminal::instance().addLog(buf);
                return;
            }
            if (args.size() < 2) {
                Terminal::instance().addLog("[ERROR] Usage: export_config <key> <value>");
                return;
            }
            const std::string& key = args[0];
            if (key == "encoderMode") {
                gExportConfig.encoderMode = args[1];
                if (gExportConfig.encoderMode != "auto" && gExportConfig.encoderMode != "discrete" &&
                    gExportConfig.encoderMode != "software")
                    gExportConfig.encoderMode = "auto";
                Terminal::instance().addLog("[EXPORT CONFIG] encoderMode=" + gExportConfig.encoderMode);
            } else {
                int val = std::stoi(args[1]);
                if (key == "width") {
                    gExportConfig.exportWidth = std::max(320, std::min(val, 7680));
                    Terminal::instance().addLog("[EXPORT CONFIG] width=" + std::to_string(gExportConfig.exportWidth));
                } else if (key == "height") {
                    gExportConfig.exportHeight = std::max(240, std::min(val, 4320));
                    Terminal::instance().addLog("[EXPORT CONFIG] height=" + std::to_string(gExportConfig.exportHeight));
                } else if (key == "crf") {
                    gExportConfig.exportCrf = std::max(0, std::min(val, 51));
                    Terminal::instance().addLog("[EXPORT CONFIG] crf=" + std::to_string(gExportConfig.exportCrf));
                } else if (key == "bitrate") {
                    gExportConfig.exportBitrate = std::max(0, val);
                    Terminal::instance().addLog("[EXPORT CONFIG] bitrate=" + std::to_string(gExportConfig.exportBitrate) + "K");
                } else if (key == "volume") {
                    gExportConfig.audioVolumeMultiplier = std::max(0.0f, std::min((float)val / 100.0f, 5.0f));
                    Terminal::instance().addLog("[EXPORT CONFIG] volume=" + std::to_string(gExportConfig.audioVolumeMultiplier));
                } else {
                    Terminal::instance().addLog("[ERROR] Unknown key: " + key + " (use width, height, crf, bitrate, volume, encoderMode)");
                }
            }
            // Write config to disk
            nlohmann::json j;
            j["exportWidth"] = gExportConfig.exportWidth;
            j["exportHeight"] = gExportConfig.exportHeight;
            j["exportCrf"] = gExportConfig.exportCrf;
            j["exportBitrate"] = gExportConfig.exportBitrate;
            j["audioVolumeMultiplier"] = gExportConfig.audioVolumeMultiplier;
            j["encoder"] = gExportConfig.encoder;
            j["encoderMode"] = gExportConfig.encoderMode;
            std::ofstream f("config/replay/replay-export.json");
            if (f.is_open()) {
                f << j.dump(2);
                f.close();
                Terminal::instance().addLog("[EXPORT CONFIG] saved to config/replay/replay-export.json");
            }
        }
    });
}
