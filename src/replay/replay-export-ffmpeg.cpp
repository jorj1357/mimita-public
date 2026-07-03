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

    if (events.empty())
    {
        printf("[RPLX AUDIO] No sound events found, creating silent track\n");
        std::vector<int16_t> silent(48000 * 2, 0);
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

    uint32_t decodedCount = 0;
    for (const auto& event : events)
    {
        if (event.tick < 0 || (uint32_t)event.tick >= totalTicks)
            continue;

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
            glm::vec3 right = glm::normalize(glm::cross(event.listenerForward, glm::vec3(0.0f, 0.0f, 1.0f)));
            float pan = glm::clamp(glm::dot(dir, right), -1.0f, 1.0f);

            // Constant-power panning
            panLeft = std::cos((pan + 1.0f) * 3.14159265f / 4.0f);
            panRight = std::sin((pan + 1.0f) * 3.14159265f / 4.0f);

            printf("[RPLX AUDIO SPATIAL] event=%s pos=(%.1f %.1f %.1f) listener=(%.1f %.1f %.1f) dist=%.1f atten=%.2f pan=%.2f left=%.2f right=%.2f\n",
                   event.soundPath.c_str(),
                   event.position.x, event.position.y, event.position.z,
                   event.listenerPosition.x, event.listenerPosition.y, event.listenerPosition.z,
                   dist, spatialAtten, pan, panLeft, panRight);
        }

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
            for (size_t i = 0; i < srcFrames && (dstFrame + i) < totalFrames; i++)
            {
                float s = (float)pcm[(i * 2)] / 32768.0f * baseVolume;
                mix[(dstFrame + i) * 2 + 0] += s * panLeft;
                mix[(dstFrame + i) * 2 + 1] += s * panRight;
            }
        }
        else
        {
            for (size_t i = 0; i < srcFrames; i++)
            {
                double srcTime = (double)i / rate;
                double dstPos = dstFrame + srcTime * sampleRate;
                if (dstPos >= (double)totalFrames - 1.0) break;

                size_t dstI = (size_t)dstPos;
                double frac = dstPos - (double)dstI;
                size_t dstNext = dstI + 1;

                float s = (float)pcm[i * ch] / 32768.0f;
                float sNext = (float)pcm[std::min(i + 1, srcFrames - 1) * ch] / 32768.0f;
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
