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

static ma_engine gEngine;
static bool gAudioInit = false;

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
static bool gSoundDebug = false;

static std::string soundPath(const std::string& name)
{
    std::string path = "assets/sound/" + name;
    if (std::filesystem::path(path).extension().empty())
        path += ".wav";
    if (std::filesystem::exists(path))
        return path;

    if (name == "revolvershoot") return "assets/sound/weapon/revolver/revolvershoot.wav";
    if (name == "revolverbulletadd") return "assets/sound/weapon/revolver/revolverbulletadd.wav";
    if (name == "revolverchamber" || name == "revolverpullback")
        return "assets/sound/weapon/revolver/revolverreload.wav";
    if (name == "gethurt") return "assets/sound/U mimita sound effects.wav - hitting not sure low.wav";
    if (name == "npc_spawn") return "assets/sound/U mimita sound effects.wav - item get.mp3";
    if (name == "npc_death") return "assets/sound/U mimita sound effects.wav - grunt kill madness combat.mp3";
    if (name == "world_impact") return "assets/sound/U mimita sound effects.wav  - hit low 1.wav";
    if (name == "ui/hover") return "assets/sound/ui/click.wav";
    if (name.find("revolver") == 0) return "assets/sound/ui/click.wav";
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
    if (!position) {
        ma_result result = ma_engine_play_sound(&gEngine, path.c_str(), nullptr);
        if (gSoundDebug)
            printf(result == MA_SUCCESS ? "[SOUND] playing event=%s category=2D path=%s\n"
                                        : "[SOUND] failed event=%s path=%s\n",
                   name.c_str(), path.c_str());
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
    }
    ma_sound_start(&active->sound);
    if (gSoundDebug) printf("[SOUND] playing event=%s category=3D path=%s\n", name.c_str(), path.c_str());
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
    startSound(name, volume, pitch, nullptr, 0.0f);
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
    ma_engine_listener_set_position(&gEngine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&gEngine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&gEngine, 0, 0.0f, 0.0f, 1.0f);
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

// mar 8 2026 todo maibe put this in like its own file or own folder idk
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
