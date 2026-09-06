// 08 16 2026, 01 35
/* purpose
* Captures rendered replay frames and advances the selected MP4 export backend.
* Owns common completion state, live-view restoration, and user notifications.
* Exposes status and progress shared by commands and gameplay overlays.
* Does NOT record replay clips or resolve encoder installations.
* Does NOT own replay editor commands or gameplay key bindings.
* Does NOT append branded outros for the Windows backend.
*/
#include "replay/replay-export.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <glad/glad.h>

#include "replay/replay.h"
#include "replay/replay-editor.h"
#include "replay/replay-export.h"
#include "replay/replay-export-target.h"
#include "video/outro.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"
#include "render/post-fx.h"
#include "audio/audio-codec.h"
#include "notifications/notifications.h"
#include "gui/hud/chat-bubble.h"

void encodeReplayToMp4();

ReplayExportJob gJob;
static std::string gLastSuccessfulExportPath;
static std::atomic<bool> gExplorerLaunchFailed{false};
// Subprocess handle (separate from gJob so isReplayExportActive stays false).
void* sExportSubprocess = nullptr;

bool gReplayExportVerbose = false;

ReplayExportFrameTimings gExportFrameTimings;
ReplayExportFrameTimings gExportTimingTotals;
uint32_t gExportTimingFrames = 0;
static double gExportFrameStartSec = 0.0;

double replayExportNowSec()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void replayExportTimingFrameBegin()
{
    gExportFrameTimings = ReplayExportFrameTimings{};
    gExportFrameStartSec = replayExportNowSec();
}

void replayExportTimingFrameEnd()
{
    const double now = replayExportNowSec();
    gExportFrameTimings.totalMs = (now - gExportFrameStartSec) * 1000.0;
    const double accounted = gExportFrameTimings.seekMs + gExportFrameTimings.updateMs +
        gExportFrameTimings.weaponEventsMs + gExportFrameTimings.audioEventsMs +
        gExportFrameTimings.renderMs + gExportFrameTimings.readPixelsMs +
        gExportFrameTimings.copyMs + gExportFrameTimings.encoderMs;
    gExportFrameTimings.waitMs = std::max(0.0, gExportFrameTimings.totalMs - accounted);

    gExportTimingTotals.seekMs += gExportFrameTimings.seekMs;
    gExportTimingTotals.updateMs += gExportFrameTimings.updateMs;
    gExportTimingTotals.weaponEventsMs += gExportFrameTimings.weaponEventsMs;
    gExportTimingTotals.audioEventsMs += gExportFrameTimings.audioEventsMs;
    gExportTimingTotals.renderMs += gExportFrameTimings.renderMs;
    gExportTimingTotals.readPixelsMs += gExportFrameTimings.readPixelsMs;
    gExportTimingTotals.copyMs += gExportFrameTimings.copyMs;
    gExportTimingTotals.encoderMs += gExportFrameTimings.encoderMs;
    gExportTimingTotals.waitMs += gExportFrameTimings.waitMs;
    gExportTimingTotals.totalMs += gExportFrameTimings.totalMs;
    gExportTimingFrames++;

    if (gReplayExportVerbose) {
        Debug::log(Debug::Category::Replay,
            "[EXPORT TIMING] frame total=%.1f seek=%.1f update=%.1f weapon=%.1f audio=%.1f "
            "render=%.1f read=%.1f copy=%.1f enc=%.1f wait=%.1f",
            gExportFrameTimings.totalMs, gExportFrameTimings.seekMs,
            gExportFrameTimings.updateMs, gExportFrameTimings.weaponEventsMs,
            gExportFrameTimings.audioEventsMs, gExportFrameTimings.renderMs,
            gExportFrameTimings.readPixelsMs, gExportFrameTimings.copyMs,
            gExportFrameTimings.encoderMs, gExportFrameTimings.waitMs);
    }
}

void replayExportTimingReset()
{
    gExportFrameTimings = ReplayExportFrameTimings{};
    gExportTimingTotals = ReplayExportFrameTimings{};
    gExportTimingFrames = 0;
    gExportFrameStartSec = 0.0;
}

void replayExportTimingLogSummary()
{
    if (gExportTimingFrames == 0) return;
    const double n = (double)gExportTimingFrames;
    Debug::warn(Debug::Category::Replay,
        "[EXPORT TIMING] frames=%u avg(total=%.1f seek=%.1f update=%.1f weapon=%.1f audio=%.1f "
        "render=%.1f read=%.1f copy=%.1f enc=%.1f wait=%.1f)ms",
        gExportTimingFrames,
        gExportTimingTotals.totalMs / n, gExportTimingTotals.seekMs / n,
        gExportTimingTotals.updateMs / n, gExportTimingTotals.weaponEventsMs / n,
        gExportTimingTotals.audioEventsMs / n, gExportTimingTotals.renderMs / n,
        gExportTimingTotals.readPixelsMs / n, gExportTimingTotals.copyMs / n,
        gExportTimingTotals.encoderMs / n, gExportTimingTotals.waitMs / n);
}

static bool gFfmpegDebugMode = false;

#define EXPORTTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)
#define EXPORTLOG(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORT] " fmt, ##__VA_ARGS__)
#define EXPORTTRACE_CRASH(fmt, ...) do { printf("[EXPORT] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

void setFfmpegDebugMode(bool enabled)
{
    gFfmpegDebugMode = enabled;
    EXPORTTRACE("ffmpeg debug mode = %s", enabled ? "ON (visible cmd window)" : "OFF (background process)");
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
        return totalTicks > 0 ? std::min(exportTick / (float)totalTicks, 1.0f) : 0.0f;
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
    if (gExplorerLaunchFailed.exchange(false, std::memory_order_acq_rel)) {
        const std::string message = "Could not open Explorer. Clip path copied to clipboard.";
        Terminal::instance().addLog(message);
        NotificationSystem::instance().pushImportant("CLIP PATH COPIED", message, 300);
    }

    // ── Subprocess export: poll the background mimita.exe process ─────
    if (sExportSubprocess) {
        DWORD exitCode = 0;
        BOOL done = GetExitCodeProcess((HANDLE)sExportSubprocess, &exitCode);
        if (done && exitCode != STILL_ACTIVE) {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT] subprocess finished: exitCode=%lu outputPath='%s'\n",
                (unsigned long)exitCode, gJob.outputPath.c_str());
            Debug::warn(Debug::Category::Replay,
                "[EXPORT] output file exists=%d size=%llu\n",
                (int)std::filesystem::exists(gJob.outputPath),
                std::filesystem::exists(gJob.outputPath)
                    ? (unsigned long long)std::filesystem::file_size(gJob.outputPath) : 0ULL);
            CloseHandle((HANDLE)sExportSubprocess);
            sExportSubprocess = nullptr;

            bool success = (exitCode == 0) &&
                std::filesystem::exists(gJob.outputPath) &&
                std::filesystem::file_size(gJob.outputPath) > 0;

            if (success) {
                Debug::warn(Debug::Category::Replay,
                    "[EXPORT] subprocess SUCCESS: %llu bytes\n",
                    (unsigned long long)std::filesystem::file_size(gJob.outputPath));
                finishReplayExport(true);
            } else {
                Debug::error(Debug::Category::Replay,
                    "[EXPORT] subprocess FAILED: exitCode=%lu exists=%d size=%llu\n",
                    (unsigned long)exitCode,
                    (int)std::filesystem::exists(gJob.outputPath),
                    std::filesystem::exists(gJob.outputPath)
                        ? (unsigned long long)std::filesystem::file_size(gJob.outputPath) : 0ULL);
                finishReplayExport(false,
                    "Export subprocess failed (exit code " + std::to_string(exitCode) + ")");
            }
        }
        return;
    }

    if (gJob.state == ReplayExportJob::Encoding) {
        if (gJob.mfWriter) {
            bool ok = false;
            bool outroMissing = false;
            std::string error;
            Debug::log(Debug::Category::Replay,
                "[EXPORT] MF encoding in progress, polling...\n");
            if (pollMfReplayExport(gJob.mfWriter, ok, outroMissing, error)) {
                Debug::warn(Debug::Category::Replay,
                    "[EXPORT] MF encoding done: ok=%d outroMissing=%d error='%s'\n",
                    (int)ok, (int)outroMissing, error.c_str());
                Debug::warn(Debug::Category::Replay,
                    "[EXPORT] output file exists=%d size=%llu\n",
                    (int)std::filesystem::exists(gJob.outputPath),
                    std::filesystem::exists(gJob.outputPath)
                        ? (unsigned long long)std::filesystem::file_size(gJob.outputPath) : 0ULL);
                if (!gJob.ffmpegWavPath.empty()) {
                    std::error_code ec;
                    std::filesystem::remove(gJob.ffmpegWavPath, ec);
                    gJob.ffmpegWavPath.clear();
                }
                gJob.mfOutroMissing = outroMissing;
                finishReplayExport(ok, error);
            }
            return;
        }
        Debug::log(Debug::Category::Replay,
            "[EXPORT] FFmpeg encoding in progress, polling...\n");
        pollReplayFfmpegEncode();
        return;
    }
    if (gJob.state != ReplayExportJob::Capturing)
    {
        return;
    }

    // Encoder inbox full: skip capture work this frame. Each replay tick is
    // rendered and enqueued exactly once, so the export never re-renders the
    // same tick while waiting for the encoder. The game keeps running.
    if (gJob.mfWriter && !mfReplayQueueHasRoom(gJob.mfWriter)) {
        if (gReplayExportVerbose)
            Debug::logThrottled(Debug::Category::Replay, "export-wait", 1.0f,
                "[EXPORT WAIT] encoder busy, inbox full\n");
        return;
    }

    // Deterministic offline frame index: captureSpeed drives exportTick.
    float captureSpeed = 1.0f;
    if (gReplayEditor.isLoaded()) {
        captureSpeed = gReplayEditor.playbackSpeedAtTick((int)gJob.exportTick);
        if (captureSpeed <= 0.001f) {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT ERROR] speed keyframe <= 0 at tick=%d (forcing 1.0)\n",
                (int)gJob.exportTick);
            captureSpeed = 1.0f;
        }
    }

    // Stuck-tick guard: count consecutive frames at the same seek tick.
    const uint32_t seekTickNow = (uint32_t)gJob.exportTick;
    if (seekTickNow == gJob.lastSeekTick) {
        gJob.seekRepeatCount++;
    } else {
        gJob.lastSeekTick = seekTickNow;
        gJob.seekRepeatCount = 0;
    }
    if (gJob.seekRepeatCount == 1) {
        // One-time dump of what is at the first-repeated tick.
        const ReplaySceneFrame* sf = REPLAY_PLAYER.currentSceneFrame();
        if (sf) {
            std::string weapons;
            for (const auto& a : sf->actors) {
                if (!weapons.empty()) weapons += ",";
                weapons += a.weaponName.empty() ? "none" : a.weaponName;
            }
            Debug::log(Debug::Category::Replay,
                "[EXPORT STEP] first repeat at tick=%u actors=%zu effects=%zu weapons=%s\n",
                seekTickNow, sf->actors.size(), sf->effects.size(), weapons.c_str());
        }
    }
    if (gJob.seekRepeatCount > 3) {
        if (gJob.mfWriter && !mfReplayInitReady(gJob.mfWriter)) {
            Debug::logThrottled(Debug::Category::Replay, "export-init-wait", 1.0f,
                "[EXPORT WAIT] waiting for encoder init (tick=%u)\n", seekTickNow);
        } else {
            Debug::warn(Debug::Category::Replay,
                "[EXPORT ERROR] seekTick stuck tick=%u repeatCount=%u frameIndex=%u "
                "state=Capturing reason=unknown; aborting export\n",
                seekTickNow, gJob.seekRepeatCount, gJob.capturedTicks);
            if (gJob.mfWriter) cancelMfReplayExport(gJob.mfWriter);
            finishReplayExport(false, "Export stalled (seekTick not advancing).");
            return;
        }
    }

    int w = gJob.capWidth;
    int h = gJob.capHeight;
    uint32_t frameNum = gJob.capturedTicks;

    if (frameNum == 0) {
        EXPORTTRACE("=== updateReplayExport: first frame ===");
        EXPORTLOG("STAGE 7/8: capturing frames");
        printf("[RPLX] render start\n");
        printf("[RPLX] resolution: %dx%d\n", w, h);
        printf("[RPLX] fps: 60\n");
        printf("[RPLX] tick rate: 60\n");
        printf("[RPLX] total ticks: %u\n", gJob.totalTicks);
        printf("[RPLX] export uses speed keyframes\n");

        if (gReplayEditor.isLoaded()) {
            auto& ed = gReplayEditor;
            printf("[RPLX] hasActiveReplayEditor=1\n");
            printf("[RPLX] hasUnsavedEditorChanges=0\n");
            printf("[RPLX] loadedKeyframes campos=%d cammode=%d pbspeed=%d\n",
                   ed.cameraKeyframeCount(), ed.cameraModeKeyframeCount(),
                   ed.timeKeyframeCount());
            printf("[RPLX] exportUsesReplayEditorCamera=%d\n",
                   ed.cameraKeyframeCount() > 0 ? 1 : 0);
        } else {
            printf("[RPLX] hasActiveReplayEditor=0\n");
            printf("[RPLX] exportUsesReplayEditorCamera=0\n");
        }

        RPLXDEBUG("====================\n");
        RPLXDEBUG("LISTENER / CAMERA\n");
        RPLXDEBUG("====================\n\n");

        RPLXDEBUG("====================\n");
        RPLXDEBUG("HEALTHBARS\n");
        RPLXDEBUG("====================\n\n");
    }

    if (gReplayExportVerbose)
        EXPORTTRACE("Frame %u/%u: capturing pixels (%dx%d*3=%d bytes)",
                    frameNum, gJob.totalTicks, w, h, w * h * 3);

    const size_t pixelBytes = (size_t)w * (size_t)h * 3;
    if (gJob.pixelBuffer.size() != pixelBytes)
        gJob.pixelBuffer.resize(pixelBytes);
    uint8_t* pixels = gJob.pixelBuffer.data();

    if (gReplayExportVerbose) {
        // [F] Framebuffer state: log read/draw FBO binding and viewport
        GLint readFb = 0, drawFb = 0, viewport[4] = {};
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFb);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFb);
        glGetIntegerv(GL_VIEWPORT, viewport);
        GLuint postfxFbo = PostFX::instance().fboId();
        EXPORTLOG("[EXPORT DEBUG] FB state: read=%d draw=%d postfxFbo=%u defaultFbo=0 viewport=%dx%d export=%dx%d",
                  readFb, drawFb, postfxFbo, viewport[2], viewport[3], w, h);
    }

    // engineTickUI has completed before updateReplayExport is called. Copy the
    // completed default framebuffer now so the export target contains the world,
    // weapon models, HUD, chat, killfeed, replay panels, and overlays together.
    // The export subprocess creates its framebuffer at capWidth/capHeight, so UI
    // projection and capture resolution match instead of stretching 1024x768.
    bool capturedPostUiTarget = false;
    if (replayExportTarget().ready()) {
        auto& tgt = replayExportTarget();
        GLint viewport[4] = {};
        glGetIntegerv(GL_VIEWPORT, viewport);
        const bool sourceMatchesCapture = viewport[2] >= 64 && viewport[3] >= 64;
        if (!sourceMatchesCapture) {
            Debug::warn(Debug::Category::Replay,
                "[replay-export-ui] frame=%u invalid post-ui source viewport=%dx%d; reading default framebuffer directly\n",
                frameNum, viewport[2], viewport[3]);
        } else {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tgt.fbo);
            glBlitFramebuffer(
                viewport[0], viewport[1], viewport[0] + viewport[2], viewport[1] + viewport[3],
                0, 0, tgt.width, tgt.height,
                GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            const GLenum blitError = glGetError();
            if (blitError != GL_NO_ERROR) {
                Debug::warn(Debug::Category::Replay,
                    "[replay-export-ui] frame=%u post-ui blit failed glError=0x%x; reading default framebuffer directly\n",
                    frameNum, blitError);
            } else if (gReplayExportVerbose) {
                Debug::logThrottled(Debug::Category::Replay, "replay-export-ui-capture", 1.0f,
                    "[replay-export-ui] frame=%u capture after ui source=%dx%d export=%dx%d renderMode=1\n",
                    frameNum, viewport[2], viewport[3], tgt.width, tgt.height);
                capturedPostUiTarget = true;
            } else {
                capturedPostUiTarget = true;
            }
        }
    }

    // Read from the post-UI offscreen export FBO when available. Falls back to
    // the completed default framebuffer if the export target could not initialize.
    const double tRead0 = replayExportNowSec();
    if (capturedPostUiTarget) {
        auto& tgt = replayExportTarget();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, tgt.fbo);
        glReadPixels(0, 0, tgt.width, tgt.height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    }
    gExportFrameTimings.readPixelsMs += (replayExportNowSec() - tRead0) * 1000.0;

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        EXPORTTRACE("Frame %u: glReadPixels GL ERROR=0x%x", frameNum, glErr);
        printf("[RPLX ERROR] glReadPixels failed at frame %u: 0x%x\n", frameNum, glErr);
    } else if (gReplayExportVerbose) {
        EXPORTTRACE("Frame %u: glReadPixels OK", frameNum);
    }

    // Log first 3 frames and last frame
    if (frameNum < 3 || frameNum >= gJob.totalTicks - 1)
        printf("[RPLX] rendered frame %u/%u\n", frameNum + 1, gJob.totalTicks);

    // Sample first pixel and compute rolling hash (diagnostics; logs gated)
    if (gReplayExportVerbose) {
        uint8_t r = pixels[0], g = pixels[1], b = pixels[2];
        uint8_t r2 = pixels[w*3], g2 = pixels[w*3+1], b2 = pixels[w*3+2];
        EXPORTTRACE_CRASH("Frame %u: pixel(0,0)=RGB(%u,%u,%u) pixel(0,1)=RGB(%u,%u,%u)",
               frameNum, r, g, b, r2, g2, b2);
        if (r == 255 && g == 0 && b == 255)
            EXPORTTRACE_CRASH("*** MAGENTA PIXEL DETECTED - PostFX FBO not rendered to default framebuffer ***");
    }
    if (gReplayExportVerbose && frameNum % 60 == 0) {
        const ReplaySceneFrame* sf = REPLAY_PLAYER.currentSceneFrame();
        uint32_t ac = sf ? (uint32_t)sf->actors.size() : 0;
        float replayTime = REPLAY_PLAYER.totalTicks() > 0
            ? (float)REPLAY_PLAYER.currentTick() / 60.0f : 0.0f;
        EXPORTLOG("[EXPORT DEBUG] frame=%u tick=%u time=%.2f playing=%d actors=%u",
                  frameNum, REPLAY_PLAYER.currentTick(), replayTime,
                  (int)REPLAY_PLAYER.isPlaying(), ac);
    }

    if (gJob.mfWriter) {
        bool accepted = false;
        std::string error;
        const double tEnc0 = replayExportNowSec();
        if (!writeMfReplayVideoFrame(gJob.mfWriter, pixels, w, h, frameNum, &accepted, error)) {
            cancelMfReplayExport(gJob.mfWriter);
            finishReplayExport(false, error);
            return;
        }
        gExportFrameTimings.encoderMs += (replayExportNowSec() - tEnc0) * 1000.0;
        if (!accepted) {
            // Encoder queue is full: keep the game running, do not advance this frame.
            replayExportTimingFrameEnd();
            return;
        }
        gJob.capturedTicks++;
        gJob.frameWriteCount = gJob.capturedTicks;
        if (gReplayExportVerbose)
            Debug::log(Debug::Category::Replay,
                "[EXPORT STEP] frameIndex=%u seekTick=%u lastSeekTick=%u advanced=1 state=Capturing encoderQueue=%zu\n",
                gJob.capturedTicks, (uint32_t)gJob.exportTick, gJob.lastSeekTick,
                mfReplayQueueSize(gJob.mfWriter));
    } else if (gJob.rawFile) {
        const double tCopy0 = replayExportNowSec();
        if (gJob.flipBuffer.size() != pixelBytes)
            gJob.flipBuffer.resize(pixelBytes);
        uint8_t* flipped = gJob.flipBuffer.data();
        for (int y = 0; y < h; ++y)
            std::memcpy(&flipped[y * w * 3], &pixels[(h - 1 - y) * w * 3], w * 3);
        const size_t expectedBytes = pixelBytes;
        size_t written = fwrite(flipped, 1, expectedBytes, gJob.rawFile);
        gExportFrameTimings.copyMs += (replayExportNowSec() - tCopy0) * 1000.0;
        if (written != expectedBytes) {
            int fwErr = ferror(gJob.rawFile);
            EXPORTTRACE_CRASH("Frame %u: fwrite FAILED (wrote %zu/%zu) ferror=%d",
                              frameNum, written, expectedBytes, fwErr);
            fclose(gJob.rawFile);
            gJob.rawFile = nullptr;
            finishReplayExport(false, "Raw file write failed during frame capture.");
            return;
        }

        gJob.capturedTicks++;
        gJob.frameWriteCount = gJob.capturedTicks;
    }

    // Advance export tick (captureSpeed computed above, clamped to never be 0).
    if (gReplayExportVerbose)
        Debug::log(Debug::Category::Replay, "[ReplayExport] tick=%d speed=%.2f\n",
            (int)gJob.exportTick, captureSpeed);
    gJob.exportTick += captureSpeed;
    gJob.seekRepeatCount = 0; // progress made this frame

    uint32_t doneTick = (uint32_t)gJob.exportTick;
    if (doneTick > gJob.totalTicks) doneTick = gJob.totalTicks;

    if (gJob.capturedTicks % 30 == 0 || doneTick >= gJob.totalTicks)
    {
        float pct = (float)doneTick / (float)gJob.totalTicks * 100.0f;
        EXPORTTRACE("PROGRESS: %u/%u (%.1f%%)", gJob.capturedTicks, gJob.totalTicks, pct);
    }
    // Terminal progress every 10%
    if (gJob.totalTicks > 0) {
        static int lastTerminalPct = -1;
        int currentPct = (int)((float)doneTick / (float)gJob.totalTicks * 100.0f);
        int reportPct = (currentPct / 10) * 10;
        if (reportPct > lastTerminalPct && reportPct > 0 && reportPct <= 100) {
            lastTerminalPct = reportPct;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "[RPLX] Export progress: %d%%", reportPct);
            Terminal::instance().addLog(std::string(buf));
        }
        if (doneTick >= gJob.totalTicks) lastTerminalPct = -1;
    }

    if (doneTick >= gJob.totalTicks)
    {
        printf("[RPLX] rendered frame %u (exportTick=%.1f/%u)\n", gJob.capturedTicks, gJob.exportTick, gJob.totalTicks);
        printf("[RPLX] render complete\n");
        printf("[RPLX] actual frames rendered: %u\n", gJob.capturedTicks);
        EXPORTTRACE("=== ALL FRAMES WRITTEN (%u) ===", gJob.capturedTicks);

        if (gJob.mfWriter) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::create_directories(fs::path("replays") / "exports" / "_tmp", ec);
            std::string wavPath = (fs::path("replays") / "exports" / "_tmp" / "export_audio.wav").string();
            bool audioOk = buildReplayExportAudio(wavPath, gJob.totalTicks);
            gJob.ffmpegWavPath = audioOk ? wavPath : std::string();
            finishMfReplayExport(gJob.mfWriter,
                                 gJob.ffmpegWavPath,
                                 "assets/video/mimitaoutrov1.mp4");
            gJob.state = ReplayExportJob::Encoding;
            replayExportTimingFrameEnd();
            return;
        }

        // [G] Verify raw file size on disk before closing
        {
            std::error_code ec;
            uint64_t expectedRawSize = (uint64_t)gJob.capturedTicks * (uint64_t)gJob.capWidth * (uint64_t)gJob.capHeight * 3ULL;
            gJob.rawFileBytes = std::filesystem::file_size(gJob.rawTempPath, ec);
            EXPORTLOG("[EXPORT DEBUG] raw file: path=%s bytes=%llu expected=%llu (frames=%u)",
                      gJob.rawTempPath.c_str(), (unsigned long long)gJob.rawFileBytes,
                      (unsigned long long)expectedRawSize, gJob.capturedTicks);
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
    replayExportTimingFrameEnd();
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
        std::snprintf(buf, sizeof(buf), "Exporting Replay...\nFrames: %u  Ticks: %u/%u (%.0f%%)",
                      gJob.capturedTicks, (uint32_t)gJob.exportTick, gJob.totalTicks, pct);
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

void finishReplayExport(bool success, const std::string& error)
{
    Debug::warn(Debug::Category::Replay,
        "[EXPORT] ========== FINISH EXPORT ==========\n");
    Debug::warn(Debug::Category::Replay,
        "[EXPORT] success=%d error='%s'\n", (int)success, error.c_str());
    Debug::warn(Debug::Category::Replay,
        "[EXPORT] outputPath='%s'\n", gJob.outputPath.c_str());
    Debug::warn(Debug::Category::Replay,
        "[EXPORT] capturedFrames=%u totalTicks=%u\n", gJob.capturedTicks, gJob.totalTicks);
    replayExportTargetDestroy();
    restoreReplayExportEditorState();
    if (gJob.restoreLiveOnFinish) {
        REPLAY_PLAYER.stopPlayback();
        REPLAY_ACTOR_MODELS.clear();
        REPLAY_WEAPON_MODELS.clear();
        REPLAY_CHAT_STATES.clear();
    }
    const bool outroMissing = gJob.mfOutroMissing;
    gJob.mfOutroMissing = false;

    if (!success) {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = error.empty() ? "MP4 export failed." : error;
        Debug::warn(Debug::Category::Replay, "[REPLAY EXPORT] FAILED: %s\n", gJob.errorMsg.c_str());
        if (gJob.clipExport)
            NotificationSystem::instance().pushCritical("CLIP EXPORT FAILED", gJob.errorMsg, 800);
        replayExportTimingLogSummary();
        return;
    }

    std::error_code ec;
    gJob.mp4FileBytes = std::filesystem::file_size(gJob.outputPath, ec);
    if (ec || gJob.mp4FileBytes == 0) {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Encoder completed without a readable MP4.";
        if (gJob.clipExport)
            NotificationSystem::instance().pushCritical("CLIP EXPORT FAILED", "Clip export failed. Check logs.", 600);
        replayExportTimingLogSummary();
        return;
    }
    gLastSuccessfulExportPath = std::filesystem::absolute(gJob.outputPath).make_preferred().string();
    gJob.state = ReplayExportJob::Done;
    if (gJob.clipExport) {
        NotificationSystem::instance().pushImportant(
            "CLIP EXPORTED",
            "Clip exported! Press J to open it in Explorer.\n" + gLastSuccessfulExportPath,
            600);
        if (outroMissing || (!error.empty() && error.find("outro") != std::string::npos)) {
            NotificationSystem::instance().push(
                "NO OUTRO",
                "Outro could not be appended; exported clip without outro.",
                300, {});
            Terminal::instance().addLog("[CLIP EXPORT] Clip exported WITHOUT outro. Check logs for details.");
        }
    }
    double now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    Debug::warn(Debug::Category::Replay,
                "[REPLAY EXPORT] completed backend=%s encoderMode=%s durationSec=%.2f frames=%u bytes=%llu output=%s outro=%s",
                gExportConfig.encoder.c_str(), gExportConfig.encoderMode.c_str(),
                now - gJob.startTimeSec,
                gJob.capturedTicks,
                (unsigned long long)gJob.mp4FileBytes, gLastSuccessfulExportPath.c_str(),
                outroMissing ? "missing" : "ok");
    replayExportTimingLogSummary();
}

void openLastReplayExport()
{
    if (gLastSuccessfulExportPath.empty()) {
        Terminal::instance().addLog("No exported clip yet. Press P after a cool moment.");
        return;
    }
    if (!std::filesystem::exists(gLastSuccessfulExportPath)) {
        copyTextToClipboard(gLastSuccessfulExportPath);
        Terminal::instance().addLog("Latest clip file not found. Path copied to clipboard.");
        return;
    }
#ifdef _WIN32
    std::string args = "/select,\"" + gLastSuccessfulExportPath + "\"";
    std::string path = gLastSuccessfulExportPath;
    std::thread([args, path]() {
        HINSTANCE result = ShellExecuteA(nullptr, "open", "explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            copyTextToClipboard(path);
            gExplorerLaunchFailed.store(true, std::memory_order_release);
        }
    }).detach();
#endif
}
