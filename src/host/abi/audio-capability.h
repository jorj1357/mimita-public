// 2026-11-12
// Versioned, bounded audio capability for a game.dll generation.
//
// The game generation never touches the audio device. It forwards typed audio
// requests to the resident AudioManager/MusicManager through this capability.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kAudioCapabilityVersion = 1u;

enum class AudioOp : std::uint32_t {
    AudioPlay = 1,
    AudioStopOwner = 2,
    AudioStopAll = 3,
    AudioSetDebug = 4,
    AudioDebug = 5,

    MusicUpdate = 6,
    MusicEnterMenuMode = 7,
    MusicEnterGameMode = 8,
    MusicPlayModeTrack = 9,
    MusicStopModeTrack = 10,
    MusicPause = 11,
    MusicResume = 12,
    MusicSkip = 13,
    MusicPrevious = 14,
    MusicSetPlaylistMode = 15,
    MusicSetPersonalMusicOverride = 16,
    MusicStop = 17,
    MusicReload = 18,
    MusicSetVolume = 19,
    MusicVolume = 20,
    MusicSetPlaybackSpeed = 21,
    MusicPlaybackSpeed = 22,
    MusicSetMuted = 23,
    MusicMuted = 24,
    MusicIsPlaying = 25,
    MusicCurrentTrackInfo = 26,

    PlayWorldSound = 27,
    PlayWorldSoundOwned = 28,
    AudioListenerPosition = 29,
    ComputeImpactAudio = 30,
};

struct AudioEventPayload {
    char name[256];
    std::int32_t category;
    std::int32_t world;
    float position[3];
    float volume;
    float pitch;
    float maxDistance;
    std::uint32_t ownerId;
    float startSeconds;
    float endSeconds;
    std::int32_t retrigger;
};

struct WorldSoundPayload {
    char name[256];
    float position[3];
    float volume;
    float pitch;
    float maxDistance;
    std::uint32_t ownerId;
    std::int32_t retrigger;
    std::int32_t category;
};

struct ImpactAudioPayload {
    float baseVolume;
    float distance;
    float severity;
    float outVolume;
    float outPitch;
};

struct AudioCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Returns 1 when handled, 0 when unsupported/failed. input/output are plain
    // bounded values valid only for the call; the host must not retain them.
    std::int32_t (*invoke)(void* hostUserData, AudioOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
