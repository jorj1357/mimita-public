// 09 01 2026, 00 00
/* purpose
* Loads the hot-reloadable killfeed presentation and mode configuration.
* Provides validated colors, verbs, and weapon display overrides.
* Keeps killfeed presentation data separate from combat and network authority.
* DOES NOT resolve damage, award rewards, or own chat history storage.
* DOES NOT decide whether a kill is authoritative.
* DOES NOT render HUD or chat output.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

struct KillfeedVerbConfig {
    std::string text;
    glm::vec4 color{0.65f, 0.65f, 0.65f, 1.0f};
};

struct KillfeedWeaponConfig {
    std::string displayName;
    std::string killVerb;
    glm::vec4 color{1.0f, 0.85f, 0.25f, 1.0f};
};

struct KillfeedConfigData {
    std::string mode = "hud";
    std::string defaultKillVerb = "killed";
    glm::vec4 killerColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 victimColor{1.0f, 0.35f, 0.35f, 1.0f};
    glm::vec4 verbColor{0.65f, 0.65f, 0.65f, 1.0f};
    glm::vec4 weaponColor{1.0f, 0.85f, 0.25f, 1.0f};
    glm::vec4 distanceColor{0.75f, 0.75f, 0.75f, 1.0f};
    bool showTick = true;
    std::string tickPrefix = "TICK";
    std::unordered_map<std::string, KillfeedVerbConfig> verbs;
    std::unordered_map<std::string, KillfeedWeaponConfig> weapons;
};

class KillfeedConfig {
public:
    static KillfeedConfig& instance();
    bool load(const std::string& path = "config/killfeed.json");
    bool pollReload();
    const KillfeedConfigData& data() const { return mData; }

private:
    KillfeedConfig() = default;
    KillfeedConfigData mData;
    std::string mPath;
    int64_t mLastModified = 0;
};
