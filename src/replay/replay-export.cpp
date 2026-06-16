#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
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
#include "video/outro.h"
#include <nlohmann/json.hpp>
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "render/post-fx.h"
#include "audio/audio.h"

static ReplayExportJob gJob;

static bool gFfmpegDebugMode = false;

// ---- Replay Export Audio Config (hot-reload) ----
struct ReplayExportAudioConfig {
    float audioVolumeMultiplier = 0.8f;
};

static ReplayExportAudioConfig gAudioConfig;
static uint64_t gAudioConfigLastWrite = 0;

static const char* REPLAY_EXPORT_CONFIG_PATH = "config/replay/replay-export.json";

static uint64_t cfgFileWriteTime(const char* path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

static void reloadReplayExportConfig()
{
    std::ifstream file(REPLAY_EXPORT_CONFIG_PATH);
    if (!file.is_open())
        return;

    try
    {
        nlohmann::json j;
        file >> j;

        ReplayExportAudioConfig loaded;
        if (j.contains("audioVolumeMultiplier"))
            loaded.audioVolumeMultiplier = j["audioVolumeMultiplier"].get<float>();

        gAudioConfig = loaded;
        Debug::log(Debug::Category::Replay, "[REPLAY AUDIO] config reloaded audioVolumeMultiplier=%.2f", gAudioConfig.audioVolumeMultiplier);
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY AUDIO] config reload failed: %s", e.what());
    }
}

void pollReplayExportConfig()
{
    static double elapsed = 0.0;
    elapsed += 1.0 / 60.0;
    if (elapsed < 0.25)
        return;
    elapsed = 0.0;

    uint64_t wt = cfgFileWriteTime(REPLAY_EXPORT_CONFIG_PATH);
    if (wt == 0)
        return;

    if (wt != gAudioConfigLastWrite)
    {
        gAudioConfigLastWrite = wt;
        reloadReplayExportConfig();
    }
}

#define EXPORTTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)
#define EXPORTLOG(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORT] " fmt, ##__VA_ARGS__)
#define EXPORTTRACE_CRASH(fmt, ...) do { printf("[EXPORT] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

float getReplayExportAudioVolume()
{
    return gAudioConfig.audioVolumeMultiplier;
}

void setFfmpegDebugMode(bool enabled)
{
    gFfmpegDebugMode = enabled;
    EXPORTTRACE("ffmpeg debug mode = %s", enabled ? "ON (visible cmd window)" : "OFF (_popen)");
}

bool isFfmpegDebugMode()
{
    return gFfmpegDebugMode;
}

std::string makeCmdKArgs(const std::string& cmd)
{
    return "/k \"" + cmd + "\"";
}

static void debugLaunchFfmpegVisible(const std::string& cmd)
{
    std::string args = makeCmdKArgs(cmd);
    EXPORTTRACE("ShellExecuteA params EXACT=%s %s", "cmd.exe", args.c_str());
    HINSTANCE h = ShellExecuteA(NULL, "open", "cmd.exe", args.c_str(), NULL, SW_SHOWNORMAL);
    INT_PTR result = (INT_PTR)h;
    if (result <= 32) {
        DWORD err = GetLastError();
        EXPORTTRACE_CRASH("ShellExecuteA FAILED result=%lld GetLastError=%lu",
               (long long)result, (unsigned long)err);
    } else {
        EXPORTTRACE("Launched ffmpeg in visible cmd window. Close when done.");
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

static bool writeWavFile(const std::string& path, const std::vector<int16_t>& samples,
                         uint32_t sampleRate, uint16_t channels = 2)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t dataBytes = (uint32_t)(samples.size() * sizeof(int16_t));
    uint32_t riffSize = 36 + dataBytes;
    uint16_t fmt = 1;
    uint16_t ch = channels;
    uint32_t byteRate = sampleRate * ch * sizeof(int16_t);
    uint16_t blockAlign = ch * sizeof(int16_t);
    uint16_t bitsPerSample = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riffSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 4, 1, f);
    fwrite(&fmt, 2, 1, f);
    fwrite(&ch, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataBytes, 4, 1, f);
    fwrite(samples.data(), 1, dataBytes, f);
    fclose(f);
    return true;
}

// Build a stereo 48000Hz WAV from replay sound events, mixed at their tick timestamps.
// Uses float mixing with linear interpolation resampling and soft limiting
// to prevent the crackling/clipping artifacts from the prior nearest-neighbor
// integer approach.
static bool buildExportAudio(const std::string& wavPath, uint32_t totalTicks)
{
    const auto& events = REPLAY_PLAYER.soundEvents();
    if (events.empty())
    {
        std::vector<int16_t> silent(48000 * 2, 0);
        return writeWavFile(wavPath, silent, 48000);
    }

    EXPORTLOG("[REPLAY AUDIO] events=%zu", events.size());
    for (const auto& ev : events)
    {
        if (ev.tick % 60 == 0 || ev.tick == 0)
            EXPORTLOG("[REPLAY AUDIO] event=%s tick=%d", ev.soundPath.c_str(), ev.tick);
    }

    // 48000 Hz stereo output
    uint32_t sampleRate = 48000;
    uint16_t numChannels = 2;
    double tickRate = 60.0;
    double totalDurationSec = (double)totalTicks / tickRate + 1.0;
    size_t totalFrames = (size_t)(totalDurationSec * sampleRate);
    size_t totalSamples = totalFrames * numChannels;

    // Float mix buffer: interleaved L/R, initialized to 0
    std::vector<float> mix(totalSamples, 0.0f);

    uint32_t decodedCount = 0;
    for (const auto& event : events)
    {
        if (event.tick < 0 || (uint32_t)event.tick >= totalTicks)
            continue;

        // Skip hitmarker sounds unless camera is viewing the attacker
        if (event.soundPath == "hitmarker1")
        {
            const ReplayCameraMode camMode = REPLAY_PLAYER.cameraController().mode();
            std::string viewedEntity;
            switch (camMode) {
                case ReplayCameraMode::Freecam: break;
                case ReplayCameraMode::FirstPerson:
                case ReplayCameraMode::Orbit:
                    viewedEntity = REPLAY_PLAYER.killerId();
                    break;
                case ReplayCameraMode::Victim:
                    viewedEntity = REPLAY_PLAYER.victimId();
                    break;
            }
            if (!ReplayShouldPlayHitmarkerAudio(
                    REPLAY_PLAYER.killerId(), camMode, viewedEntity))
            {
                EXPORTLOG("[REPLAY HITMARKER] skip export attacker=%s viewedEntity=%s camera=%s",
                          REPLAY_PLAYER.killerId().c_str(),
                          viewedEntity.c_str(),
                          REPLAY_PLAYER.cameraController().modeName());
                continue;
            }
            EXPORTLOG("[REPLAY HITMARKER] include export attacker=%s viewedEntity=%s camera=%s",
                      REPLAY_PLAYER.killerId().c_str(),
                      viewedEntity.c_str(),
                      REPLAY_PLAYER.cameraController().modeName());
        }

        std::string filePath = resolveSoundPath(event.soundPath);
        if (filePath.empty() || !std::filesystem::exists(filePath))
        {
            EXPORTLOG("[REPLAY AUDIO] skip unresolved event=%s", event.soundPath.c_str());
            continue;
        }

        // Decode at 48000 Hz stereo directly (avoids extra resampling)
        std::vector<int16_t> pcm;
        uint32_t rate = 0, ch = 0;
        if (!decodeAudioToPCM(filePath, pcm, rate, ch, sampleRate, numChannels))
        {
            EXPORTLOG("[REPLAY AUDIO] skip undecodable event=%s file=%s", event.soundPath.c_str(), filePath.c_str());
            continue;
        }
        decodedCount++;

        double eventTime = (double)event.tick / tickRate;
        size_t dstFrame = (size_t)(eventTime * sampleRate);
        size_t srcFrames = pcm.size() / ch;

        // If the decoded rate differs from target (shouldn't since we request 48000,
        // but miniaudio may fall back to source rate), do linear interpolation.
        if (rate == sampleRate && ch == numChannels)
        {
            // Fast path: direct copy with volume
            for (size_t i = 0; i < srcFrames && (dstFrame + i) < totalFrames; i++)
            {
                for (uint32_t c = 0; c < numChannels; c++)
                {
                    float s = (float)pcm[(i * ch + c)] * event.volume / 32768.0f;
                    mix[(dstFrame + i) * numChannels + c] += s;
                }
            }
        }
        else
        {
            // Resampling path: linear interpolation
            for (size_t i = 0; i < srcFrames; i++)
            {
                double srcTime = (double)i / rate;
                double dstPos = dstFrame + srcTime * sampleRate;
                if (dstPos >= (double)totalFrames - 1.0) break;

                size_t dstI = (size_t)dstPos;
                double frac = dstPos - (double)dstI;
                size_t dstNext = dstI + 1;

                for (uint32_t c = 0; c < std::min(ch, (uint32_t)numChannels); c++)
                {
                    float s0 = (float)pcm[i * ch + std::min(c, ch - 1)] / 32768.0f;
                    float s1 = (float)pcm[std::min(i + 1, srcFrames - 1) * ch + std::min(c, ch - 1)] / 32768.0f;
                    float interp = s0 + (s1 - s0) * (float)frac;
                    interp *= event.volume;

                    uint32_t outCh = std::min(c, (uint32_t)(numChannels - 1));
                    mix[dstI * numChannels + outCh] += interp;
                    if (dstNext < totalFrames)
                        mix[dstNext * numChannels + outCh] += interp * (1.0f - (float)frac);
                }
            }
        }
    }

    // Apply global audio volume multiplier (from config)
    float volMul = gAudioConfig.audioVolumeMultiplier;
    EXPORTLOG("[REPLAY AUDIO] volumeMultiplier=%.2f", volMul);

    // Soft limiting to prevent clipping while preserving dynamic range.
    // Uses a simple hard knee: apply gain reduction at peaks > 0.9.
    float peak = 0.0f;
    uint64_t clippedSamples = 0;
    std::vector<int16_t> output(totalSamples);
    for (size_t i = 0; i < totalSamples; i++)
    {
        float s = mix[i] * volMul;
        float absS = std::fabs(s);
        if (absS > peak) peak = absS;

        // Soft limit: sigmoid-like curve for values > 0.9
        if (absS > 0.9f)
        {
            float excess = (absS - 0.9f) / (absS + 0.01f);
            s = (s > 0 ? 1.0f : -1.0f) * (0.9f + excess * 0.1f);
        }

        // Hard clamp to prevent any remaining overflow
        if (s > 1.0f) { s = 1.0f; clippedSamples++; }
        if (s < -1.0f) { s = -1.0f; clippedSamples++; }

        output[i] = (int16_t)(s * 32767.0f);
    }

    bool ok = writeWavFile(wavPath, output, sampleRate);
    if (ok)
    {
        uint64_t wavBytes = 0;
        std::error_code ec;
        wavBytes = std::filesystem::file_size(wavPath, ec);
        EXPORTLOG("[EXPORT AUDIO] events=%u wavBytes=%llu sampleRate=%u channels=%u duration=%.1f",
                  decodedCount, (unsigned long long)wavBytes, sampleRate, numChannels, totalDurationSec);
        EXPORTLOG("[EXPORT AUDIO] peak=%.2f clippedSamples=%llu", peak, (unsigned long long)clippedSamples);
    }
    return ok;
}

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight)
{
    EXPORTLOG("=== REPLAY EXPORT START ===");
    EXPORTLOG("jsonPath=%s renderWidth=%d renderHeight=%d", jsonPath.c_str(), renderWidth, renderHeight);

    if (gJob.state == ReplayExportJob::Capturing || gJob.state == ReplayExportJob::Encoding)
    {
        EXPORTLOG("[EXPORT] Export already running");
        return false;
    }
    // Allow restart after Done or Failed
    gJob = ReplayExportJob{};

    EXPORTLOG("STAGE 1/8: checking replay file");
    if (!std::filesystem::exists(jsonPath))
    {
        EXPORTLOG("FAIL: replay file NOT FOUND at %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }
    EXPORTLOG("PASS: replay file found at %s", jsonPath.c_str());

    EXPORTLOG("STAGE 2/8: loading replay clip");
    ReplayClip clip;
    if (!clip.load(jsonPath))
    {
        EXPORTLOG("FAIL: cannot load replay clip: %s", jsonPath.c_str());
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay clip:\n" + jsonPath;
        return false;
    }

    uint32_t totalTicks = clip.header.tickCount;
    if (totalTicks == 0)
    {
        EXPORTLOG("FAIL: replay has zero frames");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay has no frames.";
        return false;
    }
    EXPORTLOG("PASS: replay loaded, tickCount=%u, sceneFrames=%zu, duration=%.1fs",
              totalTicks, clip.sceneFrames.size(), (float)totalTicks / 60.0f);

    EXPORTLOG("STAGE 3/8: loading replay into REPLAY_PLAYER");
    if (!REPLAY_PLAYER.loadFromJSON(jsonPath)) {
        EXPORTLOG("FAIL: cannot load into REPLAY_PLAYER");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay into player:\n" + jsonPath;
        return false;
    }
    REPLAY_PLAYER.beginPlayback();
    REPLAY_PLAYER.seekToTick(0);
    uint32_t loadedTick = REPLAY_PLAYER.currentTick();
    const ReplaySceneFrame* firstFrame = REPLAY_PLAYER.currentSceneFrame();
    uint32_t actorCount = firstFrame ? (uint32_t)firstFrame->actors.size() : 0;
    EXPORTLOG("PASS: REPLAY_PLAYER loaded: totalTicks=%u isPlaying=%d isPaused=%d currentTick=%u actorCount=%u",
              REPLAY_PLAYER.totalTicks(), (int)REPLAY_PLAYER.isPlaying(),
              (int)REPLAY_PLAYER.isPaused(), loadedTick, actorCount);
    if (loadedTick == 0 && REPLAY_PLAYER.totalTicks() > 0)
        EXPORTLOG("WARN: currentTick=0 but totalTicks=%u -- seekToTick clamped to 0 (input frames empty?)",
                  REPLAY_PLAYER.totalTicks());
    if (actorCount == 0)
        EXPORTLOG("WARN: replay has zero actors in frame 0");

    EXPORTLOG("STAGE 4/8: checking ffmpeg");

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
    EXPORTLOG("PASS: ffmpeg found at %s", ffmpeg.c_str());

    EXPORTLOG("STAGE 5/8: generating output path, total frames to export=%u", totalTicks);
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

    // ROOT CAUSE FIX: Query actual framebuffer dimensions for capture.
    // The caller may pass a desired output resolution (e.g. 1280x720) that
    // does NOT match the actual window/framebuffer size. glReadPixels must
    // read at the actual framebuffer size to produce valid pixel data.
    // If the sizes differ, ffmpeg will scale from capture size to output size.
    int captureW = renderWidth;
    int captureH = renderHeight;
    {
        GLint vp[4] = {};
        glGetIntegerv(GL_VIEWPORT, vp);
        if (vp[2] > 0 && vp[3] > 0) {
            captureW = vp[2];
            captureH = vp[3];
        }
        EXPORTLOG("Dimensions: requested=%dx%d actual viewport=%dx%d capture=%dx%d",
                  renderWidth, renderHeight, vp[2], vp[3], captureW, captureH);
    }

    EXPORTLOG("STAGE 6/8: creating temp raw file for frame capture");
    namespace fs = std::filesystem;
    std::string rawTempDir = (fs::path("replays") / "exports" / "_tmp").string();
    {
        std::error_code ec;
        fs::create_directories(rawTempDir, ec);
    }
    std::string rawTempPath = (fs::path("replays") / "exports" / "_tmp" / "export_raw.rgb").string();
    EXPORTLOG("PASS: raw temp path=%s", rawTempPath.c_str());

    FILE* rawFile = fopen(rawTempPath.c_str(), "wb");
    if (!rawFile) {
        EXPORTLOG("FAIL: cannot create temp raw file at %s (errno=%d)", rawTempPath.c_str(), errno);
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Cannot create temp raw file:\n" + rawTempPath;
        return false;
    }
    EXPORTLOG("PASS: temp raw file opened for writing");

    EXPORTTRACE("Initializing job state...");
    gJob.state = ReplayExportJob::Capturing;
    gJob.jsonPath = jsonPath;
    gJob.totalTicks = totalTicks;
    gJob.capturedTicks = 0;
    gJob.capWidth = captureW;
    gJob.capHeight = captureH;
    gJob.outputWidth = renderWidth;
    gJob.outputHeight = renderHeight;
    gJob.ffmpegPath = ffmpeg;
    gJob.rawTempPath = rawTempPath;
    gJob.rawFile = rawFile;
    gJob.outputPath = outputPath;
    gJob.ffmpegExitCode = -1;
    gJob.errorMsg.clear();
    gJob.frameWriteCount = 0;
    gJob.rawFileBytes = 0;
    gJob.mp4FileBytes = 0;
    EXPORTTRACE("gJob.state set to Capturing (1)");

    EXPORTLOG("=== startReplayExport returning true ===");
    return true;
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

        // Build ffmpeg command to encode raw file to MP4 with audio.
        // Use a batch script to avoid cmd.exe quoting issues with std::system().
        namespace fs = std::filesystem;
        std::string nativeOutput = fs::path(gJob.outputPath).make_preferred().string();
        std::string nativeRaw = fs::path(gJob.rawTempPath).make_preferred().string();
        std::string nativeWav = (fs::path("replays") / "exports" / "_tmp" / "export_audio.wav").make_preferred().string();

        // Generate audio WAV from replay sound events
        EXPORTLOG("[EXPORT AUDIO] Building audio WAV from replay sound events");
        bool audioOk = buildExportAudio(nativeWav, gJob.totalTicks);
        if (!audioOk) {
            // Fallback: create a silent WAV so ffmpeg always has an audio input
            EXPORTLOG("[EXPORT AUDIO] buildExportAudio failed, creating silent fallback");
            std::vector<int16_t> silence(48000 * 2, 0);
            audioOk = writeWavFile(nativeWav, silence, 48000, 2);
        }
        EXPORTLOG("[EXPORT AUDIO] buildExportAudio=%s", audioOk ? "OK" : "FAILED (silent fallback)");

        std::string scaleFilter;
        if (gJob.outputWidth > 0 && gJob.outputHeight > 0 &&
            (gJob.capWidth != gJob.outputWidth || gJob.capHeight != gJob.outputHeight)) {
            scaleFilter = "-vf scale=" + std::to_string(gJob.outputWidth) + ":" + std::to_string(gJob.outputHeight);
        }

        std::string audioInput = "-i \"" + nativeWav + "\"";
        std::string audioCodec = "-c:a aac -b:a 192k";

        std::string batContent = "@echo off\r\n"
            "\"" + gJob.ffmpegPath + "\" -y -f rawvideo -pixel_format rgb24 "
            "-video_size " + std::to_string(gJob.capWidth) + "x" + std::to_string(gJob.capHeight) + " "
            "-framerate 60 -i \"" + nativeRaw + "\" "
            + audioInput + " "
            + scaleFilter + " "
            "-c:v libx264 -preset fast -pix_fmt yuv420p "
            "-crf 18 "
            + audioCodec + " "
            "-shortest \""
            + nativeOutput + "\" "
            "-loglevel error\r\n"
            "exit /b %ERRORLEVEL%\r\n";

        std::string batPath = (fs::path("replays") / "exports" / "_tmp" / "encode.bat").make_preferred().string();
        {
            FILE* bf = fopen(batPath.c_str(), "w");
            if (bf) {
                fwrite(batContent.c_str(), 1, batContent.size(), bf);
                fclose(bf);
            }
        }

        // [H] Log exact ffmpeg command
        EXPORTLOG("[EXPORT DEBUG] ffmpeg command=%s", batContent.c_str());

        int encodeResult = std::system(batPath.c_str());
        gJob.ffmpegExitCode = encodeResult;

        // [H] Log exit code
        EXPORTLOG("[EXPORT DEBUG] ffmpeg exit code=%d", encodeResult);

        // [G] Clean up temp raw file, batch file, and audio WAV
        {
            std::error_code ec;
            std::filesystem::remove(gJob.rawTempPath, ec);
            std::filesystem::remove(batPath, ec);
            std::filesystem::remove(nativeWav, ec);
        }

        // Validate by checking output file existence/size, not just exit code.
        // std::system() may return non-zero on Windows even when ffmpeg
        // successfully creates the output (cmd.exe nesting issue).
        if (!std::filesystem::exists(gJob.outputPath))
        {
            EXPORTLOG("FAIL: output file missing after encoding (exit code %d)", encodeResult);
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "FFmpeg encoding failed, output missing. Exit code=" + std::to_string(encodeResult);
            return;
        }

        uint64_t outSize = 0;
        {
            std::error_code ec;
            outSize = std::filesystem::file_size(gJob.outputPath, ec);
        }
        if (outSize == 0)
        {
            EXPORTLOG("FAIL: output file is empty (0 bytes, exit code %d)", encodeResult);
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Output file is empty:\n" + gJob.outputPath;
            return;
        }

        gJob.mp4FileBytes = outSize;
        EXPORTLOG("[EXPORT DEBUG] mp4 size=%llu", (unsigned long long)gJob.mp4FileBytes);

        EXPORTLOG("PASS: output file exists, size=%llu bytes (%.1f KB)",
                  (unsigned long long)gJob.mp4FileBytes, (double)gJob.mp4FileBytes / 1024.0);
        EXPORTLOG("=== EXPORT COMPLETE ===");

        EXPORTLOG("[AUTO OUTRO] starting");
        appendOutroToFinishedMp4(gJob.outputPath.c_str());
        EXPORTLOG("[AUTO OUTRO] done");

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
    if (gJob.state == ReplayExportJob::Capturing && gJob.rawFile)
    {
        fclose(gJob.rawFile);
        gJob.rawFile = nullptr;
    }
    if (!gJob.rawTempPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(gJob.rawTempPath, ec);
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
