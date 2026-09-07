// 2026-09-07 16:20 EST
/* purpose
* load and hot-reload local player presentation preferences
* expose validated enemy, teammate, and self outline settings
* provide one effective local policy input to the player renderer
* does NOT own player simulation, collision, or network authority
* does NOT decide team membership
* does NOT render geometry
*/
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct PlayerOutlineSettings {
    bool enabled = false;
    float thickness = 1.0f;
    float alpha = 1.0f;
    glm::vec3 color{255.0f, 255.0f, 255.0f};
    bool visibleThroughWalls = false;
    bool disappearOnDeath = true;
    int renderOrder = 100;
};

struct PlayerVisualsData {
    std::vector<std::string> renderOrder{"outline"};
    PlayerOutlineSettings self;
    PlayerOutlineSettings enemy;
    PlayerOutlineSettings teammate;
};

class PlayerVisualsConfig {
public:
    static PlayerVisualsConfig& instance();
    bool load();
    bool reload();
    bool pollReload();
    const PlayerVisualsData& data() const { return mData; }
    const std::string& lastError() const { return mLastError; }
    std::string describe() const;

private:
    PlayerVisualsConfig();
    bool parseAndValidate(const std::string& text, PlayerVisualsData& out);
    std::filesystem::path mPath{"config/playervisuals.json"};
    std::filesystem::file_time_type mLastWrite{};
    PlayerVisualsData mData;
    std::string mLastError;
};
