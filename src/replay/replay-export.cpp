#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

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

static std::string generateExportOutputPath()
{
    namespace fs = std::filesystem;
    fs::create_directories("replays");

    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char fileName[64];
    std::strftime(fileName, sizeof(fileName), "duel_%Y-%m-%d_%H-%M-%S.mp4", &localTime);

    std::string path = (fs::path("replays") / fileName).string();
    // Never overwrite existing exports
    int attempt = 1;
    while (fs::exists(path)) {
        char numbered[96];
        std::snprintf(numbered, sizeof(numbered), "replays/duel_%Y-%m-%d_%H-%M-%S_%d.mp4",
                      attempt);
        path = numbered;
        attempt++;
    }
    return path;
}

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight)
{
    if (gJob.state != ReplayExportJob::Idle)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Export already in progress, ignoring");
        return false;
    }

    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Button Pressed");

    // Check JSON file exists
    if (!std::filesystem::exists(jsonPath))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Replay file not found: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }

    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Replay Found: %s", jsonPath.c_str());

    // Load clip to determine frame count
    ReplayClip clip;
    if (!clip.load(jsonPath))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Failed to load replay clip: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay clip:\n" + jsonPath;
        return false;
    }

    uint32_t totalTicks = clip.header.tickCount;
    if (totalTicks == 0)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Replay has zero frames");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay has no frames.";
        return false;
    }

    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Frames to export: %u", totalTicks);

    // Find ffmpeg
    std::string ffmpeg = defaultFfmpegPath();
    if (!std::filesystem::exists(ffmpeg))
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] FFmpeg not found: %s", ffmpeg.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg not found:\n" + ffmpeg;
        return false;
    }

    // Generate output path
    std::string outputPath = generateExportOutputPath();
    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Output: %s", outputPath.c_str());

    // Build ffmpeg command for raw RGB pipe input
    std::string cmd = "\"" + ffmpeg + "\" -y -f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(renderWidth) + "x" + std::to_string(renderHeight) + " "
        "-framerate 60 -i pipe: -c:v libx264 -preset fast -pix_fmt yuv420p "
        "-crf 18 \"" + outputPath + "\"";

    // Open ffmpeg pipe
    FILE* pipe = _popen(cmd.c_str(), "wb");
    if (!pipe)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Failed to launch FFmpeg");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to launch FFmpeg.";
        return false;
    }

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

    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Launching FFmpeg");
    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Capture started: %ux%u, %u frames",
               renderWidth, renderHeight, totalTicks);

    return true;
}

void updateReplayExport()
{
    if (gJob.state != ReplayExportJob::Capturing)
        return;

    // Read pixels from the default framebuffer
    // glReadPixels reads from the currently bound read framebuffer.
    // After the main loop renders, the back buffer has the frame.
    int w = gJob.capWidth;
    int h = gJob.capHeight;
    std::vector<uint8_t> pixels(w * h * 3);

    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // GL reads bottom-up; ffmpeg expects top-down. Flip vertically.
    std::vector<uint8_t> flipped(w * h * 3);
    for (int y = 0; y < h; ++y)
    {
        std::memcpy(
            &flipped[y * w * 3],
            &pixels[(h - 1 - y) * w * 3],
            w * 3);
    }

    // Write to ffmpeg pipe
    size_t written = fwrite(flipped.data(), 1, flipped.size(), gJob.ffmpegPipe);
    if (written != flipped.size())
    {
        Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Pipe write failed (disk full?)");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Pipe write failed during frame capture.";
        _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        return;
    }

    gJob.capturedTicks++;

    // Progress log every 120 frames
    if (gJob.capturedTicks % 120 == 0)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Captured %u / %u frames (%.0f%%)",
                   gJob.capturedTicks, gJob.totalTicks,
                   (float)gJob.capturedTicks / (float)gJob.totalTicks * 100.0f);
    }

    // Check if capture is complete
    if (gJob.capturedTicks >= gJob.totalTicks)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Frames Exported: %u", gJob.capturedTicks);

        // Close ffmpeg pipe
        int ret = _pclose(gJob.ffmpegPipe);
        gJob.ffmpegPipe = nullptr;
        gJob.ffmpegExitCode = ret;

        if (ret != 0)
        {
            Debug::log(Debug::Category::Replay, "[REPLAY ERROR] ffmpeg exited with code %d", ret);
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "FFmpeg exited with code " + std::to_string(ret);
            return;
        }

        Debug::log(Debug::Category::Replay, "[REPLAY SAVE] MP4 Created: %s", gJob.outputPath.c_str());

        // Validate output file
        if (!std::filesystem::exists(gJob.outputPath))
        {
            Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Output file missing after encoding");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file missing after encoding:\n" + gJob.outputPath;
            return;
        }

        uint64_t fileSize = std::filesystem::file_size(gJob.outputPath);
        if (fileSize == 0)
        {
            Debug::log(Debug::Category::Replay, "[REPLAY ERROR] Output file is empty (0 bytes)");
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file is empty:\n" + gJob.outputPath;
            return;
        }

        Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Validation passed: %llu bytes", (unsigned long long)fileSize);
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
    std::string path = "replays";
    std::string cmd = "explorer.exe \"" + path + "\"";
    std::thread([cmd]() {
        std::system(cmd.c_str());
    }).detach();
    Debug::log(Debug::Category::Replay, "[REPLAY] Opened replays folder");
}
