// C:\important\quiet\n\mimita-priv-v7\src\audio\audio.cpp
/**
 * purpose
 * functions so we plau adio 
 * and it sound good
 * i think feb 10 2026 was date idk 
 * i wrote dat mar 7 2026
 * also its messy with comments about caht gpt 
 * and the sound pool and sound queue and stuff
 * but i think right now i idont reallu even care
 * i like the fucked up broken bullshti version of it 
 * it gives it flavor 
 * no polished triple AAA game studio here i hate the polished nice clean
 * i want dirty real raw
 */

// audio.cpp
// THIS IS THE ONL PLACE THAT MINIAUDIO_IMPLEMENTATION CAN BE DEFINED 

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"
#include "utils/path_utils.h"
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace {
float gGlobalDuck = 1.0f;
}

void setGlobalDuck(float duck)
{
    gGlobalDuck = std::clamp(duck, 0.0f, 1.0f);
}

float getGlobalDuck()
{
    return gGlobalDuck;
}
#include <functional>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>

#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "config/spatial-audio-config.h"
#include "config/weapon-hitfx-config.h"
#include "audio/audio-acoustics.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "replay/replay.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"

static ma_engine gEngine;
static bool gAudioInit = false;
static bool gServerMode = false;  // when true, audio is completely disabled

static glm::vec3 gLastListenerPosition{0.0f};
static glm::vec3 gLastListenerForward{0.0f, 1.0f, 0.0f};
static const World* gSpatialWorld = nullptr;
static float gSpatialConfigPollTimer = 0.0f;

// cooldown timer for air jump
static float airJumpCooldown = 0.0f;
struct ActiveSound {
    ma_sound sound{};
    ma_decoder decoder{};
    bool initialized = false;
    std::string name;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    float maxDistance = 0.0f;
    float createdTime = 0.0f;
    unsigned int ownerId = 0;
    float endSeconds = 0.0f;
    AudioCategory category = AudioCategory::Impacts;
    SpatialAudioMode mode = SpatialAudioMode::Legacy;
    SpatialProfile profile;
    LegacyProfile legacyProfile;
    bool world = false;
    bool spatial = false;
    float baseVolume = 1.0f;
    float distanceGain = 1.0f;
    float lastDistance = 0.0f;
    float obstruction = 0.0f;
    float enclosure = 0.0f;
    float wetMix = 0.0f;
    float cutoffHz = 20000.0f;
    float nextAcousticUpdate = 0.0f;
    ma_lpf_node lpf{};
    ma_delay_node delay{};
    bool nodesReady = false;
};
static std::vector<std::unique_ptr<ActiveSound>> gActiveSounds;
static void initAudioOnce();
static float gAudioTime = 0.0f;
static bool gSoundDebug = true;

static std::unordered_map<std::string, std::vector<uint8_t>> gSoundFileCache;

// Sound file I/O and decoding preparation run on a worker thread. A cache
// miss during gameplay must not block the main thread on a large WAV read.
static std::mutex gSoundLoadMutex;
static std::condition_variable gSoundLoadCv;
static std::deque<std::string> gSoundLoadQueue;
static std::unordered_set<std::string> gSoundLoadQueued;
static std::thread gSoundLoadThread;
static std::atomic<bool> gSoundLoadStop{false};
static bool gSoundLoadStarted = false;

static void soundLoadWorker();
static void requestSoundCache(const std::string& name);

struct SoundCacheLifetime {
    ~SoundCacheLifetime() {
        gSoundLoadStop.store(true, std::memory_order_relaxed);
        gSoundLoadCv.notify_all();
        if (gSoundLoadThread.joinable())
            gSoundLoadThread.join();
    }
};
static SoundCacheLifetime gSoundCacheLifetime;

static std::string soundPath(const std::string& name)
{
    const auto resolveExisting = [](const std::string& candidate) {
        const std::string resolved = resolveAssetPath(candidate);
        return std::filesystem::exists(resolved) ? resolved : candidate;
    };

    std::string path = "assets/sound/" + name;
    if (std::filesystem::path(path).extension().empty())
        path += ".wav";
    const std::string resolvedPath = resolveAssetPath(path);
    if (std::filesystem::exists(resolvedPath))
        return resolvedPath;

    if (name == "revolvershoot") return "assets/sound/weapon/revolver/revolvershoot.wav";
    if (name == "revolverreload") return "assets/sound/weapon/revolver/revolverreload.wav";
    if (name == "revolverchamber" || name == "revolverpullback")
        return "assets/sound/weapon/revolver/revolverreload.wav";
    if (name == "shotgunshoot") return "assets/sound/weapon/shotgun/shotgunshoot.wav";
    if (name == "shotgunreload") return "assets/sound/weapon/shotgun/shotgunreload.wav";
    if (name == "godballhit") return "assets/sound/weapon/godball/godballhit.wav";
    if (name == "rocketlauncher/rocketshoot") return "assets/sound/weapon/rocketlauncher/rocketshoot.wav";
    if (name == "rocketlauncher/rocketlauncherinair") return "assets/sound/weapon/rocketlauncher/rocketlauncherinair.wav";
    if (name == "rocketlauncher/rocketlauncherexplode") return "assets/sound/weapon/rocketlauncher/rocketlauncherexplode.wav";
    if (name == "rocketlauncher/rocketlauncherreload") return "assets/sound/weapon/rocketlauncher/rocketlauncherreload.wav";
    if (name == "grenadelauncher/grenadelaunchershoot") return "assets/sound/weapon/grenadelauncher/grenadelaunchershoot.wav";
    if (name == "grenadelauncher/grenadelauncherload") return "assets/sound/weapon/grenadelauncher/grenadelauncherload.wav";
    if (name == "grenadelauncher/grenadelauncherexplode") return "assets/sound/weapon/grenadelauncher/grenadelauncherexplode.wav";
    if (name == "spyknifebackstab") return "assets/sound/weapon/spyknife/U spy knife mimita.wav";
    if (name == "entity/falcon/falconswing") return "assets/sound/entity/falcon/falconswing.wav";
    if (name == "swordswordhit1") return "assets/sound/weapon/swordsword/swordswordhit1.wav";
    if (name == "swordswordhit2") return "assets/sound/weapon/swordsword/swordswordhit2.wav";
    if (name == "swordswordhit3") return "assets/sound/weapon/swordsword/swordswordhit3.wav";
    if (name == "swordswordhit4") return "assets/sound/weapon/swordsword/swordswordhit4.wav";
    if (name == "gethurt") return "assets/sound/U mimita sound effects.wav - hitting not sure low.wav";
    if (name == "player_hurt") return "assets/sound/entity/player/hurtsmall.wav";
    if (name == "hitworld") return "assets/sound/hitworld.mp3";
    if (name == "npc_spawn") return "assets/sound/U mimita sound effects.wav - item get.mp3";
    if (name == "npc_death") return resolveExisting("assets/sound/U mimita sound effects.wav - grunt kill madness combat.mp3");
    if (name == "world_impact") return "assets/sound/U mimita sound effects.wav  - hit low 1.wav";
    if (name == "ui/hover") return "assets/sound/ui/click.wav";
    if (name == "serverdisagree") return "assets/sound/serverdisagree.wav";
    return path;
}

static void requestSoundCache(const std::string& name)
{
    if (name.empty())
        return;
    {
        std::lock_guard<std::mutex> lock(gSoundLoadMutex);
        if (gSoundFileCache.find(name) != gSoundFileCache.end())
            return;
        if (!gSoundLoadQueued.insert(name).second)
            return;
        gSoundLoadQueue.push_back(name);
    }
    gSoundLoadCv.notify_one();
}

static void soundLoadWorker()
{
    for (;;) {
        std::string name;
        {
            std::unique_lock<std::mutex> lock(gSoundLoadMutex);
            gSoundLoadCv.wait(lock, [] {
                return gSoundLoadStop.load(std::memory_order_relaxed) ||
                       !gSoundLoadQueue.empty();
            });
            if (gSoundLoadStop.load(std::memory_order_relaxed) &&
                gSoundLoadQueue.empty())
                return;
            name = std::move(gSoundLoadQueue.front());
            gSoundLoadQueue.pop_front();
        }

        std::string path = soundPath(name);
        std::vector<uint8_t> data;
        if (std::filesystem::exists(path)) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (file) {
                const size_t size = (size_t)file.tellg();
                file.seekg(0);
                data.resize(size);
                file.read((char*)data.data(), (std::streamsize)size);
            }
        }

        std::lock_guard<std::mutex> lock(gSoundLoadMutex);
        gSoundFileCache.emplace(std::move(name), std::move(data));
    }
}

static const std::vector<uint8_t>& getCachedSoundData(const std::string& name)
{
    MIMITA_PERF_SCOPE("Audio::StartSound::CacheLookup");
    {
        std::lock_guard<std::mutex> lock(gSoundLoadMutex);
        auto it = gSoundFileCache.find(name);
        if (it != gSoundFileCache.end()) {
            Perf::state().audioPerf.cacheHits++;
            return it->second;
        }
    }
    Perf::state().audioPerf.cacheMisses++;
    // Do not read or decode a missing file on the gameplay thread. Request it
    // for the worker; the current one-shot is silent, the next one is ready.
    requestSoundCache(name);
    static const std::vector<uint8_t> empty;
    return empty;
}

static void uninitVoice(ActiveSound& active)
{
    if (active.initialized) {
        ma_sound_uninit(&active.sound);
        ma_decoder_uninit(&active.decoder);
        active.initialized = false;
    }
    if (active.nodesReady) {
        ma_lpf_node_uninit(&active.lpf, nullptr);
        ma_delay_node_uninit(&active.delay, nullptr);
        active.nodesReady = false;
    }
}

static void setVoiceCutoff(ActiveSound& active, float cutoffHz)
{
    if (!active.nodesReady)
        return;
    cutoffHz = std::clamp(cutoffHz, 200.0f,
                          (float)ma_engine_get_sample_rate(&gEngine) * 0.45f);
    if (std::fabs(cutoffHz - active.cutoffHz) < 1.0f)
        return;
    ma_lpf_config config = ma_lpf_config_init(
        ma_format_f32, 2, ma_engine_get_sample_rate(&gEngine), cutoffHz, 2);
    if (ma_lpf_node_reinit(&config, &active.lpf) == MA_SUCCESS)
        active.cutoffHz = cutoffHz;
}

static float smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0 + 0.0001f)
        return value >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float spatialWetMix(const ActiveSound& active, float distanceFactor)
{
    const SpatialProfile& profile = active.profile;
    float wet = profile.reverbWetNear +
        (profile.reverbWetFar - profile.reverbWetNear) *
            std::max(distanceFactor, std::max(active.obstruction, active.enclosure));
    wet += active.obstruction * profile.obstructionReverbBoost;
    wet += active.enclosure * profile.enclosureReverbBoost;
    return std::clamp(wet, 0.0f, 1.0f);
}

static void applySpatialVoice(ActiveSound& active, bool refreshAcoustics)
{
    if (!active.spatial || !active.initialized)
        return;

    const glm::vec3 offset = active.position - gLastListenerPosition;
    const float distance = glm::length(offset);
    const float gain = active.profile.gainAt(distance);
    active.distanceGain = gain;
    active.lastDistance = distance;

    if (refreshAcoustics) {
        const AcousticEstimate estimate = estimateWorldAcoustics(
            gSpatialWorld, active.profile, active.position,
            gLastListenerPosition);
        active.obstruction = estimate.obstruction;
        active.enclosure = estimate.enclosure;
    }

    const float obstructionLoss =
        1.0f - active.profile.obstructionDirectLoss * active.obstruction;
    const float enclosureLoss =
        1.0f - active.profile.enclosureDirectLoss * active.enclosure;
    const float directGain = gain *
        std::clamp(obstructionLoss, 0.0f, 1.0f) *
        std::clamp(enclosureLoss, 0.0f, 1.0f);
    const float distanceFactor = std::clamp(1.0f - gain, 0.0f, 1.0f);
    const float dryShare = 1.0f - smoothStep(
        active.profile.directFadeStart, active.profile.directFadeEnd, distance);
    const float wet = std::max(spatialWetMix(active, distanceFactor),
                               1.0f - dryShare);

    active.wetMix = wet;
    const PlayerSettings& settings = GetPlayerSettings();
    ma_sound_set_volume(&active.sound,
        std::max(0.0f, active.baseVolume * active.profile.volumeScale *
                       settings.masterVolume * settings.sfxVolume * directGain *
                       gGlobalDuck));

    if (active.nodesReady) {
        ma_delay_node_set_wet(&active.delay, wet);
        ma_delay_node_set_dry(&active.delay, 1.0f - wet);
        float cutoff = active.profile.lpfOpenHz +
            (active.profile.lpfOccludedHz - active.profile.lpfOpenHz) *
                active.obstruction;
        cutoff += (active.profile.lpfEnclosedHz - cutoff) * active.enclosure;
        setVoiceCutoff(active, cutoff);
    }
}

static void applyLegacyVoice(ActiveSound& active)
{
    if (!active.initialized || active.mode != SpatialAudioMode::Legacy)
        return;
    const float distance = glm::length(active.position - gLastListenerPosition);
    active.lastDistance = distance;
    const float full = active.legacyProfile.fullVolumeDistance;
    const float outer = std::max(full + 0.01f, active.legacyProfile.maxDistance);
    active.distanceGain = distance <= full
        ? 1.0f
        : std::clamp(1.0f - (distance - full) / (outer - full), 0.0f, 1.0f);
    active.wetMix = 0.0f;
    const PlayerSettings& settings = GetPlayerSettings();
    ma_sound_set_volume(&active.sound,
        std::max(0.0f, active.baseVolume * active.legacyProfile.volumeScale *
                       settings.masterVolume * settings.sfxVolume * gGlobalDuck));
}

static void logSpatialVoice(const ActiveSound& active)
{
    if (!StructuredLogger::instance().shouldLog(
            StructuredCategory::Audio, StructuredLevel::Important))
        return;
    // Legacy attenuation is the steady-state default for world sounds. Logging
    // every voice produced >10k events in a few minutes and buried gameplay
    // evidence; keep a bounded periodic sample instead.
    if (active.world && active.mode == SpatialAudioMode::Legacy)
    {
        static thread_local uint64_t lastLegacyLogMs = 0;
        const uint64_t now = (uint64_t)std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - lastLegacyLogMs < 10000)
            return;
        lastLegacyLogMs = now;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Audio, StructuredLevel::Important,
        "audio.spatial.applied", active.name,
        !active.world ? "local sound unchanged"
            : (active.mode == SpatialAudioMode::Legacy
                ? "world sound uses legacy attenuation"
                : (active.spatial ? "world sound uses spatial profile"
                                  : "world sound kept local")),
        0,
        nlohmann::json{
            {"sound", active.name},
            {"category", audioCategoryName(active.category)},
            {"mode", spatialAudioModeName(active.mode)},
            {"profile", active.mode == SpatialAudioMode::Legacy
                ? active.legacyProfile.name : active.profile.name},
            {"world", active.world},
            {"spatial", active.spatial},
            {"legacy_full_volume_distance", active.legacyProfile.fullVolumeDistance},
            {"legacy_max_distance", active.legacyProfile.maxDistance},
            {"volume_scale", active.mode == SpatialAudioMode::Legacy
                ? active.legacyProfile.volumeScale : active.profile.volumeScale},
            {"position", {active.position.x, active.position.y, active.position.z}},
            {"listener", {gLastListenerPosition.x, gLastListenerPosition.y,
                          gLastListenerPosition.z}},
            {"direction", active.lastDistance > 0.0001f
                ? std::array<float, 3>{
                    (active.position.x - gLastListenerPosition.x) / active.lastDistance,
                    (active.position.y - gLastListenerPosition.y) / active.lastDistance,
                    (active.position.z - gLastListenerPosition.z) / active.lastDistance}
                : std::array<float, 3>{0.0f, 0.0f, 0.0f}},
            {"distance", active.lastDistance},
            {"distance_gain", active.distanceGain},
            {"obstruction", active.obstruction},
            {"enclosure", active.enclosure},
            {"wet_mix", active.wetMix},
            {"cutoff_hz", active.cutoffHz},
            {"volume", active.baseVolume},
        },
        __FILE__, __LINE__, __func__);
}

static void logSpatialConfig(const char* eventName, bool ok)
{
    if (!StructuredLogger::instance().shouldLog(
            StructuredCategory::Audio, StructuredLevel::Important))
        return;
    const SpatialAudioData& data = SpatialAudioConfig::instance().data();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Audio, StructuredLevel::Important,
        eventName, "", ok ? "spatial audio config accepted"
                          : "spatial audio config rejected; previous snapshot kept",
        0,
        nlohmann::json{
            {"enabled", data.enabled},
            {"mode", spatialAudioModeName(data.mode)},
            {"categories", data.categories.size()},
            {"legacy_categories", data.legacyCategories.size()},
            {"legacy_sounds", data.legacySounds.size()},
            {"sounds", data.exactSounds.size()},
            {"prefixes", data.prefixSounds.size()},
        },
        __FILE__, __LINE__, __func__);
}

static void startSound(const std::string& name, float volume, float pitch,
                       const glm::vec3* position, float maxDistance,
                       float startSeconds, float endSeconds, unsigned int ownerId,
                       AudioCategory category)
{
    MIMITA_PERF_SCOPE("Audio::StartSound");
    { MIMITA_PERF_SCOPE("Audio::StartSound::InitAudio"); initAudioOnce(); }
    if (!gAudioInit) { Perf::state().audioPerf.soundsRejected++; return; }
    std::unique_ptr<ActiveSound> active;
    { MIMITA_PERF_SCOPE("Audio::StartSound::Allocation");
      active = std::make_unique<ActiveSound>(); }
    const std::vector<uint8_t>& cached = getCachedSoundData(name);
    if (cached.empty()) {
        Perf::state().audioPerf.soundsRejected++;
        if (gSoundDebug) printf("[SOUND] invalid path event=%s\n", name.c_str());
        return;
    }
    { MIMITA_PERF_SCOPE("Audio::StartSound::DecoderInit");
      if (ma_decoder_init_memory(cached.data(), cached.size(), nullptr, &active->decoder) != MA_SUCCESS) {
        Perf::state().audioPerf.decoderFailures++;
        Perf::state().audioPerf.soundsRejected++;
        if (gSoundDebug) printf("[SOUND] decoder failed event=%s\n", name.c_str());
        return;
      }
    }
    { MIMITA_PERF_SCOPE("Audio::StartSound::SoundInit");
      ma_sound_config soundConfig = ma_sound_config_init_2(&gEngine);
      soundConfig.pDataSource = &active->decoder;
      soundConfig.volumeSmoothTimeInPCMFrames =
          ma_engine_get_sample_rate(&gEngine) / 20;
      if (ma_sound_init_ex(&gEngine, &soundConfig, &active->sound) != MA_SUCCESS) {
        Perf::state().audioPerf.soundInitFailures++;
        Perf::state().audioPerf.soundsRejected++;
        ma_decoder_uninit(&active->decoder);
        if (gSoundDebug) printf("[SOUND] sound init failed event=%s\n", name.c_str());
        return;
      }
    }
    active->initialized = true;
    active->name = name;
    active->position = position ? *position : glm::vec3(0.0f);
    active->volume = volume;
    active->baseVolume = volume;
    active->pitch = pitch;
    active->maxDistance = maxDistance;
    active->createdTime = gAudioTime;
    active->ownerId = ownerId;
    active->endSeconds = std::max(0.0f, endSeconds);
    active->category = category;

    const SpatialAudioData& spatialData = SpatialAudioConfig::instance().data();
    active->category = spatialData.resolveCategory(name, category);
    active->mode = spatialData.mode;
    active->profile = spatialData.profileFor(active->category);
    active->legacyProfile = spatialData.legacyProfileForSound(name, active->category);
    const bool isWorld = position && active->category != AudioCategory::UI;
    const bool wantsSpatial = isWorld && spatialData.enabled &&
        active->mode == SpatialAudioMode::Spatial && active->profile.spatial;
    active->spatial = wantsSpatial;
    active->world = isWorld;

    { MIMITA_PERF_SCOPE("Audio::StartSound::ConfigureVoice");
      const PlayerSettings& settings = GetPlayerSettings();
      ma_sound_set_volume(&active->sound, std::max(0.0f, volume * settings.masterVolume * settings.sfxVolume * gGlobalDuck));
      ma_sound_set_pitch(&active->sound, std::clamp(pitch, 0.05f, 4.0f));
      if (startSeconds > 0.0f) {
          const ma_uint64 frame = static_cast<ma_uint64>(startSeconds * active->decoder.outputSampleRate);
          ma_sound_seek_to_pcm_frame(&active->sound, frame);
      }
      if (position) {
          ma_sound_set_position(&active->sound, position->x, position->y, position->z);
          ma_sound_set_spatialization_enabled(&active->sound, MA_TRUE);
          if (active->mode == SpatialAudioMode::Legacy && isWorld) {
              ma_sound_set_attenuation_model(&active->sound, ma_attenuation_model_linear);
              ma_sound_set_min_distance(&active->sound,
                  std::max(0.0f, active->legacyProfile.fullVolumeDistance));
              ma_sound_set_max_distance(&active->sound,
                  std::max(active->legacyProfile.fullVolumeDistance + 0.01f,
                           active->legacyProfile.maxDistance));
              ma_sound_set_rolloff(&active->sound, 1.0f);
          } else {
              // The custom category curve owns distance attenuation. Setting the
              // inverse model's min and max equal keeps panning active while its
              // own gain stays at 1.
              ma_sound_set_attenuation_model(&active->sound, ma_attenuation_model_inverse);
              ma_sound_set_min_distance(&active->sound, std::max(1.0f, active->profile.outerDistance));
              ma_sound_set_max_distance(&active->sound, std::max(1.0f, active->profile.outerDistance));
          }
      } else {
          ma_sound_set_spatialization_enabled(&active->sound, MA_FALSE);
      }
    }

    if (active->mode == SpatialAudioMode::Legacy && isWorld) {
        applyLegacyVoice(*active);
    } else if (active->spatial) {
        MIMITA_PERF_SCOPE("Audio::StartSound::SpatialChain");
        const ma_uint32 sampleRate = ma_engine_get_sample_rate(&gEngine);
        const ma_uint32 channels = 2;
        ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(
            channels, sampleRate, active->profile.lpfOpenHz, 2);
        ma_delay_node_config delayConfig = ma_delay_node_config_init(
            channels, sampleRate,
            (ma_uint32)std::max(1.0f,
                active->profile.reverbDelayMs * (float)sampleRate / 1000.0f),
            active->profile.reverbDecay);
        if (ma_lpf_node_init(ma_engine_get_node_graph(&gEngine), &lpfConfig,
                             nullptr, &active->lpf) == MA_SUCCESS) {
            if (ma_delay_node_init(ma_engine_get_node_graph(&gEngine),
                                   &delayConfig, nullptr,
                                   &active->delay) == MA_SUCCESS) {
                ma_node_attach_output_bus((ma_node*)&active->lpf, 0,
                                          (ma_node*)&active->delay, 0);
                ma_node_attach_output_bus((ma_node*)&active->delay, 0,
                                          ma_engine_get_endpoint(&gEngine), 0);
                ma_node_attach_output_bus((ma_node*)&active->sound, 0,
                                          (ma_node*)&active->lpf, 0);
                active->nodesReady = true;
                active->cutoffHz = active->profile.lpfOpenHz;
                applySpatialVoice(*active, true);
            } else {
                ma_lpf_node_uninit(&active->lpf, nullptr);
            }
        }
        if (!active->nodesReady) {
            // Spatialization still works; only the filter/reverb tail is lost.
            active->spatial = false;
            Debug::warn(Debug::Category::Audio,
                "[AUDIO] spatial chain unavailable for %s; using panning only\n",
                name.c_str());
        }
    }

    { MIMITA_PERF_SCOPE("Audio::StartSound::StartVoice"); ma_sound_start(&active->sound); }
    if (gSoundDebug) printf("[SOUND] playing event=%s category=%s\n",
                            name.c_str(), position ? "3D" : "2D");
    Debug::warn(Debug::Category::Audio,
        "[AUDIO] startSound name=%s category=%s pos=%s vol=%.2f maxDist=%.2f spatial=%s\n",
                name.c_str(), audioCategoryName(active->category).c_str(),
                position ? "(set)" : "(null)",
                volume, maxDistance,
                active->spatial ? "ENABLED" : "DISABLED");
    logSpatialVoice(*active);
    { MIMITA_PERF_SCOPE("Audio::StartSound::ContainerInsert");
      gActiveSounds.push_back(std::move(active)); }
    Perf::state().audioPerf.soundsStarted++;
    Perf::state().audioPerf.activeVoices = static_cast<uint32_t>(gActiveSounds.size());
}

static void initAudioOnce()
{
    if (gAudioInit) return;
    if (gServerMode) return;  // no audio on dedicated server

    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        printf("[AUDIO] Engine failed\n");
        return;
    }

    gAudioInit = true;
    printf("[AUDIO] Engine initialized\n");
    const bool spatialConfigOk = SpatialAudioConfig::instance().load();
    logSpatialConfig("audio.spatial.config.loaded", spatialConfigOk);

    // Queue combat sounds for background caching (one per frame in audioUpdate)
    const char* combatSounds[] = {
        "revolvershoot", "revolverreload", "revolverchamber",
        "shotgunshoot", "shotgunreload",
        "weapon/heavyshotgun/heavyshotgunfire",
        "weapon/heavyshotgun/heavyshotgunreload",
        "weapon/heavyshotgun/heavyshotgunequip",
        "godballhit",
        "swordswordhit1", "swordswordhit2", "swordswordhit3", "swordswordhit4",
        "weapon/hafs/hafsequip", "weapon/hafs/hafsswing", "weapon/hafs/hafslunge",
        "weapon/hafs/hafsknockback", "weapon/hafs/hafsWorldHit",
        "spyknifebackstab", "entity/falcon/falconswing",
        "gethurt", "player_hurt", "hitworld",
        "npc_spawn", "npc_death", "world_impact",
        "dash", "jump", "land",
        "ui/hover"
    };
    if (!gSoundLoadStarted) {
        gSoundLoadStarted = true;
        gSoundLoadStop.store(false, std::memory_order_relaxed);
        gSoundLoadThread = std::thread(soundLoadWorker);
    }
    for (const char* sound : combatSounds)
        requestSoundCache(sound);
}

void audioUpdate(float dt)
{
    MIMITA_PERF_SCOPE("Audio::AudioUpdate");
    gAudioTime += dt;
    if (airJumpCooldown > 0.0f)
        airJumpCooldown -= dt;

    // Initialize audio engine early (first frame, fast — no I/O)
    if (!gAudioInit)
        initAudioOnce();

    gSpatialConfigPollTimer += dt;
    if (gSpatialConfigPollTimer >= 1.0f) {
        gSpatialConfigPollTimer = 0.0f;
        if (SpatialAudioConfig::instance().pollReload())
            logSpatialConfig("audio.spatial.config.reloaded", true);
    }

    const size_t voicesBeforeCleanup = gActiveSounds.size();
    { MIMITA_PERF_SCOPE("Audio::Update::RemoveFinishedSounds");
    gActiveSounds.erase(
        std::remove_if(gActiveSounds.begin(), gActiveSounds.end(), [](const std::unique_ptr<ActiveSound>& active) {
            float cursorSeconds = 0.0f;
            const bool reachedConfiguredEnd = active && active->initialized &&
                active->endSeconds > 0.0f &&
                ma_sound_get_cursor_in_seconds(&active->sound, &cursorSeconds) == MA_SUCCESS &&
                cursorSeconds >= active->endSeconds;
            if (!active || !active->initialized || ma_sound_at_end(&active->sound) || reachedConfiguredEnd) {
                if (active) {
                    if (gSoundDebug)
                        DBG(Audio, "stopped event=%s", active->name.c_str());
                    uninitVoice(*active);
                }
                return true;
            }
            return false;
        }),
        gActiveSounds.end()); }
    Perf::state().audioPerf.voicesCleaned += static_cast<uint32_t>(
        voicesBeforeCleanup - gActiveSounds.size());
    Perf::state().audioPerf.activeVoices = static_cast<uint32_t>(gActiveSounds.size());

    for (auto& active : gActiveSounds) {
        if (!active || !active->spatial || !active->initialized)
            continue;
        const bool refresh = gAudioTime >= active->nextAcousticUpdate;
        if (refresh)
            active->nextAcousticUpdate = gAudioTime + 0.25f;
        applySpatialVoice(*active, refresh);
    }
}

AudioManager& AudioManager::instance()
{
    static AudioManager manager;
    return manager;
}

void AudioManager::update(float dt) { audioUpdate(dt); }
void AudioManager::setListener(glm::vec3 pos, glm::vec3 forward) { setAudioListener(pos, forward); }
void AudioManager::setDebug(bool enabled) { gSoundDebug = enabled; }
bool AudioManager::debug() const { return gSoundDebug; }

void AudioManager::play(const AudioEvent& event)
{
    Perf::state().audioPerf.soundsRequested++;
    ReplaySoundEvent replayEvent;
    replayEvent.soundPath = event.name;
    replayEvent.world = event.world;
    replayEvent.position = event.position;
    replayEvent.volume = event.volume;
    replayEvent.pitch = event.pitch;
    replayEvent.maxDistance = event.maxDistance;
    replayEvent.listenerPosition = gLastListenerPosition;
    replayEvent.listenerForward = gLastListenerForward;
    replayEvent.listenerValid = true;
    captureReplaySound(replayEvent);

    glm::vec3 listenerPos = gLastListenerPosition;
    glm::vec3 listenerFwd = gLastListenerForward;
    Debug::warn(Debug::Category::Audio, "[AUDIO] play event=%s world=%d pos=(%.2f %.2f %.2f) vol=%.2f maxDist=%.2f listener=(%.2f %.2f %.2f) listenerFwd=(%.2f %.2f %.2f)\n",
                event.name.c_str(), (int)event.world,
                event.position.x, event.position.y, event.position.z,
                event.volume, event.maxDistance,
                listenerPos.x, listenerPos.y, listenerPos.z,
                listenerFwd.x, listenerFwd.y, listenerFwd.z);

    if (event.retrigger && event.ownerId != 0)
        stopOwner(event.ownerId);
    if (event.world) {
        startSound(event.name, event.volume, event.pitch, &event.position, event.maxDistance,
                   event.startSeconds, event.endSeconds, event.ownerId,
                   event.category);
    } else {
        startSound(event.name, event.volume, event.pitch, nullptr, 0.0f,
                   event.startSeconds, event.endSeconds, event.ownerId,
                   event.category);
    }
}

void AudioManager::stopOwner(unsigned int ownerId)
{
    if (ownerId == 0) return;
    gActiveSounds.erase(
        std::remove_if(gActiveSounds.begin(), gActiveSounds.end(), [ownerId](const std::unique_ptr<ActiveSound>& active) {
            if (!active || active->ownerId != ownerId) return false;
            if (gSoundDebug) DBG(Audio, "stopped event=%s owner=%u", active->name.c_str(), ownerId);
            uninitVoice(*active);
            return true;
        }),
        gActiveSounds.end());
}

void AudioManager::stopAll()
{
    for (auto& active : gActiveSounds) {
        if (active && active->initialized)
            uninitVoice(*active);
    }
    gActiveSounds.clear();
}

void playSound(const std::string& name, float volume)
{
    AudioManager::instance().play({name, AudioCategory::Movement, false, {}, volume});
}

void playEventSound(const std::string& name, float volume)
{
    playSound(name, volume);
}

void playSoundPitched(const std::string& name, float volume, float pitch)
{
    AudioManager::instance().play({name, AudioCategory::Movement, false, {}, volume, pitch});
}

void playWorldSound(const std::string& name, glm::vec3 pos, float volume, float pitch,
                    float maxDistance, AudioCategory category)
{
    AudioEvent event{name, category, true, pos, volume, pitch, maxDistance};
    AudioManager::instance().play(event);
}

unsigned int audioOwnerKey(unsigned int actorId, const std::string& channel)
{
    const size_t channelHash = std::hash<std::string>{}(channel);
    uint32_t key = actorId * 0x9e3779b9u;
    key ^= static_cast<uint32_t>(channelHash);
    key ^= static_cast<uint32_t>(channelHash >> 32);
    key ^= key >> 16;
    key *= 0x85ebca6bu;
    key ^= key >> 13;
    return key == 0 ? 1u : key;
}

void playWorldSoundOwned(const std::string& name, glm::vec3 pos, float volume,
                         float pitch, float maxDistance, unsigned int ownerId,
                         bool retrigger, AudioCategory category)
{
    AudioEvent event{name, category, true, pos, volume, pitch,
                     maxDistance, ownerId, 0.0f, 0.0f, retrigger};
    AudioManager::instance().play(event);
}

glm::vec3 audioListenerPosition()
{
    return gLastListenerPosition;
}

glm::vec3 audioListenerForward()
{
    return gLastListenerForward;
}

void setAudioSpatialWorld(const World* world)
{
    gSpatialWorld = world;
}

void computeImpactAudio(float baseVolume, float distance, float severity,
                        float& outVolume, float& outPitch)
{
    const auto& snd = WeaponHitFxConfig::instance().defaultSound();
    severity = std::clamp(severity, 0.0f, 1.0f);
    float nearFactor = std::clamp(1.0f - distance / snd.nearDistance, 0.0f, 1.0f);
    float volume = baseVolume * (1.0f + nearFactor * snd.volumeNearFactor);
    volume *= (1.0f - snd.volumeSeverityScale) + severity * snd.volumeSeverityScale;
    outVolume = std::max(0.0f, volume);
    outPitch = snd.pitchBase + severity * snd.pitchSeverityScale;
    outPitch = std::clamp(outPitch, snd.pitchMin, snd.pitchMax);
}

void computeImpactAudioConfig(const WeaponHitFxSoundConfig& snd, float distance, float severity,
                               float& outVolume, float& outPitch)
{
    severity = std::clamp(severity, 0.0f, 1.0f);
    float nearFactor = std::clamp(1.0f - distance / snd.nearDistance, 0.0f, 1.0f);
    float volume = snd.baseVolume * (1.0f + nearFactor * snd.volumeNearFactor);
    volume *= (1.0f - snd.volumeSeverityScale) + severity * snd.volumeSeverityScale;
    outVolume = std::max(0.0f, volume);
    outPitch = snd.pitchBase + severity * snd.pitchSeverityScale;
    outPitch = std::clamp(outPitch, snd.pitchMin, snd.pitchMax);
}

void playSoundAt(const std::string& name, glm::vec3 pos, float volume)
{
    playWorldSound(name, pos, volume, 1.0f, 30.0f);
}

void setAudioListener(glm::vec3 pos, glm::vec3 forward)
{
    initAudioOnce();
    if (!gAudioInit) return;
    gLastListenerPosition = pos;
    gLastListenerForward = forward;
    ma_engine_listener_set_position(&gEngine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&gEngine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&gEngine, 0, 0.0f, 0.0f, 1.0f);
    Debug::log(Debug::Category::Audio, "[AUDIO] setAudioListener pos=(%.2f %.2f %.2f) forward=(%.2f %.2f %.2f)\n",
                pos.x, pos.y, pos.z, forward.x, forward.y, forward.z);
}

void playAirJumpSound()
{
    if (airJumpCooldown > 0.0f)
        return;

    playSound("entity/player/doublejump", 1.0f);

    // how much time to wait beofre playing the sound again 
    airJumpCooldown = 0.5f;
}

void playRandomFootstep(float sizeScale)
{
    int r = rand() % 4;
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float vol = sc.scale(1.0f, sc.footstepVolumeExponent, ss);
    float pitch = sc.scale(1.0f, sc.footstepPitchExponent, ss);
    const char* sound;
    switch (r)
    {
        case 0: sound = "entity/player/walk1"; break;
        case 1: sound = "entity/player/walk2"; break;
        case 2: sound = "entity/player/walk3"; break;
        case 3: sound = "entity/player/walk4"; break;
        default: return;
    }
    playSoundPitched(sound, vol, pitch);
}

void playFreezeBeginSound()
{
    playSound("entity/player/freezebegin", 1.0f);
}

void playFreezeHoldSound()
{
    playSound("entity/player/freezehold", 1.0f);
}

void playFreezeEndSound()
{
    playSound("entity/player/freezeend", 1.0f);
}

void playMenuClick()
{
    AudioManager::instance().play({"ui/click", AudioCategory::UI, false, {}, 0.6f});
}

void playMenuHover()
{
    AudioManager::instance().play({"ui/hover", AudioCategory::UI, false, {}, 0.18f, 1.15f});
}

std::string resolveSoundPath(const std::string& name)
{
    return ::soundPath(name);
}

bool decodeAudioToPCM(const std::string& path, std::vector<int16_t>& outPCM,
                      uint32_t& outSampleRate, uint32_t& outChannels,
                      uint32_t targetSampleRate, uint32_t targetChannels)
{
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, targetChannels, targetSampleRate);
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS)
        return false;

    outSampleRate = decoder.outputSampleRate;
    outChannels = decoder.outputChannels;

    ma_uint64 totalFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return false;
    }

    outPCM.resize((size_t)totalFrames * outChannels);
    ma_decoder_read_pcm_frames(&decoder, outPCM.data(), totalFrames, nullptr);
    ma_decoder_uninit(&decoder);
    return true;
}

// ── Preview music track for replay editor ───────────────────
static ma_sound* gReplayMusicSound = nullptr;
static bool gReplayMusicInit = false;

bool playReplayMusicPreview(const std::string& path, float volume)
{
    if (!gAudioInit) { Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Audio not initialized\n"); return false; }
    stopReplayMusicPreview();

    ma_sound* sound = new ma_sound();
    ma_result result = ma_sound_init_from_file(&gEngine, path.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Failed to load preview music: %s (err=%d)\n", path.c_str(), (int)result);
        delete sound;
        return false;
    }

    ma_sound_set_volume(sound, volume);

    // Seek to start (tick 0) — sound is NOT started; caller must resumeReplayMusicPreview()
    ma_sound_seek_to_pcm_frame(sound, 0);

    gReplayMusicSound = sound;
    gReplayMusicInit = true;
    Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Preview music started: %s\n", path.c_str());
    return true;
}

void stopReplayMusicPreview()
{
    if (gReplayMusicSound) {
        ma_sound_stop(gReplayMusicSound);
        ma_sound_uninit(gReplayMusicSound);
        delete gReplayMusicSound;
        gReplayMusicSound = nullptr;
        gReplayMusicInit = false;
        Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Preview music stopped\n");
    }
}

void pauseReplayMusicPreview()
{
    if (gReplayMusicSound && gReplayMusicInit)
        ma_sound_stop(gReplayMusicSound);
}

void resumeReplayMusicPreview()
{
    if (gReplayMusicSound && gReplayMusicInit)
        ma_sound_start(gReplayMusicSound);
}

bool seekReplayMusicPreview(float seconds)
{
    if (!gReplayMusicSound || !gReplayMusicInit) return false;
    ma_uint64 frame = (ma_uint64)(seconds * 48000.0);
    if (ma_sound_seek_to_pcm_frame(gReplayMusicSound, frame) != MA_SUCCESS)
        return false;
    return true;
}

bool isReplayMusicPreviewPlaying()
{
    if (!gReplayMusicSound || !gReplayMusicInit) return false;
    return ma_sound_is_playing(gReplayMusicSound);
}

float getReplayMusicPreviewDuration()
{
    if (!gReplayMusicSound || !gReplayMusicInit) return 0.0f;
    ma_uint64 totalFrames;
    if (ma_sound_get_length_in_pcm_frames(gReplayMusicSound, &totalFrames) != MA_SUCCESS)
        return 0.0f;
    return (float)totalFrames / 48000.0f;
}

void setReplayMusicPreviewSpeed(float speed)
{
    if (!gReplayMusicSound || !gReplayMusicInit) return;
    float clamped = std::clamp(speed, 0.01f, 100.0f);
    ma_sound_set_pitch(gReplayMusicSound, clamped);
}

void setReplayMusicPreviewVolume(float volume)
{
    if (!gReplayMusicSound || !gReplayMusicInit) return;
    float clamped = std::clamp(volume, 0.0f, 10.0f);
    ma_sound_set_volume(gReplayMusicSound, clamped);
}

float getAudioFileDuration(const std::string& path)
{
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 48000);
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS)
        return 0.0f;
    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    ma_decoder_uninit(&decoder);
    return (float)totalFrames / 48000.0f;
}

void setServerAudioMode(bool enabled)
{
    gServerMode = enabled;
    if (enabled && gAudioInit) {
        for (auto& s : gActiveSounds) {
            if (s)
                uninitVoice(*s);
        }
        gActiveSounds.clear();
        ma_engine_uninit(&gEngine);
        gAudioInit = false;
        printf("[AUDIO] Server mode: audio disabled\n");
    }
}
