// 09 01 2026, 00 00
/* purpose
* Display floating +XP and +Gold reward popups on the HUD when kills are confirmed.
* Fades from start_alpha to fade_to_alpha over duration_ticks.
* Hot-reloadable HUD config (colors, position, timing) from config/rewards-hud.json.
* Does NOT implement persistence, rewards calculation, or database logic.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct RewardPopupConfigData
{
    glm::vec4 xpColor{0.0f, 0.85f, 0.85f, 1.0f};
    glm::vec4 goldColor{1.0f, 0.85f, 0.0f, 1.0f};
    glm::vec4 statColor{1.0f}, deathColor{1.0f};
    std::string killText, deathText, xpText, goldText, attemptedText, confirmedText;
    std::string selfName, environmentName;
    std::string stacking = "vertical";
    float lineSpacing = 22.0f;
    float fontScale = 0.38f;
    int durationTicks = 30;
    float startAlpha = 0.5f;
    float fadeToAlpha = 0.0f;
    float anchorX = 960.0f;
    float anchorY = 540.0f;
    float offsetX = -200.0f;
    float offsetY = -120.0f;
    float stackSpacing = 22.0f;
    float floatSpeedY = -0.4f;
};

class RewardPopupConfig
{
public:
    static RewardPopupConfig& instance();

    bool load(const std::string& path = "config/rewards-hud.json");
    bool pollReload();

    const RewardPopupConfigData& data() const { return mData; }

private:
    RewardPopupConfig() = default;

    RewardPopupConfigData mData;
    std::string mPath;
    int64_t mLastModified = 0;
};

struct ActiveRewardPopup
{
    int xpAmount = 0;
    int goldAmount = 0;
    int elapsedTicks = 0;
    uint8_t kind = 0;
    std::string name, confirmedAt;
};

class RewardPopupSystem
{
public:
    static RewardPopupSystem& instance();

    void pushProgression(uint8_t kind, const std::string& name, const std::string& confirmedAt);
    void tick();
    void render() const;

private:
    RewardPopupSystem() = default;

    std::vector<ActiveRewardPopup> mPopups;
};
