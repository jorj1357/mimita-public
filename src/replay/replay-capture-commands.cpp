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

#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"

// ============================================================
// replay.save - Save an instant replay (F3)
// ============================================================
static void doSaveInstantReplay()
{
    if (!REPLAY_RECORDER.isRecording()) {
        Terminal::instance().addLog("[REPLAY] No active recording to save");
        return;
    }

    std::string path = saveInstantReplay(REPLAY_RECORDER, 15);
    if (path.empty()) {
        Terminal::instance().addLog("[ERROR] Failed to save instant replay");
        return;
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf), "[REPLAY] Instant replay saved: %s", path.c_str());
    Terminal::instance().addLog(buf);
    Debug::log(Debug::Category::Replay, "[REPLAY] Instant replay saved: %s", path.c_str());
}

// ============================================================
// rplx list - List 30 most recent replay files
// ============================================================
static void doReplayList()
{
    std::vector<std::string> clips = listReplayClips();
    if (clips.empty()) {
        Terminal::instance().addLog("[REPLAY] No replay files found");
        return;
    }

    int count = std::min((size_t)30, clips.size());
    char buf[256];
    std::snprintf(buf, sizeof(buf), "Replay Library (%d most recent)", count);
    Terminal::instance().addLog(buf);
    Terminal::instance().addLog("");

    for (int i = 0; i < count; i++) {
        std::filesystem::path p(clips[i]);
        std::string filename = p.filename().string();

        // Extract date from parent directory name (MM-DD-YYYY)
        std::string dateStr = p.parent_path().filename().string();
        // Extract time from filename prefix (HH-MM-SS-replay.json)
        std::string timeStr;
        for (size_t ci = 0; ci < filename.size() && filename[ci] != '-'; ci++)
            timeStr += filename[ci];
        // Build readable time
        char timeBuf[32] = "??";
        if (filename.size() >= 8) {
            int h = 0, m = 0, s = 0;
            if (std::sscanf(filename.c_str(), "%d-%d-%d", &h, &m, &s) >= 2)
                std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", h, m, s);
        }

        char line[128];
        if (dateStr.size() == 10 && timeBuf[0] != '?') {
            std::snprintf(line, sizeof(line), "%d  %s %s", i + 1, dateStr.c_str(), timeBuf);
            Terminal::instance().addLog(line);
            std::snprintf(line, sizeof(line), "   %s", filename.c_str());
        } else {
            std::snprintf(line, sizeof(line), "%d  %s", i + 1, filename.c_str());
        }
        Terminal::instance().addLog(line);
    }
}

// ============================================================
// rplx <count> / rplx all - Batch export
// ============================================================
static int gBatchExportTotal = 0;
static int gBatchExportCurrent = 0;
static std::vector<std::string> gBatchExportQueue;
static bool gBatchExportActive = false;

static void startNextBatchExport();
static void onBatchExportComplete();

static void doBatchExport(int count)
{
    if (isReplayExportActive()) {
        Terminal::instance().addLog("[ERROR] Export already in progress");
        return;
    }

    std::vector<std::string> clips = listReplayClips();
    if (clips.empty()) {
        Terminal::instance().addLog("[ERROR] No replay files to export");
        return;
    }

    int actual = std::min(count, (int)clips.size());
    gBatchExportQueue.clear();
    for (int i = 0; i < actual; i++)
        gBatchExportQueue.push_back(clips[i]);

    gBatchExportTotal = actual;
    gBatchExportCurrent = 0;
    gBatchExportActive = true;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "[REPLAY] Batch export: %d replay(s) queued", actual);
    Terminal::instance().addLog(buf);

    startNextBatchExport();
}

static void startNextBatchExport()
{
    if (!gBatchExportActive || gBatchExportCurrent >= (int)gBatchExportQueue.size()) {
        onBatchExportComplete();
        return;
    }

    if (isReplayExportActive()) {
        return;
    }

    const std::string& path = gBatchExportQueue[gBatchExportCurrent];

    char buf[128];
    std::snprintf(buf, sizeof(buf), "[REPLAY] Exporting %d/%d", gBatchExportCurrent + 1, gBatchExportTotal);
    Terminal::instance().addLog(buf);

    if (!startReplayExport(path, 1280, 720)) {
        Debug::log(Debug::Category::Replay, "[REPLAY] Batch export failed for: %s", path.c_str());
        gBatchExportCurrent++;
        startNextBatchExport();
    }
}

void updateReplayBatchExport()
{
    if (!gBatchExportActive)
        return;
    if (gBatchExportCurrent >= (int)gBatchExportQueue.size()) {
        onBatchExportComplete();
        return;
    }
    if (isReplayExportActive())
        return;
    startNextBatchExport();
}

static void onBatchExportComplete()
{
    gBatchExportActive = false;
    gBatchExportQueue.clear();

    char buf[128];
    std::snprintf(buf, sizeof(buf), "[REPLAY] Batch export complete: %d/%d exported",
                  gBatchExportCurrent, gBatchExportTotal);
    Terminal::instance().addLog(buf);
    Debug::log(Debug::Category::Replay, "[REPLAY] Batch export complete: %d/%d",
               gBatchExportCurrent, gBatchExportTotal);
}

bool isReplayBatchExportActive()
{
    return gBatchExportActive;
}

// ============================================================
// Registration
// ============================================================

void registerReplayCaptureCommands()
{
    Terminal::instance().registerCommand({
        "replay.save", "Save an instant replay (last 15 seconds)", "replay.save",
        [](const std::vector<std::string>&) {
            doSaveInstantReplay();
        },
        std::string(), CommandCategory::Replay
    });

    Terminal::instance().registerCommand({
        "rplx", "Replay export: list, <count>, or all", "rplx [list|<count>|all]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[USAGE] rplx list  - show replay files");
                Terminal::instance().addLog("[USAGE] rplx <N>   - export newest N replays");
                Terminal::instance().addLog("[USAGE] rplx all   - export all replays");
                return;
            }

            if (args[0] == "list") {
                doReplayList();
            } else if (args[0] == "all") {
                std::vector<std::string> clips = listReplayClips();
                if (clips.empty()) {
                    Terminal::instance().addLog("[ERROR] No replay files to export");
                    return;
                }
                doBatchExport((int)clips.size());
            } else {
                char* end = nullptr;
                long n = std::strtol(args[0].c_str(), &end, 10);
                if (end == args[0].c_str() || n < 1) {
                    Terminal::instance().addLog("[ERROR] Usage: rplx [list|<count>|all]");
                    return;
                }
                doBatchExport((int)n);
            }
        },
        std::string(), CommandCategory::Replay, {"replay.batch"}
    });
}
