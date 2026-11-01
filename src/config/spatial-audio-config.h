#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "audio/audio.h"

struct SpatialPoint {
    float distance = 0.0f;
    float gain = 0.0f;
};

enum class SpatialAudioMode {
    Legacy,
    Spatial
};

struct LegacyProfile {
    std::string name;
    float fullVolumeDistance = 50.0f;
    float maxDistance = 100.0f;
    float volumeScale = 1.0f;
};

struct SpatialProfile {
    std::string name;
    bool spatial = true;
    float minDistance = 2.0f;
    float outerDistance = 30.0f;
    float volumeScale = 1.0f;
    float directFadeStart = 50.0f;
    float directFadeEnd = 100.0f;
    std::vector<SpatialPoint> points;
    float reverbWetNear = 0.03f;
    float reverbWetFar = 0.75f;
    float reverbDelayMs = 60.0f;
    float reverbDecay = 0.45f;
    float lpfOpenHz = 20000.0f;
    float lpfOccludedHz = 1800.0f;
    float lpfEnclosedHz = 900.0f;
    float obstructionDirectLoss = 0.55f;
    float obstructionReverbBoost = 0.22f;
    float enclosureDirectLoss = 0.20f;
    float enclosureReverbBoost = 0.22f;
    float probeRadius = 10.0f;

    float gainAt(float distance) const;
};

struct SpatialAudioData {
    bool enabled = true;
    SpatialAudioMode mode = SpatialAudioMode::Legacy;
    float legacyFullVolumeDistance = 50.0f;
    std::unordered_map<int, LegacyProfile> legacyCategories;
    std::unordered_map<std::string, LegacyProfile> legacySounds;
    std::unordered_map<int, SpatialProfile> categories;
    std::unordered_map<std::string, AudioCategory> exactSounds;
    std::vector<std::pair<std::string, AudioCategory>> prefixSounds;

    const SpatialProfile& profileFor(AudioCategory category) const;
    const LegacyProfile& legacyProfileFor(AudioCategory category) const;
    LegacyProfile legacyProfileForSound(const std::string& sound,
                                        AudioCategory category) const;
    AudioCategory resolveCategory(const std::string& sound,
                                  AudioCategory fallback) const;
};

const char* spatialAudioModeName(SpatialAudioMode mode);

class SpatialAudioConfig {
public:
    static SpatialAudioConfig& instance();
    bool load(const std::string& path = "config/audio/spatial-audio.json");
    bool pollReload();
    const SpatialAudioData& data() const { return mData; }

private:
    SpatialAudioConfig();
    SpatialAudioData mData;
    std::string mPath = "config/audio/spatial-audio.json";
    std::filesystem::file_time_type mLastWrite{};
    std::string mLastHash;
    bool mWatchLogged = false;
};

std::string audioCategoryName(AudioCategory category);
bool audioCategoryFromName(const std::string& name, AudioCategory& out);
