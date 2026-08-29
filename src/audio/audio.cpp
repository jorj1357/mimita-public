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
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_map>
#include <fstream>

#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "config/weapon-hitfx-config.h"
#include "debug/debug-log.h"
#include "replay/replay.h"
#include "perf/perf-spike.h"

static ma_engine gEngine;
static bool gAudioInit = false;
static bool gServerMode = false;  // when true, audio is completely disabled

static glm::vec3 gLastListenerPosition{0.0f};
static glm::vec3 gLastListenerForward{0.0f, 1.0f, 0.0f};

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
};
static std::vector<std::unique_ptr<ActiveSound>> gActiveSounds;
static void initAudioOnce();
static float gAudioTime = 0.0f;
static bool gSoundDebug = true;

static std::unordered_map<std::string, std::vector<uint8_t>> gSoundFileCache;

// Background cache queue: one sound loaded per frame in audioUpdate()
static std::vector<const char*> gSoundCacheQueue;
static size_t gSoundCacheIndex = 0;
static bool gSoundCacheComplete = false;

static std::string soundPath(const std::string& name)
{
    std::string path = "assets/sound/" + name;
    if (std::filesystem::path(path).extension().empty())
        path += ".wav";
    if (std::filesystem::exists(path))
        return path;

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
    if (name == "npc_death") return "assets/sound/U mimita sound effects.wav - grunt kill madness combat.mp3";
    if (name == "world_impact") return "assets/sound/U mimita sound effects.wav  - hit low 1.wav";
    if (name == "ui/hover") return "assets/sound/ui/click.wav";
    if (name == "serverdisagree") return "assets/sound/serverdisagree.wav";
    return path;
}

static const std::vector<uint8_t>& getCachedSoundData(const std::string& name)
{
    auto it = gSoundFileCache.find(name);
    if (it != gSoundFileCache.end())
        return it->second;
    std::string path = soundPath(name);
    if (!std::filesystem::exists(path)) {
        static std::vector<uint8_t> empty;
        return empty;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        static std::vector<uint8_t> empty;
        return empty;
    }
    size_t size = (size_t)file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read((char*)data.data(), size);
    auto result = gSoundFileCache.emplace(name, std::move(data));
    return result.first->second;
}

static void startSound(const std::string& name, float volume, float pitch,
                       const glm::vec3* position, float maxDistance)
{
    MIMITA_PERF_SCOPE("Audio::StartSound");
    initAudioOnce();
    if (!gAudioInit) return;
    auto active = std::make_unique<ActiveSound>();
    const std::vector<uint8_t>& cached = getCachedSoundData(name);
    if (cached.empty()) {
        if (gSoundDebug) printf("[SOUND] invalid path event=%s\n", name.c_str());
        return;
    }
    if (ma_decoder_init_memory(cached.data(), cached.size(), nullptr, &active->decoder) != MA_SUCCESS) {
        if (gSoundDebug) printf("[SOUND] decoder failed event=%s\n", name.c_str());
        return;
    }
    if (ma_sound_init_from_data_source(&gEngine, &active->decoder, 0, nullptr, &active->sound) != MA_SUCCESS) {
        ma_decoder_uninit(&active->decoder);
        if (gSoundDebug) printf("[SOUND] sound init failed event=%s\n", name.c_str());
        return;
    }
    active->initialized = true;
    active->name = name;
    active->position = position ? *position : glm::vec3(0.0f);
    active->volume = volume;
    active->pitch = pitch;
    active->maxDistance = maxDistance;
    active->createdTime = gAudioTime;
    const PlayerSettings& settings = GetPlayerSettings();
    ma_sound_set_volume(&active->sound, std::max(0.0f, volume * settings.masterVolume * settings.sfxVolume));
    ma_sound_set_pitch(&active->sound, std::clamp(pitch, 0.25f, 3.0f));
    if (position) {
        ma_sound_set_position(&active->sound, position->x, position->y, position->z);
        ma_sound_set_spatialization_enabled(&active->sound, MA_TRUE);
        ma_sound_set_attenuation_model(&active->sound, ma_attenuation_model_linear);
        ma_sound_set_min_distance(&active->sound, 2.0f);
        ma_sound_set_max_distance(&active->sound, std::max(1.0f, maxDistance));
    } else {
        ma_sound_set_spatialization_enabled(&active->sound, MA_FALSE);
    }
    ma_sound_start(&active->sound);
    if (gSoundDebug) printf("[SOUND] playing event=%s category=%s\n",
                            name.c_str(), position ? "3D" : "2D");
    Debug::warn(Debug::Category::Audio, "[AUDIO] startSound name=%s pos=%s vol=%.2f maxDist=%.2f spatial=%s\n",
                name.c_str(),
                position ? "(set)" : "(null)",
                volume, maxDistance,
                position ? "ENABLED" : "DISABLED");
    gActiveSounds.push_back(std::move(active));
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

    // Queue combat sounds for background caching (one per frame in audioUpdate)
    const char* combatSounds[] = {
        "revolvershoot", "revolverreload", "revolverchamber",
        "shotgunshoot", "shotgunreload",
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
    gSoundCacheQueue.assign(combatSounds, combatSounds + sizeof(combatSounds) / sizeof(combatSounds[0]));
    gSoundCacheIndex = 0;
    gSoundCacheComplete = false;
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

    // Background cache: one sound per frame, never block
    if (gAudioInit && !gSoundCacheComplete && gSoundCacheIndex < gSoundCacheQueue.size())
    {
        getCachedSoundData(gSoundCacheQueue[gSoundCacheIndex]);
        ++gSoundCacheIndex;
        if (gSoundCacheIndex >= gSoundCacheQueue.size())
        {
            gSoundCacheComplete = true;
            printf("[AUDIO] Combat sounds cached: %zu files\n", gSoundFileCache.size());
        }
    }

    gActiveSounds.erase(
        std::remove_if(gActiveSounds.begin(), gActiveSounds.end(), [](const std::unique_ptr<ActiveSound>& active) {
            if (!active || !active->initialized || ma_sound_at_end(&active->sound)) {
                if (active && active->initialized) {
                    ma_sound_uninit(&active->sound);
                    ma_decoder_uninit(&active->decoder);
                }
                if (gSoundDebug && active)
                    printf("[SOUND] stopped event=%s\n", active->name.c_str());
                return true;
            }
            return false;
        }),
        gActiveSounds.end());
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

    if (event.world) {
        startSound(event.name, event.volume, event.pitch, &event.position, event.maxDistance);
        if (!gActiveSounds.empty())
            gActiveSounds.back()->ownerId = event.ownerId;
    } else {
        startSound(event.name, event.volume, event.pitch, nullptr, 0.0f);
    }
}

void AudioManager::stopOwner(unsigned int ownerId)
{
    if (ownerId == 0) return;
    gActiveSounds.erase(
        std::remove_if(gActiveSounds.begin(), gActiveSounds.end(), [ownerId](const std::unique_ptr<ActiveSound>& active) {
            if (!active || active->ownerId != ownerId) return false;
            if (active->initialized) {
                ma_sound_uninit(&active->sound);
                ma_decoder_uninit(&active->decoder);
            }
            if (gSoundDebug) printf("[SOUND] stopped event=%s owner=%u\n", active->name.c_str(), ownerId);
            return true;
        }),
        gActiveSounds.end());
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

void playWorldSound(const std::string& name, glm::vec3 pos, float volume, float pitch, float maxDistance)
{
    AudioEvent event{name, AudioCategory::Impacts, true, pos, volume, pitch, maxDistance};
    AudioManager::instance().play(event);
}

glm::vec3 audioListenerPosition()
{
    return gLastListenerPosition;
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
            if (s->initialized) {
                ma_sound_uninit(&s->sound);
                ma_decoder_uninit(&s->decoder);
            }
        }
        gActiveSounds.clear();
        ma_engine_uninit(&gEngine);
        gAudioInit = false;
        printf("[AUDIO] Server mode: audio disabled\n");
    }
}
