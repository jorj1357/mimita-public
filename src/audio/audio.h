// C:\important\quiet\n\mimita-priv-v7\src\audio\audio.h
/**
 * purpose
 * audio header file 
 * so we can pla sounds and it sound good and cool
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

enum class AudioCategory { Movement, UI, Weapons, NPC, Impacts, Ambient };

struct AudioEvent {
    std::string name;
    AudioCategory category = AudioCategory::Impacts;
    bool world = false;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    float maxDistance = 30.0f;
    unsigned int ownerId = 0;
};

class AudioManager {
public:
    static AudioManager& instance();
    void update(float dt);
    void setListener(glm::vec3 pos, glm::vec3 forward);
    void play(const AudioEvent& event);
    void stopOwner(unsigned int ownerId);
    void setDebug(bool enabled);
    bool debug() const;
};

void audioUpdate(float dt);

void playFreezeBeginSound();
void playFreezeHoldSound();
void playFreezeEndSound();

void playSound(const std::string& name, float volume = 1.0f);
void playEventSound(const std::string& name, float volume = 1.0f);
void playSoundPitched(const std::string& name, float volume, float pitch);
void playWorldSound(const std::string& name, glm::vec3 pos, float volume = 1.0f,
                    float pitch = 1.0f, float maxDistance = 30.0f);
void setAudioListener(glm::vec3 pos, glm::vec3 forward);
void playAirJumpSound();
void playSoundAt(const std::string& name, glm::vec3 pos, float volume = 1.0f);
void playRandomFootstep(float sizeScale = 1.0f);

// Get current audio listener world position
glm::vec3 audioListenerPosition();

// Compute volume/pitch for impact audio based on distance and severity.
// severity: 0=graze, 1=direct center-mass hit
void computeImpactAudio(float baseVolume, float distance, float severity,
                        float& outVolume, float& outPitch);

void playMenuClick();
void playMenuHover();

// Resolve a sound name (e.g. "revolvershoot") to its filesystem path.
// Returns the path if the file exists, or empty string if not found.
std::string resolveSoundPath(const std::string& name);

// Decode an audio file to 16-bit PCM at target sample rate / channels.
// Returns false on failure. Default: 48000 Hz stereo.
bool decodeAudioToPCM(const std::string& path, std::vector<int16_t>& outPCM,
                      uint32_t& outSampleRate, uint32_t& outChannels,
                      uint32_t targetSampleRate = 48000, uint32_t targetChannels = 2);

// Preview music track for replay editor (uses miniaudio engine internally)
bool playReplayMusicPreview(const std::string& path, float volume = 1.0f);
void stopReplayMusicPreview();
void pauseReplayMusicPreview();
void resumeReplayMusicPreview();
bool seekReplayMusicPreview(float seconds);
bool isReplayMusicPreviewPlaying();
float getReplayMusicPreviewDuration();
void setReplayMusicPreviewSpeed(float speed);
void setReplayMusicPreviewVolume(float volume);
float getAudioFileDuration(const std::string& path);
