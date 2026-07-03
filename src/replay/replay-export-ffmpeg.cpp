#include "replay/replay-export.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

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
#include "audio/audio-codec.h"
#include "replay/replay.h"
#include "terminal/terminal-state.h"

extern ReplayExportJob gJob;

struct ReplayExportAudioConfig {
    float audioVolumeMultiplier = 0.8f;
};

extern ReplayExportAudioConfig gAudioConfig;

#define EXPORTTRACE(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORTTRACE] " fmt, ##__VA_ARGS__)
#define EXPORTLOG(fmt, ...) Debug::log(Debug::Category::Replay, "[EXPORT] " fmt, ##__VA_ARGS__)
#define EXPORTTRACE_CRASH(fmt, ...) do { printf("[EXPORT] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

FILE* gReplayExportDebugFile = nullptr;

void replayExportDebugOpen()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("logs", ec);
    gReplayExportDebugFile = fopen("logs/replay_export_debug.txt", "w");
    if (gReplayExportDebugFile)
    {
        fprintf(gReplayExportDebugFile, "====================\n");
        fprintf(gReplayExportDebugFile, "REPLAY EXPORT DEBUG\n");
        fprintf(gReplayExportDebugFile, "====================\n\n");
    }
}

void replayExportDebugClose()
{
    if (gReplayExportDebugFile)
    {
        fprintf(gReplayExportDebugFile, "\n====================\n");
        fprintf(gReplayExportDebugFile, "END DEBUG LOG\n");
        fprintf(gReplayExportDebugFile, "====================\n");
        fclose(gReplayExportDebugFile);
        gReplayExportDebugFile = nullptr;
    }
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

static bool buildExportAudio(const std::string& wavPath, uint32_t totalTicks)
{
    const auto& events = REPLAY_PLAYER.soundEvents();
    printf("[RPLX AUDIO] Audio system initialized\n");
    printf("[RPLX AUDIO] Total sound events in replay: %zu\n", events.size());

    RPLXDEBUG("====================\n");
    RPLXDEBUG("REPLAY EXPORT\n");
    RPLXDEBUG("====================\n\n");
    RPLXDEBUG("Replay length: %.2f sec\n", (double)totalTicks / 60.0);
    RPLXDEBUG("Frame count: %u\n", totalTicks);
    RPLXDEBUG("Tick count: %u\n", totalTicks);
    RPLXDEBUG("Total sound events: %zu\n\n", events.size());

    if (events.empty())
    {
        printf("[RPLX AUDIO] No sound events found, creating silent track\n");
        std::vector<int16_t> silent(48000 * 2, 0);
        RPLXDEBUG("No sound events found\n");
        replayExportDebugClose();
        return writeWavFile(wavPath, silent, 48000);
    }

    for (const auto& ev : events)
    {
        if (ev.tick % 60 == 0 || ev.tick == 0)
            printf("[RPLX AUDIO] Sound event: %s tick=%d\n", ev.soundPath.c_str(), ev.tick);
    }

    uint32_t sampleRate = 48000;
    uint16_t numChannels = 2;
    double tickRate = 60.0;
    double totalDurationSec = (double)totalTicks / tickRate;
    size_t totalFrames = (size_t)(totalDurationSec * sampleRate);
    size_t totalSamples = totalFrames * numChannels;

    std::vector<float> mix(totalSamples, 0.0f);

    {
        int validCount = 0, worldCount = 0;
        for (const auto& e : events) { if (e.world) worldCount++; if (e.listenerValid) validCount++; }
        Debug::warn(Debug::Category::Replay, "[RPLX AUDIO] export totalEvents=%zu world=%d listenerValid=%d\n",
                    events.size(), worldCount, validCount);
        RPLXDEBUG("Events summary: total=%zu world=%d listenerValid=%d\n\n", events.size(), worldCount, validCount);
    }

    // Collect unique listener states every ~30 ticks
    RPLXDEBUG("====================\n");
    RPLXDEBUG("LISTENER\n");
    RPLXDEBUG("====================\n\n");
    {
        int lastListenerLogTick = -1;
        for (const auto& e : events)
        {
            int tick30 = (e.tick / 30) * 30;
            if (tick30 != lastListenerLogTick && e.listenerValid)
            {
                lastListenerLogTick = tick30;
                RPLXDEBUG("tick=%d listener pos=(%.2f %.2f %.2f) forward=(%.2f %.2f %.2f)\n",
                         tick30,
                         e.listenerPosition.x, e.listenerPosition.y, e.listenerPosition.z,
                         e.listenerForward.x, e.listenerForward.y, e.listenerForward.z);
            }
        }
    }
    RPLXDEBUG("\n");

    uint32_t decodedCount = 0;
    uint32_t duplicateCount = 0;
    uint32_t skippedCount = 0;
    std::string lastSoundPath;
    int lastSoundTick = -1;

    RPLXDEBUG("====================\n");
    RPLXDEBUG("AUDIO EVENTS\n");
    RPLXDEBUG("====================\n\n");
    for (const auto& event : events)
    {
        if (event.tick < 0 || (uint32_t)event.tick >= totalTicks)
        {
            skippedCount++;
            continue;
        }

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
                skippedCount++;
                continue;
            }
            EXPORTLOG("[REPLAY HITMARKER] include export attacker=%s viewedEntity=%s camera=%s",
                      REPLAY_PLAYER.killerId().c_str(),
                      viewedEntity.c_str(),
                      REPLAY_PLAYER.cameraController().modeName());
        }

        // Detect duplicate sound events (same sound, same tick)
        if (event.soundPath == lastSoundPath && event.tick == lastSoundTick)
            duplicateCount++;
        lastSoundPath = event.soundPath;
        lastSoundTick = event.tick;

        // Compute spatialization for world sounds with valid listener data
        float spatialAtten = 1.0f;
        float panLeft = 1.0f;
        float panRight = 1.0f;
        bool spatialize = event.world && event.listenerValid;
        if (spatialize)
        {
            glm::vec3 toSound = event.position - event.listenerPosition;
            float dist = glm::length(toSound);
            float maxDist = std::max(1.0f, event.maxDistance > 0.0f ? event.maxDistance : 30.0f);
            float minDist = 1.0f;

            // Linear distance attenuation (matches miniaudio)
            if (dist <= minDist) {
                spatialAtten = 1.0f;
            } else if (dist >= maxDist) {
                spatialAtten = 0.0f;
            } else {
                spatialAtten = 1.0f - (dist - minDist) / (maxDist - minDist);
            }

            // Stereo pan: project direction-to-sound onto listener's right vector
            glm::vec3 dir = dist > 0.001f ? glm::normalize(toSound) : glm::vec3(0.0f);
            glm::vec3 fwd = glm::normalize(event.listenerForward);
            glm::vec3 up(0.0f, 0.0f, 1.0f);
            // If forward is nearly parallel to up, use world forward as fallback
            glm::vec3 right;
            if (std::fabs(glm::dot(fwd, up)) > 0.999f)
                right = glm::vec3(1.0f, 0.0f, 0.0f);
            else
                right = glm::normalize(glm::cross(fwd, up));
            float pan = glm::clamp(glm::dot(dir, right), -1.0f, 1.0f);

            // Constant-power panning
            panLeft = std::cos((pan + 1.0f) * 3.14159265f / 4.0f);
            panRight = std::sin((pan + 1.0f) * 3.14159265f / 4.0f);

            Debug::warn(Debug::Category::Replay, "[RPLX AUDIO SPATIAL] SPATIALIZED event=%s pos=(%.2f %.2f %.2f) listener=(%.2f %.2f %.2f) dist=%.2f atten=%.4f pan=%.4f left=%.4f right=%.4f\n",
                        event.soundPath.c_str(),
                        event.position.x, event.position.y, event.position.z,
                        event.listenerPosition.x, event.listenerPosition.y, event.listenerPosition.z,
                        dist, spatialAtten, pan, panLeft, panRight);
        }
        else
        {
            Debug::warn(Debug::Category::Replay, "[RPLX AUDIO SPATIAL] FLAT event=%s world=%d listenerValid=%d listenerPos=(%.2f %.2f %.2f) pos=(%.2f %.2f %.2f)\n",
                        event.soundPath.c_str(),
                        (int)event.world, (int)event.listenerValid,
                        event.listenerPosition.x, event.listenerPosition.y, event.listenerPosition.z,
                        event.position.x, event.position.y, event.position.z);
        }

        RPLXDEBUG("tick=%d name=%s world=%d pos=(%.2f %.2f %.2f) listener=(%.2f %.2f %.2f) dist=%.2f atten=%.4f panL=%.4f panR=%.4f vol=%.2f maxDist=%.2f spatial=%d listenerValid=%d\n",
                 event.tick, event.soundPath.c_str(), (int)event.world,
                 event.position.x, event.position.y, event.position.z,
                 event.listenerPosition.x, event.listenerPosition.y, event.listenerPosition.z,
                 spatialize ? glm::length(event.position - event.listenerPosition) : 0.0f,
                 spatialAtten, panLeft, panRight,
                 event.volume, event.maxDistance, (int)spatialize, (int)event.listenerValid);

        std::string filePath = resolveSoundPath(event.soundPath);
        if (filePath.empty() || !std::filesystem::exists(filePath))
        {
            EXPORTLOG("[REPLAY AUDIO] skip unresolved event=%s", event.soundPath.c_str());
            continue;
        }

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
        float baseVolume = event.volume * spatialAtten;

        if (rate == sampleRate && ch == numChannels)
        {
            if (spatialize)
            {
                // 3D spatialized: downmix to mono then apply stereo pan
                for (size_t i = 0; i < srcFrames && (dstFrame + i) < totalFrames; i++)
                {
                    float sM = ((float)pcm[(i * 2 + 0)] + (float)pcm[(i * 2 + 1)]) / 65536.0f * baseVolume;
                    mix[(dstFrame + i) * 2 + 0] += sM * panLeft;
                    mix[(dstFrame + i) * 2 + 1] += sM * panRight;
                }
            }
            else
            {
                // 2D non-spatialized: pass through stereo unchanged
                for (size_t i = 0; i < srcFrames && (dstFrame + i) < totalFrames; i++)
                {
                    float sL = (float)pcm[(i * 2 + 0)] / 32768.0f * baseVolume;
                    float sR = (float)pcm[(i * 2 + 1)] / 32768.0f * baseVolume;
                    mix[(dstFrame + i) * 2 + 0] += sL;
                    mix[(dstFrame + i) * 2 + 1] += sR;
                }
            }
        }
        else
        {
            if (spatialize)
            {
                // 3D spatialized: downmix to mono then apply stereo pan
                for (size_t i = 0; i < srcFrames; i++)
                {
                    double srcTime = (double)i / rate;
                    double dstPos = dstFrame + srcTime * sampleRate;
                    if (dstPos >= (double)totalFrames - 1.0) break;

                    size_t dstI = (size_t)dstPos;
                    double frac = dstPos - (double)dstI;
                    size_t dstNext = dstI + 1;

                    float s = 0.0f;
                    for (uint32_t c = 0; c < ch; c++)
                        s += (float)pcm[i * ch + c] / 32768.0f;
                    s /= (float)ch;
                    float sNext = 0.0f;
                    size_t nextIdx = std::min(i + 1, srcFrames - 1);
                    for (uint32_t c = 0; c < ch; c++)
                        sNext += (float)pcm[nextIdx * ch + c] / 32768.0f;
                    sNext /= (float)ch;
                    float interp = s + (sNext - s) * (float)frac;
                    interp *= baseVolume;

                    mix[dstI * 2 + 0] += interp * panLeft;
                    if (dstNext < totalFrames)
                        mix[dstNext * 2 + 0] += interp * panLeft * (1.0f - (float)frac);
                    mix[dstI * 2 + 1] += interp * panRight;
                    if (dstNext < totalFrames)
                        mix[dstNext * 2 + 1] += interp * panRight * (1.0f - (float)frac);
                }
            }
            else
            {
                // 2D non-spatialized: pass through stereo unchanged with linear interpolation
                for (size_t i = 0; i < srcFrames; i++)
                {
                    double srcTime = (double)i / rate;
                    double dstPos = dstFrame + srcTime * sampleRate;
                    if (dstPos >= (double)totalFrames - 1.0) break;

                    size_t dstI = (size_t)dstPos;
                    double frac = dstPos - (double)dstI;
                    size_t dstNext = dstI + 1;

                    float sL = (float)pcm[i * ch + 0] / 32768.0f * baseVolume;
                    float sR = ch > 1 ? (float)pcm[i * ch + 1] / 32768.0f * baseVolume : sL;
                    size_t nextIdx = std::min(i + 1, srcFrames - 1);
                    float sLNext = (float)pcm[nextIdx * ch + 0] / 32768.0f * baseVolume;
                    float sRNext = ch > 1 ? (float)pcm[nextIdx * ch + 1] / 32768.0f * baseVolume : sLNext;
                    float interpL = sL + (sLNext - sL) * (float)frac;
                    float interpR = sR + (sRNext - sR) * (float)frac;

                    mix[dstI * 2 + 0] += interpL;
                    if (dstNext < totalFrames)
                        mix[dstNext * 2 + 0] += interpL * (1.0f - (float)frac);
                    mix[dstI * 2 + 1] += interpR;
                    if (dstNext < totalFrames)
                        mix[dstNext * 2 + 1] += interpR * (1.0f - (float)frac);
                }
            }
        }
    }

    float volMul = gAudioConfig.audioVolumeMultiplier;
    EXPORTLOG("[REPLAY AUDIO] volumeMultiplier=%.2f", volMul);

    if (decodedCount > 0) {
        printf("[RPLX AUDIO SPATIAL] Summary: %u events spatialized\n", decodedCount);
    }

    float peak = 0.0f;
    uint64_t clippedSamples = 0;
    std::vector<int16_t> output(totalSamples);
    for (size_t i = 0; i < totalSamples; i++)
    {
        float s = mix[i] * volMul;
        float absS = std::fabs(s);
        if (absS > peak) peak = absS;

        if (absS > 0.9f)
        {
            float excess = (absS - 0.9f) / (absS + 0.01f);
            s = (s > 0 ? 1.0f : -1.0f) * (0.9f + excess * 0.1f);
        }

        if (s > 1.0f) { s = 1.0f; clippedSamples++; }
        if (s < -1.0f) { s = -1.0f; clippedSamples++; }

        output[i] = (int16_t)(s * 32767.0f);
    }

    RPLXDEBUG("\n====================\n");
    RPLXDEBUG("SUMMARY\n");
    RPLXDEBUG("====================\n\n");
    RPLXDEBUG("Replay exported successfully\n");
    RPLXDEBUG("Audio events played: %u\n", decodedCount);
    RPLXDEBUG("Duplicate audio events: %u\n", duplicateCount);
    RPLXDEBUG("Skipped audio events: %u\n", skippedCount);
    RPLXDEBUG("Peak level: %.2f\n", peak);
    RPLXDEBUG("Clipped samples: %llu\n", (unsigned long long)clippedSamples);
    RPLXDEBUG("Audio duration: %.1f sec\n", totalDurationSec);
    RPLXDEBUG("Warnings found: 0\n");

    bool ok = writeWavFile(wavPath, output, sampleRate);
    if (ok)
    {
        uint64_t wavBytes = 0;
        std::error_code ec;
        wavBytes = std::filesystem::file_size(wavPath, ec);
        EXPORTLOG("[EXPORT AUDIO] events=%u wavBytes=%llu sampleRate=%u channels=%u duration=%.1f",
                  decodedCount, (unsigned long long)wavBytes, sampleRate, numChannels, totalDurationSec);
        EXPORTLOG("[EXPORT AUDIO] peak=%.2f clippedSamples=%llu", peak, (unsigned long long)clippedSamples);
        printf("[RPLX AUDIO] Sounds triggered: %u\n", decodedCount);
        printf("[RPLX AUDIO] Samples written: %zu\n", output.size());
        printf("[RPLX AUDIO] Audio duration: %.1f sec\n", totalDurationSec);
    } else {
        printf("[RPLX AUDIO] FAILED to write WAV file: %s\n", wavPath.c_str());
    }
    replayExportDebugClose();
    return ok;
}

static void debugLaunchFfmpegVisible(const std::string& cmd)
{
    std::string args = "/k \"" + cmd + "\"";
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

// Run ffmpeg via batch file and capture its exit code + stderr output.
// Returns ffmpeg exit code (0 = success). On failure, stderr is populated.
static int runFfmpegWithLog(const std::string& batPath, std::string& stderrOut)
{
    std::string cmd = "\"" + batPath + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        stderrOut = "_popen failed";
        return -1;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe))
        stderrOut += buf;
    return _pclose(pipe);
}

static bool encodeVideo(const std::string& nativeOutput, bool withAudio)
{
    namespace fs = std::filesystem;
    std::string nativeRaw = fs::path(gJob.rawTempPath).make_preferred().string();
    std::string nativeWav = fs::absolute(fs::path("replays") / "exports" / "_tmp" / "export_audio.wav").make_preferred().string();

    // Remove any previous output file
    std::error_code ec;
    fs::remove(nativeOutput, ec);

    std::string scaleFilter;
    if (gJob.outputWidth > 0 && gJob.outputHeight > 0 &&
        (gJob.capWidth != gJob.outputWidth || gJob.capHeight != gJob.outputHeight)) {
        scaleFilter = "-vf scale=" + std::to_string(gJob.outputWidth) + ":" + std::to_string(gJob.outputHeight);
    }

    std::string audioInput;
    std::string audioCodec;
    if (withAudio) {
        audioInput = "-i \"" + nativeWav + "\"";
        audioCodec = "-c:a aac -b:a 192k";
    }

    std::string framesV = "-frames:v " + std::to_string(gJob.totalTicks) + " ";
    std::string batContent = "@echo off\r\n"
        "\"" + fs::absolute(gJob.ffmpegPath).make_preferred().string() + "\" -y -f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(gJob.capWidth) + "x" + std::to_string(gJob.capHeight) + " "
        "-framerate 60 -i \"" + fs::absolute(nativeRaw).make_preferred().string() + "\" "
        + audioInput + " "
        + scaleFilter + " "
        "-c:v libx264 -preset fast -pix_fmt yuv420p "
        "-crf 18 "
        + framesV
        + audioCodec + " "
        + (withAudio ? "-shortest " : "") +
        "\"" + nativeOutput + "\" "
        "2>&1\r\n"
        "exit /b %ERRORLEVEL%\r\n";

    std::string batPath = fs::absolute(fs::path("replays") / "exports" / "_tmp" / (withAudio ? "encode_video.bat" : "encode_video_only.bat")).make_preferred().string();
    {
        FILE* bf = fopen(batPath.c_str(), "w");
        if (bf) {
            fwrite(batContent.c_str(), 1, batContent.size(), bf);
            fclose(bf);
        }
    }

    printf("[RPLX] ffmpeg command:\n%s\n", batContent.c_str());

    std::string stderrOut;
    int exitCode = runFfmpegWithLog(batPath, stderrOut);
    gJob.ffmpegExitCode = exitCode;

    printf("[RPLX] ffmpeg exit code: %d\n", exitCode);
    if (!stderrOut.empty())
        printf("[RPLX] ffmpeg output:\n%s\n", stderrOut.c_str());

    fs::remove(batPath, ec);

    printf("[RPLX] output exists after encode: %s\n",
           fs::exists(nativeOutput, ec) ? "yes" : "no");
    if (fs::exists(nativeOutput, ec)) {
        uint64_t size = fs::file_size(nativeOutput, ec);
        printf("[RPLX] output size: %llu bytes\n", (unsigned long long)size);
        return true;
    }
    return false;
}

void encodeReplayToMp4()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    EXPORTLOG("=== ENCODE START ===");

    std::string outputPath = fs::absolute(gJob.outputPath).make_preferred().string();
    std::string nativeRaw = fs::absolute(gJob.rawTempPath).make_preferred().string();
    std::string nativeWav = fs::absolute(fs::path("replays") / "exports" / "_tmp" / "export_audio.wav").make_preferred().string();

    // Ensure temp directory exists
    fs::create_directories(fs::path("replays") / "exports" / "_tmp", ec);

    printf("[RPLX] output folder: %s\n", fs::path(outputPath).parent_path().string().c_str());
    printf("[RPLX] output folder exists: %s\n", fs::exists(fs::path(outputPath).parent_path(), ec) ? "yes" : "no");
    printf("[RPLX] output mp4 path: %s\n", outputPath.c_str());
    printf("[RPLX] temp raw path: %s\n", nativeRaw.c_str());
    printf("[RPLX] raw file exists: %s\n", fs::exists(nativeRaw, ec) ? "yes" : "no");
    if (fs::exists(nativeRaw, ec))
        printf("[RPLX] raw file size: %llu bytes\n", (unsigned long long)fs::file_size(nativeRaw, ec));
    printf("[RPLX] temp audio path: %s\n", nativeWav.c_str());

    // Step 1: Build audio WAV
    printf("[RPLX] building audio WAV from replay sound events...\n");
    bool audioOk = buildExportAudio(nativeWav, gJob.totalTicks);
    if (!audioOk) {
        printf("[RPLX WARN] buildExportAudio failed, creating silent fallback\n");
        std::vector<int16_t> silence(48000 * 2, 0);
        audioOk = writeWavFile(nativeWav, silence, 48000, 2);
    }
    printf("[RPLX] audio export: %s\n", audioOk ? "OK" : "FAILED");

    // Step 2: Try video+audio encode
    printf("[RPLX] encoding video with audio...\n");
    bool encodeOk = encodeVideo(outputPath, true);

    // Step 3: If video+audio failed, try video-only
    if (!encodeOk) {
        printf("[RPLX WARN] video+audio encode failed, retrying video-only\n");
        encodeOk = encodeVideo(outputPath, false);
        if (encodeOk) {
            printf("[RPLX WARN] audio export failed, created video-only MP4\n");
            printf("[RPLX AUDIO] Mux successful: no\n");
            EXPORTLOG("[RPLX WARN] audio export failed, created video-only MP4");
        }
    }

    // Clean up temp files
    {
        fs::remove(gJob.rawTempPath, ec);
        fs::remove(nativeWav, ec);
    }

    if (!encodeOk) {
        EXPORTLOG("FAIL: output file missing after encoding (exit code %d)", gJob.ffmpegExitCode);
        printf("[RPLX] FAIL: output missing after encode\n");
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg encoding failed, output missing. Exit code=" + std::to_string(gJob.ffmpegExitCode);
        return;
    }

    uint64_t outSize = fs::file_size(outputPath, ec);
    gJob.mp4FileBytes = outSize;
    EXPORTLOG("PASS: output file exists, size=%llu bytes (%.1f KB)",
              (unsigned long long)gJob.mp4FileBytes, (double)gJob.mp4FileBytes / 1024.0);
    printf("[RPLX] PASS: output exists, size=%llu bytes\n", (unsigned long long)gJob.mp4FileBytes);
    printf("[RPLX AUDIO] Mux successful: yes\n");

    EXPORTLOG("[AUTO OUTRO] starting");
    appendOutroToFinishedMp4(outputPath.c_str());
    EXPORTLOG("[AUTO OUTRO] done");

    gJob.state = ReplayExportJob::Done;
    printf("[RPLX] export complete\n");
    printf("[RPLX] output exists: yes\n");
    printf("[RPLX] output size: %llu bytes\n", (unsigned long long)gJob.mp4FileBytes);
}
