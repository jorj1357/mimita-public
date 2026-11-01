#include "config/spatial-audio-config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

SpatialProfile makeProfile(const char* name, bool spatial, float minDistance,
                           float outerDistance,
                           std::initializer_list<SpatialPoint> points)
{
    SpatialProfile profile;
    profile.name = name;
    profile.spatial = spatial;
    profile.minDistance = minDistance;
    profile.outerDistance = outerDistance;
    profile.points.assign(points.begin(), points.end());
    return profile;
}

float readFloat(const json& value, const char* key, float fallback)
{
    return value.contains(key) && value[key].is_number()
        ? value[key].get<float>() : fallback;
}

bool readBool(const json& value, const char* key, bool fallback)
{
    return value.contains(key) && value[key].is_boolean()
        ? value[key].get<bool>() : fallback;
}

SpatialAudioData makeDefaultData()
{
    SpatialAudioData data;

    SpatialProfile weapons = makeProfile("Weapons", true, 2.0f, 10000.0f, {
        {2.0f, 1.0f}, {50.0f, 1.0f}, {100.0f, 0.35f},
        {500.0f, 0.05f}, {1000.0f, 0.02f}, {10000.0f, 0.0f}
    });
    weapons.directFadeStart = 50.0f;
    weapons.directFadeEnd = 150.0f;
    weapons.obstructionDirectLoss = 0.80f;
    weapons.obstructionReverbBoost = 0.35f;
    weapons.enclosureDirectLoss = 0.50f;
    weapons.enclosureReverbBoost = 0.35f;
    weapons.reverbWetNear = 0.03f;
    weapons.reverbWetFar = 0.95f;
    weapons.reverbDelayMs = 70.0f;
    weapons.reverbDecay = 0.50f;
    weapons.probeRadius = 12.0f;

    SpatialProfile impacts = makeProfile("Impacts", true, 1.0f, 300.0f, {
        {1.0f, 1.0f}, {50.0f, 1.0f}, {80.0f, 0.15f}, {300.0f, 0.0f}
    });
    impacts.directFadeStart = 50.0f;
    impacts.directFadeEnd = 100.0f;
    impacts.obstructionDirectLoss = 0.80f;
    impacts.obstructionReverbBoost = 0.35f;
    impacts.enclosureDirectLoss = 0.50f;
    impacts.enclosureReverbBoost = 0.35f;
    impacts.reverbWetNear = 0.02f;
    impacts.reverbWetFar = 0.95f;
    impacts.reverbDelayMs = 45.0f;
    impacts.reverbDecay = 0.40f;
    impacts.probeRadius = 6.0f;

    SpatialProfile movement = makeProfile("Movement", true, 1.0f, 150.0f, {
        {1.0f, 1.0f}, {50.0f, 1.0f}, {80.0f, 0.10f}, {150.0f, 0.0f}
    });
    movement.directFadeStart = 50.0f;
    movement.directFadeEnd = 80.0f;
    movement.obstructionDirectLoss = 0.80f;
    movement.obstructionReverbBoost = 0.35f;
    movement.enclosureDirectLoss = 0.50f;
    movement.enclosureReverbBoost = 0.35f;
    movement.reverbWetNear = 0.02f;
    movement.reverbWetFar = 0.95f;
    movement.reverbDelayMs = 35.0f;
    movement.reverbDecay = 0.32f;
    movement.probeRadius = 5.0f;

    SpatialProfile npc = makeProfile("NPC", true, 2.0f, 300.0f, {
        {2.0f, 1.0f}, {50.0f, 1.0f}, {100.0f, 0.20f}, {300.0f, 0.0f}
    });
    npc.directFadeStart = 50.0f;
    npc.directFadeEnd = 120.0f;
    npc.obstructionDirectLoss = 0.80f;
    npc.obstructionReverbBoost = 0.35f;
    npc.enclosureDirectLoss = 0.50f;
    npc.enclosureReverbBoost = 0.35f;
    npc.reverbWetNear = 0.02f;
    npc.reverbWetFar = 0.95f;
    npc.reverbDelayMs = 55.0f;
    npc.reverbDecay = 0.42f;
    npc.probeRadius = 8.0f;

    SpatialProfile ambient = makeProfile("Ambient", true, 1.0f, 5000.0f, {
        {1.0f, 1.0f}, {50.0f, 1.0f}, {500.0f, 0.20f}, {2000.0f, 0.03f}, {5000.0f, 0.0f}
    });
    ambient.directFadeStart = 50.0f;
    ambient.directFadeEnd = 500.0f;
    ambient.obstructionDirectLoss = 0.80f;
    ambient.obstructionReverbBoost = 0.35f;
    ambient.enclosureDirectLoss = 0.50f;
    ambient.enclosureReverbBoost = 0.35f;
    ambient.reverbWetNear = 0.05f;
    ambient.reverbWetFar = 0.85f;
    ambient.reverbDelayMs = 110.0f;
    ambient.reverbDecay = 0.55f;
    ambient.probeRadius = 20.0f;

    SpatialProfile ui = makeProfile("UI", false, 1.0f, 2.0f, {
        {1.0f, 1.0f}, {2.0f, 0.0f}
    });

    data.categories[(int)AudioCategory::Weapons] = weapons;
    data.categories[(int)AudioCategory::Impacts] = impacts;
    data.categories[(int)AudioCategory::Movement] = movement;
    data.categories[(int)AudioCategory::NPC] = npc;
    data.categories[(int)AudioCategory::Ambient] = ambient;
    data.categories[(int)AudioCategory::UI] = ui;

    data.mode = SpatialAudioMode::Legacy;
    data.legacyFullVolumeDistance = 50.0f;
    const auto legacyProfile = [](const char* name, float maxDistance) {
        LegacyProfile profile;
        profile.name = name;
        profile.fullVolumeDistance = 50.0f;
        profile.maxDistance = maxDistance;
        profile.volumeScale = 1.0f;
        return profile;
    };
    data.legacyCategories[(int)AudioCategory::Weapons] =
        legacyProfile("Weapons", 150.0f);
    data.legacyCategories[(int)AudioCategory::Impacts] =
        legacyProfile("Impacts", 100.0f);
    data.legacyCategories[(int)AudioCategory::Movement] =
        legacyProfile("Movement", 80.0f);
    data.legacyCategories[(int)AudioCategory::NPC] =
        legacyProfile("NPC", 120.0f);
    data.legacyCategories[(int)AudioCategory::Ambient] =
        legacyProfile("Ambient", 1000.0f);
    data.legacyCategories[(int)AudioCategory::UI] =
        legacyProfile("UI", 2.0f);

    data.exactSounds = {
        {"revolvershoot", AudioCategory::Weapons},
        {"shotgunshoot", AudioCategory::Weapons},
        {"godballhit", AudioCategory::Weapons},
        {"npc_spawn", AudioCategory::NPC},
        {"npc_death", AudioCategory::NPC},
        {"hitworld", AudioCategory::Impacts},
        {"world_impact", AudioCategory::Impacts},
        {"player_hurt", AudioCategory::Impacts},
        {"ui/click", AudioCategory::UI},
        {"ui/hover", AudioCategory::UI},
    };

    data.prefixSounds = {
        {"entity/player/walk", AudioCategory::Movement},
        {"entity/player/jump", AudioCategory::Movement},
        {"entity/player/dash", AudioCategory::Movement},
        {"entity/player/land", AudioCategory::Movement},
        {"entity/player/freeze", AudioCategory::Movement},
        {"entity/player/doublejump", AudioCategory::Movement},
        {"entity/player/spawning", AudioCategory::Movement},
        {"weapon/", AudioCategory::Weapons},
        {"npc_", AudioCategory::NPC},
        {"ui/", AudioCategory::UI},
        {"hit", AudioCategory::Impacts},
        {"impact", AudioCategory::Impacts},
        {"rain", AudioCategory::Ambient},
        {"ambient", AudioCategory::Ambient},
    };

    std::sort(data.prefixSounds.begin(), data.prefixSounds.end(),
        [](const auto& a, const auto& b) {
            return a.first.size() > b.first.size();
        });
    return data;
}

SpatialProfile parseProfile(const json& value, const SpatialProfile& fallback)
{
    SpatialProfile profile = fallback;
    profile.spatial = readBool(value, "spatial", profile.spatial);
    profile.minDistance = readFloat(value, "minDistance", profile.minDistance);
    profile.outerDistance = readFloat(value, "outerDistance", profile.outerDistance);
    profile.volumeScale = readFloat(value, "volumeScale", profile.volumeScale);
    profile.directFadeStart = readFloat(value, "directFadeStart", profile.directFadeStart);
    profile.directFadeEnd = readFloat(value, "directFadeEnd", profile.directFadeEnd);

    if (value.contains("points") && value["points"].is_array()) {
        std::vector<SpatialPoint> points;
        for (const json& point : value["points"]) {
            if (!point.is_array() || point.size() < 2)
                continue;
            if (!point[0].is_number() || !point[1].is_number())
                continue;
            points.push_back({point[0].get<float>(), point[1].get<float>()});
        }
        if (points.size() >= 2) {
            std::sort(points.begin(), points.end(),
                [](const SpatialPoint& a, const SpatialPoint& b) {
                    return a.distance < b.distance;
                });
            profile.points = std::move(points);
        }
    }

    if (value.contains("reverb") && value["reverb"].is_object()) {
        const json& reverb = value["reverb"];
        profile.reverbWetNear = readFloat(reverb, "wetNear", profile.reverbWetNear);
        profile.reverbWetFar = readFloat(reverb, "wetFar", profile.reverbWetFar);
        profile.reverbDelayMs = readFloat(reverb, "delayMs", profile.reverbDelayMs);
        profile.reverbDecay = readFloat(reverb, "decay", profile.reverbDecay);
    }
    if (value.contains("muffle") && value["muffle"].is_object()) {
        const json& muffle = value["muffle"];
        profile.lpfOpenHz = readFloat(muffle, "openHz", profile.lpfOpenHz);
        profile.lpfOccludedHz = readFloat(muffle, "occludedHz", profile.lpfOccludedHz);
        profile.lpfEnclosedHz = readFloat(muffle, "enclosedHz", profile.lpfEnclosedHz);
    }
    if (value.contains("obstruction") && value["obstruction"].is_object()) {
        const json& obstruction = value["obstruction"];
        profile.obstructionDirectLoss = readFloat(
            obstruction, "directLoss", profile.obstructionDirectLoss);
        profile.obstructionReverbBoost = readFloat(
            obstruction, "reverbBoost", profile.obstructionReverbBoost);
    }
    if (value.contains("enclosure") && value["enclosure"].is_object()) {
        const json& enclosure = value["enclosure"];
        profile.enclosureDirectLoss = readFloat(
            enclosure, "directLoss", profile.enclosureDirectLoss);
        profile.enclosureReverbBoost = readFloat(
            enclosure, "reverbBoost", profile.enclosureReverbBoost);
        profile.probeRadius = readFloat(
            enclosure, "probeRadius", profile.probeRadius);
    }

    profile.minDistance = std::max(0.0f, profile.minDistance);
    profile.outerDistance = std::max(profile.minDistance, profile.outerDistance);
    if (!profile.points.empty())
        profile.outerDistance = std::max(profile.outerDistance,
                                         profile.points.back().distance);
    profile.volumeScale = std::clamp(profile.volumeScale, 0.0f, 4.0f);
    profile.directFadeStart = std::max(0.0f, profile.directFadeStart);
    profile.directFadeEnd = std::max(profile.directFadeStart + 0.01f,
                                     profile.directFadeEnd);
    profile.reverbWetNear = std::clamp(profile.reverbWetNear, 0.0f, 1.0f);
    profile.reverbWetFar = std::clamp(profile.reverbWetFar, profile.reverbWetNear, 1.0f);
    profile.reverbDelayMs = std::clamp(profile.reverbDelayMs, 1.0f, 500.0f);
    profile.reverbDecay = std::clamp(profile.reverbDecay, 0.0f, 0.98f);
    profile.lpfOpenHz = std::clamp(profile.lpfOpenHz, 200.0f, 20000.0f);
    profile.lpfOccludedHz = std::clamp(profile.lpfOccludedHz, 200.0f, profile.lpfOpenHz);
    profile.lpfEnclosedHz = std::clamp(profile.lpfEnclosedHz, 200.0f, profile.lpfOccludedHz);
    profile.obstructionDirectLoss = std::clamp(profile.obstructionDirectLoss, 0.0f, 1.0f);
    profile.obstructionReverbBoost = std::clamp(profile.obstructionReverbBoost, 0.0f, 1.0f);
    profile.enclosureDirectLoss = std::clamp(profile.enclosureDirectLoss, 0.0f, 1.0f);
    profile.enclosureReverbBoost = std::clamp(profile.enclosureReverbBoost, 0.0f, 1.0f);
    profile.probeRadius = std::clamp(profile.probeRadius, 0.0f, 100.0f);
    return profile;
}

LegacyProfile parseLegacyProfile(const json& value,
                                 const LegacyProfile& fallback,
                                 float globalFullVolumeDistance)
{
    LegacyProfile profile = fallback;
    profile.fullVolumeDistance = readFloat(
        value, "fullVolumeDistance",
        globalFullVolumeDistance > 0.0f
            ? globalFullVolumeDistance : profile.fullVolumeDistance);
    profile.maxDistance = readFloat(
        value, "maxDistance", profile.maxDistance);
    profile.volumeScale = readFloat(
        value, "volumeScale", profile.volumeScale);
    profile.fullVolumeDistance = std::max(0.0f, profile.fullVolumeDistance);
    profile.maxDistance = std::max(profile.fullVolumeDistance + 0.01f,
                                   profile.maxDistance);
    profile.volumeScale = std::clamp(profile.volumeScale, 0.0f, 4.0f);
    return profile;
}

} // namespace

std::string audioCategoryName(AudioCategory category)
{
    switch (category) {
        case AudioCategory::Movement: return "Movement";
        case AudioCategory::UI: return "UI";
        case AudioCategory::Weapons: return "Weapons";
        case AudioCategory::NPC: return "NPC";
        case AudioCategory::Impacts: return "Impacts";
        case AudioCategory::Ambient: return "Ambient";
    }
    return "Impacts";
}

bool audioCategoryFromName(const std::string& name, AudioCategory& out)
{
    const std::string value = lowercase(name);
    if (value == "movement") { out = AudioCategory::Movement; return true; }
    if (value == "ui") { out = AudioCategory::UI; return true; }
    if (value == "weapons" || value == "weapon") { out = AudioCategory::Weapons; return true; }
    if (value == "npc") { out = AudioCategory::NPC; return true; }
    if (value == "impacts" || value == "impact") { out = AudioCategory::Impacts; return true; }
    if (value == "ambient") { out = AudioCategory::Ambient; return true; }
    return false;
}

float SpatialProfile::gainAt(float distance) const
{
    if (!spatial || points.empty())
        return 0.0f;
    if (distance <= points.front().distance)
        return points.front().gain;
    for (size_t i = 1; i < points.size(); ++i) {
        if (distance > points[i].distance)
            continue;
        const SpatialPoint& a = points[i - 1];
        const SpatialPoint& b = points[i];
        const float span = b.distance - a.distance;
        if (span <= 0.0001f)
            return b.gain;
        const float t = (distance - a.distance) / span;
        return a.gain + (b.gain - a.gain) * t;
    }
    return points.back().gain;
}

const SpatialProfile& SpatialAudioData::profileFor(AudioCategory category) const
{
    auto it = categories.find((int)category);
    if (it != categories.end())
        return it->second;
    it = categories.find((int)AudioCategory::Impacts);
    if (it != categories.end())
        return it->second;
    static const SpatialProfile fallback = makeProfile(
        "Fallback", true, 1.0f, 60.0f,
        {{1.0f, 1.0f}, {60.0f, 0.0f}});
    return fallback;
}

const LegacyProfile& SpatialAudioData::legacyProfileFor(
    AudioCategory category) const
{
    auto it = legacyCategories.find((int)category);
    if (it != legacyCategories.end())
        return it->second;
    it = legacyCategories.find((int)AudioCategory::Impacts);
    if (it != legacyCategories.end())
        return it->second;
    static const LegacyProfile fallback = [] {
        LegacyProfile profile;
        profile.name = "Fallback";
        profile.fullVolumeDistance = 50.0f;
        profile.maxDistance = 100.0f;
        profile.volumeScale = 1.0f;
        return profile;
    }();
    return fallback;
}

LegacyProfile SpatialAudioData::legacyProfileForSound(
    const std::string& sound, AudioCategory category) const
{
    LegacyProfile profile = legacyProfileFor(category);
    auto it = legacySounds.find(lowercase(sound));
    if (it != legacySounds.end())
        profile = it->second;
    return profile;
}

AudioCategory SpatialAudioData::resolveCategory(const std::string& sound,
                                                AudioCategory fallback) const
{
    if (sound.empty())
        return fallback;
    const std::string key = lowercase(sound);
    auto exact = exactSounds.find(key);
    if (exact != exactSounds.end())
        return exact->second;
    for (const auto& prefix : prefixSounds) {
        if (key.rfind(prefix.first, 0) == 0)
            return prefix.second;
    }
    return fallback;
}

const char* spatialAudioModeName(SpatialAudioMode mode)
{
    return mode == SpatialAudioMode::Legacy ? "legacy" : "spatial";
}

SpatialAudioConfig::SpatialAudioConfig()
{
    mData = makeDefaultData();
}

SpatialAudioConfig& SpatialAudioConfig::instance()
{
    static SpatialAudioConfig config;
    return config;
}

bool SpatialAudioConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(mPath, ec);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Audio,
            "[AUDIO SPATIAL CONFIG] Watching: %s\n", mPath.c_str());
        mWatchLogged = true;
    }

    std::ifstream file(mPath, std::ios::binary);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        mLastHash.clear();
        Debug::warn(Debug::Category::Audio,
            "[AUDIO SPATIAL CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    const std::string contentHash = std::to_string(contents.size()) + ":" +
        std::to_string(std::hash<std::string>{}(contents));

    try {
        json root;
        root = json::parse(contents, nullptr, true, true);
        SpatialAudioData next = makeDefaultData();
        next.enabled = readBool(root, "enabled", next.enabled);

        if (root.contains("mode")) {
            bool parsed = false;
            if (root["mode"].is_string()) {
                const std::string mode = lowercase(root["mode"].get<std::string>());
                if (mode == "legacy" || mode == "1") {
                    next.mode = SpatialAudioMode::Legacy;
                    parsed = true;
                } else if (mode == "spatial" || mode == "2") {
                    next.mode = SpatialAudioMode::Spatial;
                    parsed = true;
                }
            } else if (root["mode"].is_number_integer()) {
                const int mode = root["mode"].get<int>();
                if (mode == 1 || mode == 2) {
                    next.mode = mode == 1 ? SpatialAudioMode::Legacy
                                          : SpatialAudioMode::Spatial;
                    parsed = true;
                }
            }
            if (!parsed) {
                Debug::warn(Debug::Category::Audio,
                    "[AUDIO SPATIAL CONFIG] Invalid mode; using legacy.\n");
                next.mode = SpatialAudioMode::Legacy;
            }
        }

        if (root.contains("legacy") && root["legacy"].is_object()) {
            const json& legacy = root["legacy"];
            next.legacyFullVolumeDistance = readFloat(
                legacy, "fullVolumeDistance",
                next.legacyFullVolumeDistance);
            next.legacyFullVolumeDistance = std::max(
                0.0f, next.legacyFullVolumeDistance);
            if (legacy.contains("categories") &&
                legacy["categories"].is_object()) {
                for (auto it = legacy["categories"].begin();
                     it != legacy["categories"].end(); ++it) {
                    AudioCategory category;
                    if (!audioCategoryFromName(it.key(), category))
                        continue;
                    const LegacyProfile& fallback =
                        next.legacyProfileFor(category);
                    next.legacyCategories[(int)category] = parseLegacyProfile(
                        it.value(), fallback,
                        next.legacyFullVolumeDistance);
                    next.legacyCategories[(int)category].name = it.key();
                }
            }
            if (legacy.contains("sounds") &&
                legacy["sounds"].is_object()) {
                next.legacySounds.clear();
                for (auto it = legacy["sounds"].begin();
                     it != legacy["sounds"].end(); ++it) {
                    if (!it.value().is_object())
                        continue;
                    const std::string key = lowercase(it.key());
                    AudioCategory category =
                        next.resolveCategory(key, AudioCategory::Impacts);
                    next.legacySounds[key] = parseLegacyProfile(
                        it.value(), next.legacyProfileFor(category),
                        next.legacyFullVolumeDistance);
                    next.legacySounds[key].name = key;
                }
            }
        }

        if (root.contains("categories") && root["categories"].is_object()) {
            for (auto it = root["categories"].begin();
                 it != root["categories"].end(); ++it) {
                AudioCategory category;
                if (!audioCategoryFromName(it.key(), category))
                    continue;
                const SpatialProfile& fallback =
                    next.profileFor(category);
                next.categories[(int)category] =
                    parseProfile(it.value(), fallback);
                next.categories[(int)category].name = it.key();
            }
        }

        if (root.contains("sounds") && root["sounds"].is_object()) {
            next.exactSounds.clear();
            for (auto it = root["sounds"].begin();
                 it != root["sounds"].end(); ++it) {
                AudioCategory category;
                if (it.value().is_string() &&
                    audioCategoryFromName(it.value().get<std::string>(), category)) {
                    next.exactSounds[lowercase(it.key())] = category;
                }
            }
        }

        if (root.contains("prefixes") && root["prefixes"].is_object()) {
            next.prefixSounds.clear();
            for (auto it = root["prefixes"].begin();
                 it != root["prefixes"].end(); ++it) {
                AudioCategory category;
                if (it.value().is_string() &&
                    audioCategoryFromName(it.value().get<std::string>(), category)) {
                    next.prefixSounds.emplace_back(lowercase(it.key()), category);
                }
            }
            std::sort(next.prefixSounds.begin(), next.prefixSounds.end(),
                [](const auto& a, const auto& b) {
                    return a.first.size() > b.first.size();
                });
        }

        mData = std::move(next);
        mLastWrite = writeTime;
        mLastHash = contentHash;
        Debug::warn(Debug::Category::Audio,
            "[AUDIO SPATIAL CONFIG] Loaded: %s mode=%s categories=%zu legacy=%zu sounds=%zu prefixes=%zu\n",
            mPath.c_str(), spatialAudioModeName(mData.mode),
            mData.categories.size(), mData.legacyCategories.size(),
            mData.exactSounds.size(), mData.prefixSounds.size());
        return true;
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        mLastHash = contentHash;
        Debug::error(Debug::Category::Audio,
            "[AUDIO SPATIAL CONFIG] Invalid %s: %s; keeping last valid settings.\n",
            mPath.c_str(), e.what());
        return false;
    }
}

bool SpatialAudioConfig::pollReload()
{
    std::ifstream file(mPath, std::ios::binary);
    if (!file.is_open())
        return false;
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    const std::string contentHash = std::to_string(contents.size()) + ":" +
        std::to_string(std::hash<std::string>{}(contents));
    if (contentHash == mLastHash)
        return false;
    Debug::warn(Debug::Category::Audio,
        "[AUDIO SPATIAL CONFIG] Detected change: %s\n", mPath.c_str());
    return load(mPath);
}
