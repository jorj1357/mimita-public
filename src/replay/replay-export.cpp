#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "replay/replay.h"
#include "video/outro.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "render/post-fx.h"
#include "audio/audio-codec.h"

void encodeReplayToMp4();

ReplayExportJob gJob;

static bool gFfmpegDebugMode = false;

static constexpr const char* REPLAY_EXPORT_CONFIG_PATH = "config/replay/replay-export.json";

#define EXPORTTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)
#define EXPORTLOG(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORT] " fmt, ##__VA_ARGS__)
#define EXPORTTRACE_CRASH(fmt, ...) do { printf("[EXPORT] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

void setFfmpegDebugMode(bool enabled)
{
    gFfmpegDebugMode = enabled;
    EXPORTTRACE("ffmpeg debug mode = %s", enabled ? "ON (visible cmd window)" : "OFF (_popen)");
}

bool isFfmpegDebugMode()
{
    return gFfmpegDebugMode;
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

static void captureReplayAndEncode()
{
    EXPORTTRACE("Running capture thread for replay export...");
}

void updateReplayExport()
{
    if (gJob.state != ReplayExportJob::Capturing)
    {
        return;
    }

    int w = gJob.capWidth;
    int h = gJob.capHeight;
    uint32_t frameNum = gJob.capturedTicks;

    if (frameNum == 0) {
        EXPORTTRACE("=== updateReplayExport: first frame ===");
        EXPORTLOG("STAGE 7/8: capturing frames");
    }

    EXPORTTRACE("Frame %u/%u: allocating pixels buffer (%dx%d*3=%d bytes)",
                frameNum, gJob.totalTicks, w, h, w * h * 3);
    std::vector<uint8_t> pixels(w * h * 3);

    // [F] Framebuffer state: log read/draw FBO binding and viewport
    {
        GLint readFb = 0, drawFb = 0, viewport[4] = {};
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFb);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFb);
        glGetIntegerv(GL_VIEWPORT, viewport);
        GLuint postfxFbo = PostFX::instance().fboId();
        EXPORTLOG("[EXPORT DEBUG] FB state: read=%d draw=%d postfxFbo=%u defaultFbo=0 viewport=%dx%d export=%dx%d",
                  readFb, drawFb, postfxFbo, viewport[2], viewport[3], w, h);
    }

    // [E] Ensure we read from the default framebuffer (PostFX should have resolved by now)
    // Explicitly bind default framebuffer for read to prevent reading from stale FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    EXPORTTRACE("Frame %u: calling glReadPixels...", frameNum);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
        EXPORTTRACE("Frame %u: glReadPixels GL ERROR=0x%x", frameNum, glErr);
    else
        EXPORTTRACE("Frame %u: glReadPixels OK", frameNum);

    // Sample first pixel and compute rolling hash
    {
        uint8_t r = pixels[0], g = pixels[1], b = pixels[2];
        uint8_t r2 = pixels[w*3], g2 = pixels[w*3+1], b2 = pixels[w*3+2];
        EXPORTTRACE_CRASH("Frame %u: pixel(0,0)=RGB(%u,%u,%u) pixel(0,1)=RGB(%u,%u,%u)",
               frameNum, r, g, b, r2, g2, b2);
        if (r == 255 && g == 0 && b == 255)
            EXPORTTRACE_CRASH("*** MAGENTA PIXEL DETECTED - PostFX FBO not rendered to default framebuffer ***");

        // Rolling hash to detect static frames
        static uint64_t firstFrameHash = 0;
        uint64_t thisHash = 0;
        const uint8_t* cp = pixels.data() + (w * (h/2) * 3);
        for (int i = 0; i < 64 && i < w; i++)
            thisHash = (thisHash << 1) ^ cp[i * 3];
        if (frameNum == 0) {
            firstFrameHash = thisHash;
            EXPORTLOG("frame 0 hash=%llu", (unsigned long long)thisHash);
        } else if (frameNum % 60 == 0) {
            bool identical = (thisHash == firstFrameHash);
            EXPORTLOG("frame %u hash=%llu sameAsFrame0=%s", frameNum, (unsigned long long)thisHash, identical ? "YES (STATIC)" : "NO (advancing)");
        }

        // [A] [B] Debug log every 60 frames
        if (frameNum % 60 == 0) {
            const ReplaySceneFrame* sf = REPLAY_PLAYER.currentSceneFrame();
            uint32_t ac = sf ? (uint32_t)sf->actors.size() : 0;
            float replayTime = REPLAY_PLAYER.totalTicks() > 0
                ? (float)REPLAY_PLAYER.currentTick() / 60.0f : 0.0f;
            EXPORTLOG("[EXPORT DEBUG] frame=%u tick=%u time=%.2f playing=%d actors=%u",
                      frameNum, REPLAY_PLAYER.currentTick(), replayTime,
                      (int)REPLAY_PLAYER.isPlaying(), ac);
            if (sf && !sf->actors.empty()) {
                auto& p = sf->actors[0].position;
                EXPORTLOG("[EXPORT DEBUG] actor0=(%.2f,%.2f,%.2f)", (double)p.x, (double)p.y, (double)p.z);
            }
        }
    }

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
    size_t written = fwrite(flipped.data(), 1, expectedBytes, gJob.rawFile);
    EXPORTTRACE("Frame %u: fwrite returned %zu (expected %zu)", frameNum, written, expectedBytes);

    if (written != expectedBytes)
    {
        int fwErr = ferror(gJob.rawFile);
        EXPORTTRACE_CRASH("Frame %u: fwrite FAILED (wrote %zu/%zu) ferror=%d",
                    frameNum, written, expectedBytes, fwErr);
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Raw file write failed during frame capture.";
        fclose(gJob.rawFile);
        gJob.rawFile = nullptr;
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

        // [G] Verify raw file size on disk before closing
        {
            std::error_code ec;
            uint64_t expectedRawSize = (uint64_t)gJob.totalTicks * (uint64_t)gJob.capWidth * (uint64_t)gJob.capHeight * 3ULL;
            gJob.rawFileBytes = std::filesystem::file_size(gJob.rawTempPath, ec);
            EXPORTLOG("[EXPORT DEBUG] raw file: path=%s bytes=%llu expected=%llu",
                      gJob.rawTempPath.c_str(), (unsigned long long)gJob.rawFileBytes,
                      (unsigned long long)expectedRawSize);
        }

        // Flush and close raw file
        fflush(gJob.rawFile);
        fclose(gJob.rawFile);
        gJob.rawFile = nullptr;

        // [G] Verify file size after close
        {
            std::error_code ec;
            gJob.rawFileBytes = std::filesystem::file_size(gJob.rawTempPath, ec);
            EXPORTLOG("[EXPORT DEBUG] raw file after close: bytes=%llu", (unsigned long long)gJob.rawFileBytes);
        }

        EXPORTLOG("STAGE 8/8: encoding MP4 from raw frames");
        gJob.state = ReplayExportJob::Encoding;
        encodeReplayToMp4();
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

const ReplayExportJob& getReplayExportJob()
{
    return gJob;
}
