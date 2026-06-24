#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
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
    fwrite(&bitsPerSample, 16, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataBytes, 4, 1, f);
    fwrite(samples.data(), 1, dataBytes, f);
    fclose(f);
    return true;
}

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

    uint32_t sampleRate = 48000;
    uint16_t numChannels = 2;
    double tickRate = 60.0;
    double totalDurationSec = (double)totalTicks / tickRate + 1.0;
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

        if (rate == sampleRate && ch == numChannels)
        {
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

    float volMul = gAudioConfig.audioVolumeMultiplier;
    EXPORTLOG("[REPLAY AUDIO] volumeMultiplier=%.2f", volMul);

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

void encodeReplayToMp4()
{
    namespace fs = std::filesystem;
    std::string nativeOutput = fs::path(gJob.outputPath).make_preferred().string();
    std::string nativeRaw = fs::path(gJob.rawTempPath).make_preferred().string();
    std::string nativeWav = (fs::path("replays") / "exports" / "_tmp" / "export_audio.wav").make_preferred().string();

    EXPORTLOG("[EXPORT AUDIO] Building audio WAV from replay sound events");
    bool audioOk = buildExportAudio(nativeWav, gJob.totalTicks);
    if (!audioOk) {
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

    EXPORTLOG("[EXPORT DEBUG] ffmpeg command=%s", batContent.c_str());

    int encodeResult = std::system(batPath.c_str());
    gJob.ffmpegExitCode = encodeResult;

    EXPORTLOG("[EXPORT DEBUG] ffmpeg exit code=%d", encodeResult);

    {
        std::error_code ec;
        std::filesystem::remove(gJob.rawTempPath, ec);
        std::filesystem::remove(batPath, ec);
        std::filesystem::remove(nativeWav, ec);
    }

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
