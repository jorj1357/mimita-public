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
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "replay/replay.h"

static ma_engine gEngine;
static bool gAudioInit = false;

static glm::vec3 gLastListenerPosition{0.0f};
static glm::vec3 gLastListenerForward{0.0f, 1.0f, 0.0f};

// cooldown timer for air jump
static float airJumpCooldown = 0.0f;
struct ActiveSound {
    ma_sound sound{};
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
    return path;
}

static void startSound(const std::string& name, float volume, float pitch,
                       const glm::vec3* position, float maxDistance)
{
    initAudioOnce();
    if (!gAudioInit) return;
    auto active = std::make_unique<ActiveSound>();
    std::string path = soundPath(name);
    if (!std::filesystem::exists(path)) {
        if (gSoundDebug) printf("[SOUND] invalid path event=%s path=%s\n", name.c_str(), path.c_str());
        return;
    }
    if (ma_sound_init_from_file(&gEngine, path.c_str(), 0, nullptr, nullptr, &active->sound) != MA_SUCCESS) {
        if (gSoundDebug) printf("[SOUND] failed event=%s path=%s\n", name.c_str(), path.c_str());
        return;
    }
    if (gSoundDebug) printf("[SOUND] loaded event=%s path=%s\n", name.c_str(), path.c_str());
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
        ma_sound_set_min_distance(&active->sound, 1.0f);
        ma_sound_set_max_distance(&active->sound, std::max(1.0f, maxDistance));
    } else {
        ma_sound_set_spatialization_enabled(&active->sound, MA_FALSE);
    }
    ma_sound_start(&active->sound);
    if (gSoundDebug) printf("[SOUND] playing event=%s category=%s path=%s\n",
                            name.c_str(), position ? "3D" : "2D", path.c_str());
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

    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        printf("[AUDIO] Engine failed\n");
        return;
    }

    gAudioInit = true;
    printf("[AUDIO] Engine initialized\n");

    // Preload all combat sounds so first-use playback has no file I/O hitch
    const char* combatSounds[] = {
        "revolvershoot", "revolverreload", "revolverchamber",
        "shotgunshoot", "shotgunreload",
        "godballhit",
        "swordswordhit1", "swordswordhit2", "swordswordhit3", "swordswordhit4",
        "gethurt", "player_hurt", "hitworld",
        "npc_spawn", "npc_death", "world_impact",
        "dash", "jump", "land",
        "ui/hover"
    };
    for (const char* name : combatSounds)
    {
        std::string path = soundPath(name);
        if (std::filesystem::exists(path))
        {
            ma_sound preload;
            if (ma_sound_init_from_file(&gEngine, path.c_str(), MA_SOUND_FLAG_ASYNC, nullptr, nullptr, &preload) == MA_SUCCESS)
            {
                ma_sound_uninit(&preload);
            }
        }
    }
    printf("[AUDIO] Combat sounds preloaded\n");
}

void audioUpdate(float dt)
{
    gAudioTime += dt;
    if (airJumpCooldown > 0.0f)
        airJumpCooldown -= dt;
    gActiveSounds.erase(
        std::remove_if(gActiveSounds.begin(), gActiveSounds.end(), [](const std::unique_ptr<ActiveSound>& active) {
            if (!active || !active->initialized || ma_sound_at_end(&active->sound)) {
                if (active && active->initialized)
                    ma_sound_uninit(&active->sound);
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
            if (active->initialized) ma_sound_uninit(&active->sound);
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

void playRandomFootstep()
{
    int r = rand() % 4;

    switch (r)
    {
        case 0: playSound("entity/player/walk1",1.0f); break;
        case 1: playSound("entity/player/walk2",1.0f); break;
        case 2: playSound("entity/player/walk3",1.0f); break;
        case 3: playSound("entity/player/walk4",1.0f); break;
    }
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
    ma_result result = ma_sound_init_from_file(&gEngine, path.c_str(), MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Failed to load preview music: %s (err=%d)\n", path.c_str(), (int)result);
        delete sound;
        return false;
    }

    ma_sound_set_volume(sound, volume);
    // Set to loop if we want infinite preview — for now, no loop (stops at end)
    ma_sound_start(sound);

    // Seek to start (tick 0)
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
