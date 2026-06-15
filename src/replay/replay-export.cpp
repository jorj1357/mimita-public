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
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "replay/replay.h"
#include "debug/debug-log.h"

static ReplayExportJob gJob;

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

static std::string generateExportOutputPath()
{
    namespace fs = std::filesystem;
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
    std::error_code ec;
    fs::create_directories(exportDir, ec);
    if (ec) {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] failed to create export dir: %s", ec.message().c_str());
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
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] output path=%s", result.c_str());
    return result;
}

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight)
{
    if (gJob.state != ReplayExportJob::Idle)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Export already in progress, ignoring");
        return false;
    }

    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Button pressed, jsonPath=%s", jsonPath.c_str());

    // Check JSON file exists
    if (!std::filesystem::exists(jsonPath))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Replay file not found: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Replay file found: %s", jsonPath.c_str());

    // Load clip
    ReplayClip clip;
    if (!clip.load(jsonPath))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Failed to load replay clip: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay clip:\n" + jsonPath;
        return false;
    }

    uint32_t totalTicks = clip.header.tickCount;
    if (totalTicks == 0)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Replay has zero frames");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay has no frames.";
        return false;
    }
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Frames to export: %u", totalTicks);

    // Validate ffmpeg
    std::string ffmpeg = defaultFfmpegPath();
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] ffmpeg path=%s", ffmpeg.c_str());
    if (!std::filesystem::exists(ffmpeg))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] FFmpeg not found: %s", ffmpeg.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg not found:\n" + ffmpeg;
        return false;
    }
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] ffmpeg exists OK");

    // Generate output path
    std::string outputPath = generateExportOutputPath();
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] output path=%s", outputPath.c_str());

    // Validate output directory exists
    std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
    if (!std::filesystem::exists(outDir))
    {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] failed to create output dir: %s", ec.message().c_str());
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Cannot create output directory:\n" + outDir.string();
            return false;
        }
    }

    // Build ffmpeg command with native separators
    namespace fs = std::filesystem;
    std::string nativeOutput = fs::path(outputPath).make_preferred().string();
    std::string cmd = "\"" + ffmpeg + "\" -y -f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(renderWidth) + "x" + std::to_string(renderHeight) + " "
        "-framerate 60 -i pipe: -c:v libx264 -preset fast -pix_fmt yuv420p "
        "-crf 18 \"" + nativeOutput + "\"";

    Debug::log(Debug::Category::Replay, "[FFMPEG] command: %s", cmd.c_str());

    // Open ffmpeg pipe
    FILE* pipe = _popen(cmd.c_str(), "wb");
    if (!pipe)
    {
#ifdef _WIN32
        DWORD err = GetLastError();
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] pipe open FAILED GetLastError=%lu errno=%d",
                   (unsigned long)err, errno);
#else
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] pipe open FAILED errno=%d", errno);
#endif
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] full command=%s", cmd.c_str());
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] working dir=%s",
                   std::filesystem::current_path().string().c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to launch FFmpeg.";
        return false;
    }
    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] pipe open OK");

    // Initialize job state
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

    Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Capture started: %ux%u, %u frames",
               renderWidth, renderHeight, totalTicks);

    return true;
}

void updateReplayExport()
{
    if (gJob.state != ReplayExportJob::Capturing)
        return;

    int w = gJob.capWidth;
    int h = gJob.capHeight;
    std::vector<uint8_t> pixels(w * h * 3);

    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Flip vertically
    std::vector<uint8_t> flipped(w * h * 3);
    for (int y = 0; y < h; ++y)
    {
        std::memcpy(
            &flipped[y * w * 3],
            &pixels[(h - 1 - y) * w * 3],
            w * 3);
    }

    size_t written = fwrite(flipped.data(), 1, flipped.size(), gJob.ffmpegPipe);
    if (written != flipped.size())
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] pipe write FAILED at frame %u (disk full?)",
                   gJob.capturedTicks);
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Pipe write failed during frame capture.";
        _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        return;
    }

    gJob.capturedTicks++;
    gJob.frameWriteCount = gJob.capturedTicks;

    if (gJob.capturedTicks % 120 == 0)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] frame write count=%u/%u (%.0f%%)",
                   gJob.capturedTicks, gJob.totalTicks,
                   (float)gJob.capturedTicks / (float)gJob.totalTicks * 100.0f);
    }

    if (gJob.capturedTicks >= gJob.totalTicks)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] frame write count=%u (done)", gJob.capturedTicks);

        int ret = _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        gJob.ffmpegExitCode = ret;

        if (ret != 0)
        {
            Debug::log(Debug::Category::Replay, "[FFMPEG] exit code=%d (NON-ZERO)", ret);
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "FFmpeg exited with code " + std::to_string(ret);
            return;
        }
        else
        {
            Debug::log(Debug::Category::Replay, "[FFMPEG] exit code=0 (OK)");
        }

        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] finished OK output=%s", gJob.outputPath.c_str());

        if (!std::filesystem::exists(gJob.outputPath))
        {
            Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] output file missing after encoding");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file missing after encoding:\n" + gJob.outputPath;
            return;
        }

        uint64_t fileSize = std::filesystem::file_size(gJob.outputPath);
        if (fileSize == 0)
        {
            Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] output file is empty (0 bytes)");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file is empty:\n" + gJob.outputPath;
            return;
        }

        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] Validation passed: %llu bytes", (unsigned long long)fileSize);
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
