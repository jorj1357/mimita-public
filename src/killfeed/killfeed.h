#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct ReplayKillfeedEvent;

struct KillfeedEntry {
    std::string killerName;
    std::string victimName;
    std::string weaponName;
    float age = 0.0f;
    float opacity = 0.0f;
};

class KillfeedManager {
public:
    static KillfeedManager& instance();

    void onKill(const std::string& killerName,
                const std::string& victimName,
                const std::string& weaponName,
                bool fromReplay = false);

    void update(float dt);
    void render();

    void clear();

private:
    static constexpr int MAX_ENTRIES = 5;
    static constexpr float STILL_OPACITY = 0.50f;
    static constexpr float STILL_DURATION = 3.0f;
    static constexpr float FADE_DURATION = 1.0f;
    static constexpr float TOTAL_DURATION = STILL_DURATION + FADE_DURATION;
    static constexpr float ENTRY_HEIGHT = 20.0f;
    static constexpr float ENTRY_X_OFFSET = 10.0f;
    static constexpr float ENTRY_Y_OFFSET = 60.0f;
    static constexpr float ENTRY_FONT_SCALE = 0.30f;

    std::vector<KillfeedEntry> mEntries;
};
