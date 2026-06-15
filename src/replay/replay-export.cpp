#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "replay/replay.h"
#include "debug/debug-log.h"

static ReplayExportJob gJob;

// Debug flag: when true, launches ffmpeg in a visible cmd window instead of _popen
static bool gFfmpegDebugMode = false;

#define EXPORTTRACE(fmt, ...) do { printf("[EXPORTTRACE] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#define EXPORTLOG(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)

void setFfmpegDebugMode(bool enabled)
{
    gFfmpegDebugMode = enabled;
    printf("[EXPORTTRACE] ffmpeg debug mode = %s\n", enabled ? "ON (visible cmd window)" : "OFF (_popen)");
    fflush(stdout);
}

bool isFfmpegDebugMode()
{
    return gFfmpegDebugMode;
}

static void debugLaunchFfmpegVisible(const std::string& cmd)
{
    // Start ffmpeg in a new visible cmd.exe window with /k (stays open after ffmpeg exits)
    // This lets us see ffmpeg's stdout/stderr directly
    std::string args = std::string("/k ") + cmd;
    HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", args.c_str(), NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        printf("[EXPORTTRACE] ShellExecuteA failed to open cmd window. error=%d\n", (int)(INT_PTR)h);
        fflush(stdout);
    } else {
        printf("[EXPORTTRACE] Launched ffmpeg in visible cmd window. Close the window when done debugging.\n");
        fflush(stdout);
    }
}

std::string defaultFfmpegPath()
{
    return "C:\\important\\ffmpeg-2025-11-17-git-e94439e49b-full_build\\bin\\ffmpeg.exe";
}

float ReplayExportJob::progress() const
{
    switch (state) {
    case Idle:    return 0.0f;
    case Capturing:
        return totalTicks > 0 ? (float)capturedTicks / (float)totalTicks : 0.0f;
    case Encoding: return 0.95f;
    case Done:    return 1.0f;
    case Failed:  return 0.0f;
    }
    return 0.0f;
}

static std::string sanitizeFilenameWindows(const std::string& name)
{
    const std::string invalidChars = "<>:\"/\\|?*\n\r\t";
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (invalidChars.find(c) == std::string::npos && (unsigned char)c >= 32)
            result += c;
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
        result.pop_back();
    if (result.empty())
        result = "replay";
    return result;
}

std::string generateExportOutputPath()
{
    namespace fs = std::filesystem;
    EXPORTTRACE("generateExportOutputPath entered");
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char dateDir[32];
    std::strftime(dateDir, sizeof(dateDir), "%m-%d-%Y", &localTime);

    char timeFile[64];
    std::strftime(timeFile, sizeof(timeFile), "%H-%M-%S-clip-duel.mp4", &localTime);

    const fs::path exportDir = fs::path("replays") / "exports" / dateDir;
    EXPORTTRACE("export dir=%s", exportDir.string().c_str());
    std::error_code ec;
    fs::create_directories(exportDir, ec);
    if (ec) {
        EXPORTTRACE("failed to create export dir: %s", ec.message().c_str());
    }

    std::string baseFile = timeFile;
    fs::path path = exportDir / baseFile;
    int attempt = 1;
    while (fs::exists(path, ec)) {
        std::string stem = baseFile;
        size_t dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        std::string numbered = stem + "_" + std::to_string(attempt) + ".mp4";
        path = exportDir / numbered;
        attempt++;
    }
    std::string result = path.string();
    EXPORTTRACE("output path=%s", result.c_str());
    return result;
}

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight)
{
    EXPORTTRACE("=== startReplayExport entered ===");
    EXPORTTRACE("jsonPath=%s renderWidth=%d renderHeight=%d", jsonPath.c_str(), renderWidth, renderHeight);
    EXPORTTRACE("gJob.state=%d (0=Idle)", (int)gJob.state);

    if (gJob.state != ReplayExportJob::Idle)
    {
        EXPORTTRACE("Export already in progress, ignoring");
        return false;
    }

    EXPORTTRACE("Checking jsonPath exists...");
    if (!std::filesystem::exists(jsonPath))
    {
        EXPORTTRACE("Replay file NOT FOUND: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }
    EXPORTTRACE("jsonPath exists OK");

    EXPORTTRACE("Loading replay clip...");
    ReplayClip clip;
    if (!clip.load(jsonPath))
    {
        EXPORTTRACE("Failed to load replay clip: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay clip:\n" + jsonPath;
        return false;
    }
    EXPORTTRACE("Replay clip loaded OK");

    uint32_t totalTicks = clip.header.tickCount;
    EXPORTTRACE("totalTicks=%u", totalTicks);
    if (totalTicks == 0)
    {
        EXPORTTRACE("Replay has zero frames");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay has no frames.";
        return false;
    }
    EXPORTTRACE("Frames to export: %u", totalTicks);

    EXPORTTRACE("Checking ffmpeg path...");
    std::string ffmpeg = defaultFfmpegPath();
    EXPORTTRACE("ffmpeg path=%s", ffmpeg.c_str());
    if (!std::filesystem::exists(ffmpeg))
    {
        EXPORTTRACE("FFmpeg NOT FOUND: %s", ffmpeg.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg not found:\n" + ffmpeg;
        return false;
    }
    EXPORTTRACE("ffmpeg exists OK");

    EXPORTTRACE("Generating output path...");
    std::string outputPath = generateExportOutputPath();
    EXPORTTRACE("outputPath=%s", outputPath.c_str());

    EXPORTTRACE("Validating output directory...");
    std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
    EXPORTTRACE("outDir=%s", outDir.string().c_str());
    if (!std::filesystem::exists(outDir))
    {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            EXPORTTRACE("failed to create output dir: %s", ec.message().c_str());
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Cannot create output directory:\n" + outDir.string();
            return false;
        }
        EXPORTTRACE("output dir created");
    }
    EXPORTTRACE("output dir OK");

    EXPORTTRACE("Building ffmpeg command...");
    namespace fs = std::filesystem;
    std::string nativeOutput = fs::path(outputPath).make_preferred().string();
    std::string cmd = "\"" + ffmpeg + "\" -y -f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(renderWidth) + "x" + std::to_string(renderHeight) + " "
        "-framerate 60 -i - -c:v libx264 -preset fast -pix_fmt yuv420p "
        "-crf 18 \"" + nativeOutput + "\"";

    EXPORTTRACE("=== EXACT FFMPEG COMMAND ===");
    EXPORTTRACE("%s", cmd.c_str());
    EXPORTTRACE("=== COMMAND ARGUMENTS ===");
    {
        // Print each argument separately for manual testing
        std::string a0 = "\"" + ffmpeg + "\"";
        std::string a1 = "-y";
        std::string a2 = "-f";
        std::string a3 = "rawvideo";
        std::string a4 = "-pixel_format";
        std::string a5 = "rgb24";
        std::string a6 = "-video_size";
        std::string a7 = std::to_string(renderWidth) + "x" + std::to_string(renderHeight);
        std::string a8 = "-framerate";
        std::string a9 = "60";
        std::string a10 = "-i";
        std::string a11 = "-";
        std::string a12 = "-c:v";
        std::string a13 = "libx264";
        std::string a14 = "-preset";
        std::string a15 = "fast";
        std::string a16 = "-pix_fmt";
        std::string a17 = "yuv420p";
        std::string a18 = "-crf";
        std::string a19 = "18";
        std::string a20 = "\"" + nativeOutput + "\"";
        EXPORTTRACE("  [0] %s", a0.c_str());
        EXPORTTRACE("  [1] %s", a1.c_str());
        EXPORTTRACE("  [2] %s", a2.c_str());
        EXPORTTRACE("  [3] %s", a3.c_str());
        EXPORTTRACE("  [4] %s", a4.c_str());
        EXPORTTRACE("  [5] %s", a5.c_str());
        EXPORTTRACE("  [6] %s", a6.c_str());
        EXPORTTRACE("  [7] %s", a7.c_str());
        EXPORTTRACE("  [8] %s", a8.c_str());
        EXPORTTRACE("  [9] %s", a9.c_str());
        EXPORTTRACE(" [10] %s", a10.c_str());
        EXPORTTRACE(" [11] %s", a11.c_str());
        EXPORTTRACE(" [12] %s", a12.c_str());
        EXPORTTRACE(" [13] %s", a13.c_str());
        EXPORTTRACE(" [14] %s", a14.c_str());
        EXPORTTRACE(" [15] %s", a15.c_str());
        EXPORTTRACE(" [16] %s", a16.c_str());
        EXPORTTRACE(" [17] %s", a17.c_str());
        EXPORTTRACE(" [18] %s", a18.c_str());
        EXPORTTRACE(" [19] %s", a19.c_str());
        EXPORTTRACE(" [20] %s", a20.c_str());
    }
    EXPORTTRACE("Working directory: %s", std::filesystem::current_path().string().c_str());

    if (gFfmpegDebugMode)
    {
        // DEBUG MODE: launch ffmpeg in a visible cmd window instead of _popen
        EXPORTTRACE("FFMPEG DEBUG MODE: launching visible cmd window...");
        debugLaunchFfmpegVisible(cmd);
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg launched in debug window. Check the cmd window for errors.";
        EXPORTTRACE("=== startReplayExport returning false (debug mode) ===");
        return false;
    }

    EXPORTTRACE("Calling _popen...");
    FILE* pipe = _popen(cmd.c_str(), "wb");
    EXPORTTRACE("_popen returned pipe=%p", (void*)pipe);
    if (!pipe)
    {
#ifdef _WIN32
        DWORD err = GetLastError();
        EXPORTTRACE("pipe open FAILED GetLastError=%lu errno=%d", (unsigned long)err, errno);
#else
        EXPORTTRACE("pipe open FAILED errno=%d", errno);
#endif
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to launch FFmpeg.";
        return false;
    }
    EXPORTTRACE("pipe open OK");

    EXPORTTRACE("Initializing job state...");
    gJob.state = ReplayExportJob::Capturing;
    gJob.jsonPath = jsonPath;
    gJob.totalTicks = totalTicks;
    gJob.capturedTicks = 0;
    gJob.capWidth = renderWidth;
    gJob.capHeight = renderHeight;
    gJob.ffmpegPath = ffmpeg;
    gJob.ffmpegPipe = pipe;
    gJob.outputPath = outputPath;
    gJob.ffmpegExitCode = -1;
    gJob.errorMsg.clear();
    gJob.frameWriteCount = 0;
    EXPORTTRACE("gJob.state set to Capturing (1)");

    EXPORTTRACE("=== startReplayExport returning true ===");
    return true;
}

void updateReplayExport()
{
    if (gJob.state != ReplayExportJob::Capturing)
    {
        // EXPORTTRACE("updateReplayExport: state=%d not Capturing, return", (int)gJob.state);
        return;
    }

    int w = gJob.capWidth;
    int h = gJob.capHeight;
    uint32_t frameNum = gJob.capturedTicks;

    if (frameNum == 0)
        EXPORTTRACE("=== updateReplayExport: first frame ===");

    EXPORTTRACE("Frame %u/%u: allocating pixels buffer (%dx%d*3=%d bytes)",
                frameNum, gJob.totalTicks, w, h, w * h * 3);
    std::vector<uint8_t> pixels(w * h * 3);

    EXPORTTRACE("Frame %u: calling glReadPixels...", frameNum);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
        EXPORTTRACE("Frame %u: glReadPixels GL ERROR=0x%x", frameNum, glErr);
    else
        EXPORTTRACE("Frame %u: glReadPixels OK", frameNum);

    // Flip vertically
    std::vector<uint8_t> flipped(w * h * 3);
    for (int y = 0; y < h; ++y)
    {
        std::memcpy(
            &flipped[y * w * 3],
            &pixels[(h - 1 - y) * w * 3],
            w * 3);
    }
    EXPORTTRACE("Frame %u: flip done", frameNum);

    size_t expectedBytes = flipped.size();
    EXPORTTRACE("Frame %u: calling fwrite (%zu bytes to pipe)...", frameNum, expectedBytes);
    size_t written = fwrite(flipped.data(), 1, expectedBytes, gJob.ffmpegPipe);
    EXPORTTRACE("Frame %u: fwrite returned %zu (expected %zu)", frameNum, written, expectedBytes);

    if (written != expectedBytes)
    {
        int fwErr = ferror(gJob.ffmpegPipe);
        EXPORTTRACE("Frame %u: fwrite FAILED (wrote %zu/%zu) ferror=%d feof=%d",
                    frameNum, written, expectedBytes, fwErr, feof(gJob.ffmpegPipe));
#ifdef _WIN32
        DWORD lastErr = GetLastError();
        EXPORTTRACE("Frame %u: GetLastError=%lu", frameNum, (unsigned long)lastErr);
#endif
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Pipe write failed during frame capture.";
        EXPORTTRACE("Frame %u: calling _pclose on failed pipe...", frameNum);
        _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        EXPORTTRACE("Frame %u: pipe closed after write failure", frameNum);
        return;
    }

    gJob.capturedTicks++;
    gJob.frameWriteCount = gJob.capturedTicks;

    if (gJob.capturedTicks % 30 == 0 || gJob.capturedTicks == gJob.totalTicks)
    {
        float pct = (float)gJob.capturedTicks / (float)gJob.totalTicks * 100.0f;
        EXPORTTRACE("PROGRESS: %u/%u (%.1f%%)", gJob.capturedTicks, gJob.totalTicks, pct);
    }

    if (gJob.capturedTicks >= gJob.totalTicks)
    {
        EXPORTTRACE("=== ALL FRAMES WRITTEN (%u) ===", gJob.capturedTicks);

        EXPORTTRACE("Calling _pclose to finalize ffmpeg...");
        int ret = _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        gJob.ffmpegExitCode = ret;
        EXPORTTRACE("_pclose returned %d", ret);

        if (ret != 0)
        {
            EXPORTTRACE("FFMPEG EXIT CODE = %d (NON-ZERO)", ret);
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "FFmpeg exited with code " + std::to_string(ret);
            return;
        }

        EXPORTTRACE("FFMPEG EXIT CODE = 0 (OK)");
        EXPORTTRACE("Checking output file: %s", gJob.outputPath.c_str());

        if (!std::filesystem::exists(gJob.outputPath))
        {
            EXPORTTRACE("OUTPUT FILE MISSING");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file missing after encoding:\n" + gJob.outputPath;
            return;
        }

        uint64_t fileSize = std::filesystem::file_size(gJob.outputPath);
        EXPORTTRACE("output file size=%llu bytes", (unsigned long long)fileSize);
        if (fileSize == 0)
        {
            EXPORTTRACE("OUTPUT FILE EMPTY (0 bytes)");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file is empty:\n" + gJob.outputPath;
            return;
        }

        EXPORTTRACE("=== EXPORT COMPLETE ===");
        gJob.state = ReplayExportJob::Done;
    }
}

bool isReplayExportActive()
{
    return gJob.state == ReplayExportJob::Capturing ||
           gJob.state == ReplayExportJob::Encoding;
}

float getReplayExportProgress()
{
    return gJob.progress();
}

std::string getReplayExportResultPath()
{
    return gJob.outputPath;
}

std::string getReplayExportStatusText()
{
    switch (gJob.state)
    {
    case ReplayExportJob::Idle:   return "";
    case ReplayExportJob::Capturing:
    {
        float pct = gJob.progress() * 100.0f;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Exporting Replay...\nFrames: %u / %u (%.0f%%)",
                      gJob.capturedTicks, gJob.totalTicks, pct);
        return buf;
    }
    case ReplayExportJob::Encoding: return "Encoding MP4...";
    case ReplayExportJob::Done:
    {
        uint64_t size = std::filesystem::exists(gJob.outputPath)
            ? std::filesystem::file_size(gJob.outputPath) : 0;
        double durationSec = gJob.totalTicks / 60.0;
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Replay Saved!\nPath: %s\nDuration: %.0f sec\nFile Size: %.1f MB",
            gJob.outputPath.c_str(), durationSec, (double)size / (1024.0 * 1024.0));
        return buf;
    }
    case ReplayExportJob::Failed:
        return "Export Failed:\n" + gJob.errorMsg;
    }
    return "";
}

void cancelReplayExport()
{
    if (gJob.state == ReplayExportJob::Capturing && gJob.ffmpegPipe)
    {
        _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
    }
    gJob = ReplayExportJob{};
}

const ReplayExportJob& getReplayExportJob()
{
    return gJob;
}

void openReplayFolder()
{
    std::string path = "replays\\exports";
    std::string cmd = "explorer.exe \"" + path + "\"";
    std::thread([cmd]() {
        std::system(cmd.c_str());
    }).detach();
    Debug::log(Debug::Category::Replay, "[REPLAY] Opened replays folder");
}
