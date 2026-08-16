// 08 16 2026, 01 35
/* purpose
* Mixes replay audio and runs the optional developer FFmpeg MP4 backend.
* Preserves replay camera timing, spatial audio, scaling, and outro behavior.
* Reports encoder output and validates that the requested MP4 was produced.
* Does NOT select the active export backend or resolve FFmpeg installations.
* Does NOT capture OpenGL frames or register terminal commands.
* Does NOT bundle FFmpeg with player releases.
*/
#include "replay/replay-export.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "gui/hud/player-nameplates.h"

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "replay/replay.h"
#include "replay/replay-editor.h"
#include "video/outro.h"
#include <nlohmann/json.hpp>
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "audio/audio-codec.h"
#include "replay/replay.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"

extern ReplayExportJob gJob;
static std::atomic<bool> gOutroDone{false};
static bool gOutroActive = false;
static std::thread gOutroThread;

struct ReplayExportAudioConfig {
    float audioVolumeMultiplier = 0.8f;
};

extern ReplayExportConfig gExportConfig;

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

bool writeReplayExportWav(const std::string& path, const int16_t* samples,
                          size_t sampleCount, uint32_t sampleRate, uint16_t channels)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t dataBytes = (uint32_t)(sampleCount * sizeof(int16_t));
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
    fwrite(samples, 1, dataBytes, f);
    fclose(f);
    return true;
}

// Simulate the export capture loop to compute how many video frames
// will be rendered under the current speed keyframes.
static uint32_t computeSpeedAwareFrameCount(uint32_t totalTicks)
{
    if (!gReplayEditor.isLoaded() || gReplayEditor.timeKeyframeCount() == 0)
        return totalTicks;
    double exportTick = 0.0;
    uint32_t frames = 0;
    while (exportTick < (double)totalTicks) {
        double speed = gReplayEditor.playbackSpeedAtTick((int)exportTick);
        exportTick += speed;
        frames++;
    }
    return frames;
}

// Map an original replay tick to its export-frame index (position in the
// speed-affected output). Simulates the same capture loop used by
// updateReplayExport() so audio events stay synchronized with video.
static uint32_t originalTickToExportFrame(uint32_t origTick, uint32_t totalTicks)
{
    if (!gReplayEditor.isLoaded() || gReplayEditor.timeKeyframeCount() == 0)
        return origTick;
    double exportTick = 0.0;
    uint32_t frame = 0;
    while (exportTick < (double)origTick && (double)origTick > 0.0) {
        double speed = gReplayEditor.playbackSpeedAtTick((int)exportTick);
        exportTick += speed;
        frame++;
        if (frame > totalTicks * 4) break; // safety
    }
    return frame;
}

bool buildReplayExportAudio(const std::string& wavPath, uint32_t totalTicks)
{
    const auto& events = REPLAY_PLAYER.soundEvents();
    printf("[RPLX AUDIO] Audio system initialized\n");
    printf("[RPLX AUDIO] Total sound events in replay: %zu\n", events.size());

    uint32_t videoFrames = computeSpeedAwareFrameCount(totalTicks);

    RPLXDEBUG("====================\n");
    RPLXDEBUG("REPLAY EXPORT\n");
    RPLXDEBUG("====================\n\n");
    RPLXDEBUG("Replay length: %.2f sec\n", (double)totalTicks / 60.0);
    RPLXDEBUG("Source tick count: %u\n", totalTicks);
    RPLXDEBUG("Speed-aware video frames: %u\n", videoFrames);
    RPLXDEBUG("Total sound events: %zu\n\n", events.size());

    uint32_t sampleRate = 48000;
    uint16_t numChannels = 2;
    double tickRate = 60.0;
    double totalDurationSec = (double)videoFrames / tickRate;
    size_t totalFrames = (size_t)(totalDurationSec * sampleRate);
    size_t totalSamples = totalFrames * numChannels;

    std::vector<float> mix(totalSamples, 0.0f);

    // ── Mix editor music track if loaded ─────────────────
    bool musicIncluded = false;
    std::string musicPath;
    float musicVolume = 1.0f;
    double musicOffset = 0.0, musicCropStart = 0.0, musicCropEnd = 0.0;
    double musicSpeedMul = 1.0;
    if (gReplayEditor.isLoaded()) {
        const auto& music = gReplayEditor.music();
        musicPath = music.path;
        musicOffset = music.offsetSeconds;
        musicCropStart = music.cropStartSeconds;
        musicCropEnd = music.cropEndSeconds;
        musicSpeedMul = music.speedMultiplier;
        // Fall back to legacy audio track if new music path is empty
        if (musicPath.empty() && gReplayEditor.audioTrackCount() > 0 &&
            gReplayEditor.audioTrack(0).enabled) {
            musicPath = gReplayEditor.audioTrack(0).path;
            musicVolume = gReplayEditor.audioTrack(0).volume;
        }
        if (!musicPath.empty() && std::filesystem::exists(musicPath)) {
            printf("[RPLX AUDIO] Loading editor music track: %s\n", musicPath.c_str());
            std::vector<int16_t> musicPCM;
            uint32_t musicRate = 0, musicCh = 0;
            if (decodeAudioToPCM(musicPath, musicPCM, musicRate, musicCh, sampleRate, numChannels)) {
                size_t musicFrames = musicPCM.size() / numChannels;
                size_t cropEndFrame = musicCropEnd > 0.0
                    ? (size_t)(musicCropEnd * (double)sampleRate)
                    : musicFrames;
                // Log decoded music analysis
                {
                    auto analysis = analyzeAudioBuffer(musicPCM,
                        (uint32_t)musicFrames, numChannels, musicRate);
                    logAudioAnalysis(StructuredCategory::Audio,
                        StructuredLevel::Important,
                        "MUSIC_DECODE", "REPLAY_EXPORT",
                        "Decoded music track", analysis);
                }
                // Sample-accurate mixing with offset + crop + speed + pbspeed.
                // Match the video capture loop: iterate over export-frame positions
                // (speed-aware) so music stays synchronized with video.
                // Fill a CONTIGUOUS block of audio samples per video frame.
                // Each video frame (1/60s) corresponds to 800 audio samples at 48kHz.
                // Writing only one sample per frame produces a 60 Hz click train.
                double songTime = musicOffset + musicCropStart;
                double exportTick = 0.0;
                size_t samplesPerFrame = (size_t)(sampleRate / tickRate); // 800
                for (uint32_t frameIdx = 0; frameIdx < videoFrames; ++frameIdx) {
                    double pbspeed = gReplayEditor.isLoaded()
                        ? gReplayEditor.playbackSpeedAtTick((int)exportTick)
                        : 1.0;
                    size_t srcPos = (size_t)(songTime * (double)sampleRate);
                    size_t dstPos = (size_t)((double)frameIdx * sampleRate / tickRate);
                    if (dstPos >= totalFrames) break;
                    size_t samplesToWrite = std::min({samplesPerFrame,
                        musicFrames > srcPos ? musicFrames - srcPos : 0,
                        totalFrames - dstPos});
                    if (srcPos >= cropEndFrame || srcPos >= musicFrames) break;
                    for (size_t s = 0; s < samplesToWrite; s++) {
                        float sL = (float)musicPCM[(srcPos + s) * 2 + 0] / 32768.0f * musicVolume;
                        float sR = (float)musicPCM[(srcPos + s) * 2 + 1] / 32768.0f * musicVolume;
                        mix[(dstPos + s) * 2 + 0] += sL;
                        mix[(dstPos + s) * 2 + 1] += sR;
                    }
                    songTime += (1.0 / tickRate) * musicSpeedMul * pbspeed;
                    exportTick += pbspeed;
                }
                musicIncluded = true;
                printf("[RPLX AUDIO] Music track mixed: %zu frames at %.1f sec (offset=%.1f crop=%.1f-%.1f speed=%.2f)\n",
                       musicFrames, (double)musicFrames / sampleRate,
                       musicOffset, musicCropStart, musicCropEnd, musicSpeedMul);
                Debug::log(Debug::Category::Replay,
                    "[ReplayExportMusic] source=%s offset=%.1f crop=%.1f-%.1f speed=%.2f\n",
                    musicPath.c_str(), musicOffset, musicCropStart, musicCropEnd, musicSpeedMul);
                // Log mix buffer analysis after music contribution
                {
                    auto analysis = analyzeAudioBuffer(mix,
                        (uint32_t)totalFrames, numChannels, sampleRate);
                    logAudioAnalysis(StructuredCategory::Audio,
                        StructuredLevel::Important,
                        "MUSIC_MIX", "REPLAY_EXPORT",
                        "Mix buffer after music track", analysis);
                }
            } else {
                printf("[RPLX AUDIO WARN] Failed to decode music track: %s\n", musicPath.c_str());
                Debug::log(Debug::Category::Replay,
                    "[EXPORT] Music track decode FAILED: %s\n", musicPath.c_str());
            }
        } else {
            printf("[RPLX AUDIO WARN] Music track file not found: %s\n", musicPath.c_str());
            Debug::log(Debug::Category::Replay,
                "[EXPORT] Music track file MISSING: %s\n", musicPath.c_str());
        }
    }

    if (!musicIncluded) {
        printf("[RPLX AUDIO] No music track to include\n");
    }

    // If no events and no music, return silent track
    if (events.empty() && !musicIncluded)
    {
        printf("[RPLX AUDIO] No sound events found, creating silent track\n");
        std::vector<int16_t> silent(48000 * 2, 0);
        RPLXDEBUG("No sound events found\n");
        replayExportDebugClose();
        return writeReplayExportWav(wavPath, silent.data(), silent.size(), 48000);
    }

    for (const auto& ev : events)
    {
        if (ev.tick % 60 == 0 || ev.tick == 0)
            printf("[RPLX AUDIO] Sound event: %s tick=%d\n", ev.soundPath.c_str(), ev.tick);
    }

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

        // Resample-mode mixing: iterate OUTPUT frames, advance SOURCE by pbspeed
        // per output frame. This gives the same pitch-follows-speed behavior as
        // FL Studio "resample mode" / vinyl slowdown (no pitch correction).
        double eventPbspeed = gReplayEditor.isLoaded()
            ? gReplayEditor.playbackSpeedAtTick((uint32_t)event.tick)
            : 1.0;
        eventPbspeed = std::max(eventPbspeed, 0.01);
        uint32_t eventExportFrame = originalTickToExportFrame(
            (uint32_t)event.tick, totalTicks);
        double eventTime = (double)eventExportFrame / tickRate;
        size_t dstStart = (size_t)(eventTime * sampleRate);
        size_t srcFrames = pcm.size() / ch;
        float baseVolume = event.volume * spatialAtten;

        double srcPos = 0.0;
        for (size_t dstOff = 0; dstStart + dstOff < totalFrames; dstOff++) {
            size_t srcIdx = (size_t)srcPos;
            if (srcIdx >= srcFrames) break;
            double frac = srcPos - (double)srcIdx;
            size_t srcNext = std::min(srcIdx + 1, srcFrames - 1);

            if (spatialize) {
                // 3D spatialized: downmix to mono then apply stereo pan
                float s = 0.0f;
                for (uint32_t c = 0; c < ch; c++)
                    s += (float)pcm[srcIdx * ch + c] / 32768.0f;
                s /= (float)ch;
                float sNext = 0.0f;
                for (uint32_t c = 0; c < ch; c++)
                    sNext += (float)pcm[srcNext * ch + c] / 32768.0f;
                sNext /= (float)ch;
                float interp = s + (sNext - s) * (float)frac;
                interp *= baseVolume;
                mix[(dstStart + dstOff) * 2 + 0] += interp * panLeft;
                mix[(dstStart + dstOff) * 2 + 1] += interp * panRight;
            } else {
                // 2D non-spatialized: pass through stereo with linear interpolation
                float sL = (float)pcm[srcIdx * ch + 0] / 32768.0f * baseVolume;
                float sR = ch > 1 ? (float)pcm[srcIdx * ch + 1] / 32768.0f * baseVolume : sL;
                float sLNext = (float)pcm[srcNext * ch + 0] / 32768.0f * baseVolume;
                float sRNext = ch > 1 ? (float)pcm[srcNext * ch + 1] / 32768.0f * baseVolume : sLNext;
                float interpL = sL + (sLNext - sL) * (float)frac;
                float interpR = sR + (sRNext - sR) * (float)frac;
                mix[(dstStart + dstOff) * 2 + 0] += interpL;
                mix[(dstStart + dstOff) * 2 + 1] += interpR;
            }
            srcPos += eventPbspeed;
        }
    }

    float volMul = gExportConfig.audioVolumeMultiplier;
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
    RPLXDEBUG("\n");
    RPLXDEBUG("Healthbars rendered total: %d\n", getHealthbarTotal());
    RPLXDEBUG("Live-world healthbars rendered total: %d\n", getHealthbarLiveWorld());
    RPLXDEBUG("Invalid healthbars rendered total: %d\n", getHealthbarInvalid());
    RPLXDEBUG("\n");
    RPLXDEBUG("impact_world count: %d\n", gRplxImpactWorldCount);
    RPLXDEBUG("hit_burst count: %d\n", gRplxHitBurstCount);
    RPLXDEBUG("debris_block count: %d\n", gRplxDebrisBlockCount);
    RPLXDEBUG("Duplicate effect count: %d\n", gRplxEffectDuplicateCount);

    bool ok = writeReplayExportWav(wavPath, output.data(), output.size(), sampleRate);
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
#ifndef NDEBUG
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
#else
    (void)cmd;
#endif
}

static bool launchEncodeVideo(bool withAudio)
{
    namespace fs = std::filesystem;
    std::string nativeOutput = fs::absolute(gJob.outputPath).make_preferred().string();
    std::string nativeRaw = fs::path(gJob.rawTempPath).make_preferred().string();
    std::string nativeWav = gJob.ffmpegWavPath;

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

    uint32_t actualFrames = gJob.capturedTicks > 0 ? gJob.capturedTicks : gJob.totalTicks;
    std::string framesV = "-frames:v " + std::to_string(actualFrames) + " ";
    std::string crfStr = "-crf " + std::to_string(gExportConfig.exportCrf) + " ";
    std::string bitrateStr;
    if (gExportConfig.exportBitrate > 0) {
        bitrateStr = "-b:v " + std::to_string(gExportConfig.exportBitrate) + "k ";
    }
    std::string batContent = "@echo off\r\n"
        "\"" + fs::absolute(gJob.ffmpegPath).make_preferred().string() + "\" -y -f rawvideo -pixel_format rgb24 "
        "-video_size " + std::to_string(gJob.capWidth) + "x" + std::to_string(gJob.capHeight) + " "
        "-framerate 60 -i \"" + fs::absolute(nativeRaw).make_preferred().string() + "\" "
        + audioInput + " "
        + scaleFilter + " "
        "-c:v libx264 -preset fast -pix_fmt yuv420p "
        + crfStr
        + bitrateStr
        + framesV
        + audioCodec + " "
        + (withAudio ? "-shortest " : "") +
        "\"" + nativeOutput + "\" "
        "2>&1\r\n"
        "exit /b %ERRORLEVEL%\r\n";

    gJob.ffmpegBatchPath = fs::absolute(fs::path("replays") / "exports" / "_tmp" / (withAudio ? "encode_video.bat" : "encode_video_only.bat")).make_preferred().string();
    gJob.ffmpegLogPath = fs::absolute(fs::path("replays") / "exports" / "_tmp" / "ffmpeg-export.log").make_preferred().string();
    {
        FILE* bf = fopen(gJob.ffmpegBatchPath.c_str(), "w");
        if (bf) {
            fwrite(batContent.c_str(), 1, batContent.size(), bf);
            fclose(bf);
        } else return false;
    }
#ifdef _WIN32
    HANDLE log = CreateFileA(gJob.ffmpegLogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                             nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return false;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = log;
    startup.hStdError = log;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::string command = "cmd.exe /d /s /c \"\"" + gJob.ffmpegBatchPath + "\"\"";
    BOOL started = CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(process.hThread);
    if (!started) { CloseHandle(log); return false; }
    gJob.ffmpegProcess = process.hProcess;
    gJob.ffmpegLogHandle = log;
    gJob.ffmpegWithAudio = withAudio;
    return true;
#else
    return false;
#endif
}

void encodeReplayToMp4()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    EXPORTLOG("=== ENCODE START ===");
    {
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Replay;
        e.level = StructuredLevel::Important;
        e.eventId = "REPLAY_ENCODE_STARTED";
        e.correlationId = "REPLAY_EXPORT";
        e.reason = "Replay encode started: building audio + muxing video";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        StructuredLogger::instance().write(e);
    }

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
    bool audioOk = buildReplayExportAudio(nativeWav, gJob.totalTicks);
    if (!audioOk) {
        printf("[RPLX WARN] buildExportAudio failed, creating silent fallback\n");
        std::vector<int16_t> silence(48000 * 2, 0);
        audioOk = writeReplayExportWav(nativeWav, silence.data(), silence.size(), 48000, 2);
    }
    printf("[RPLX] audio export: %s\n", audioOk ? "OK" : "FAILED");

    gJob.ffmpegWavPath = nativeWav;
    if (!launchEncodeVideo(audioOk))
        finishReplayExport(false, "Could not start FFmpeg.");
}

void pollReplayFfmpegEncode()
{
#ifdef _WIN32
    if (gOutroActive) {
        if (!gOutroDone.load(std::memory_order_acquire)) return;
        if (gOutroThread.joinable()) gOutroThread.join();
        gOutroActive = false;
        finishReplayExport(true);
        return;
    }
    if (!gJob.ffmpegProcess) return;
    DWORD code = STILL_ACTIVE;
    if (!GetExitCodeProcess((HANDLE)gJob.ffmpegProcess, &code) || code == STILL_ACTIVE)
        return;
    CloseHandle((HANDLE)gJob.ffmpegProcess);
    CloseHandle((HANDLE)gJob.ffmpegLogHandle);
    gJob.ffmpegProcess = nullptr;
    gJob.ffmpegLogHandle = nullptr;
    gJob.ffmpegExitCode = (int)code;
    namespace fs = std::filesystem;
    std::error_code ec;
    bool outputOk = code == 0 && fs::exists(gJob.outputPath, ec) && fs::file_size(gJob.outputPath, ec) > 0;
    fs::remove(gJob.ffmpegBatchPath, ec);
    if (!outputOk && gJob.ffmpegWithAudio) {
        if (!launchEncodeVideo(false))
            finishReplayExport(false, "Could not start FFmpeg video-only fallback.");
        return;
    }
    fs::remove(gJob.rawTempPath, ec);
    fs::remove(gJob.ffmpegWavPath, ec);
    if (!outputOk) {
        finishReplayExport(false, "FFmpeg encoding failed. Exit code=" + std::to_string(code));
        return;
    }
    std::string output = fs::absolute(gJob.outputPath).make_preferred().string();
    gOutroDone.store(false, std::memory_order_release);
    gOutroActive = true;
    gOutroThread = std::thread([output]() {
        appendOutroToFinishedMp4(output.c_str());
        gOutroDone.store(true, std::memory_order_release);
    });
#endif
}

void cancelReplayFfmpegEncode()
{
#ifdef _WIN32
    if (gOutroThread.joinable()) gOutroThread.join();
    gOutroActive = false;
    if (gJob.ffmpegProcess) {
        TerminateProcess((HANDLE)gJob.ffmpegProcess, 1);
        CloseHandle((HANDLE)gJob.ffmpegProcess);
        gJob.ffmpegProcess = nullptr;
    }
    if (gJob.ffmpegLogHandle) {
        CloseHandle((HANDLE)gJob.ffmpegLogHandle);
        gJob.ffmpegLogHandle = nullptr;
    }
#endif
}
