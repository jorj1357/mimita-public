#include "terminal/replay-commands.h"
#include "terminal/terminal-state.h"

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
#include "debug/debug-log.h"

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
}
