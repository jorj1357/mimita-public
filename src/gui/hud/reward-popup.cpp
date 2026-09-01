// 09 01 2026, 00 00
/* purpose
* Implement floating +XP and +Gold reward popups on the HUD.
* Hot-reloadable HUD config from config/rewards-hud.json.
* Fades from start_alpha to fade_to_alpha over duration_ticks.
* Does NOT implement persistence or rewards calculation.
*/

#include "gui/hud/reward-popup.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "gui/ui-system.h"

using json = nlohmann::json;

static int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

// ── RewardPopupConfig ──────────────────────────────────────────────────

RewardPopupConfig& RewardPopupConfig::instance()
{
    static RewardPopupConfig config;
    return config;
}

bool RewardPopupConfig::load(const std::string& path)
{
    mPath = path;
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    try {
        json j;
        file >> j;
        RewardPopupConfigData d;

        auto readColor4 = [&](const char* key, glm::vec4& out) {
            if (j.contains(key) && j[key].is_array() && j[key].size() >= 4) {
                out.r = std::clamp(j[key][0].get<float>(), 0.0f, 1.0f);
                out.g = std::clamp(j[key][1].get<float>(), 0.0f, 1.0f);
                out.b = std::clamp(j[key][2].get<float>(), 0.0f, 1.0f);
                out.a = std::clamp(j[key][3].get<float>(), 0.0f, 1.0f);
            }
        };

        readColor4("xp_color", d.xpColor);
        readColor4("gold_color", d.goldColor);
        d.fontScale = std::max(0.05f, j.value("font_scale", d.fontScale));
        d.durationTicks = std::max(1, j.value("duration_ticks", d.durationTicks));
        d.startAlpha = std::clamp(j.value("start_alpha", d.startAlpha), 0.0f, 1.0f);
        d.fadeToAlpha = std::clamp(j.value("fade_to_alpha", d.fadeToAlpha), 0.0f, 1.0f);
        d.anchorX = j.value("anchor_x", d.anchorX);
        d.anchorY = j.value("anchor_y", d.anchorY);
        d.offsetX = j.value("offset_x", d.offsetX);
        d.offsetY = j.value("offset_y", d.offsetY);
        d.stackSpacing = std::max(0.0f, j.value("stack_spacing", d.stackSpacing));
        d.floatSpeedY = j.value("float_speed_y", d.floatSpeedY);

        mData = d;
        mLastModified = modifiedTime(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool RewardPopupConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    return current != 0 && current != mLastModified && load(mPath);
}

// ── RewardPopupSystem ──────────────────────────────────────────────────

RewardPopupSystem& RewardPopupSystem::instance()
{
    static RewardPopupSystem sys;
    return sys;
}

void RewardPopupSystem::pushReward(int xpAmount, int goldAmount)
{
    ActiveRewardPopup p;
    p.xpAmount = xpAmount;
    p.goldAmount = goldAmount;
    p.elapsedTicks = 0;
    mPopups.push_back(p);

    if ((int)mPopups.size() > 8)
        mPopups.erase(mPopups.begin());
}

void RewardPopupSystem::tick()
{
    const auto& cfg = RewardPopupConfig::instance().data();
    for (auto it = mPopups.begin(); it != mPopups.end();)
    {
        it->elapsedTicks++;
        if (it->elapsedTicks >= cfg.durationTicks)
            it = mPopups.erase(it);
        else
            ++it;
    }
}

void RewardPopupSystem::render() const
{
    if (mPopups.empty())
        return;

    const auto& cfg = RewardPopupConfig::instance().data();
    const float baseX = uiScaleX(cfg.anchorX + cfg.offsetX);
    const float baseY = uiScaleY(cfg.anchorY + cfg.offsetY);

    for (int i = 0; i < (int)mPopups.size(); ++i)
    {
        const auto& p = mPopups[i];
        const float progress = (float)p.elapsedTicks / (float)cfg.durationTicks;
        const float alpha = cfg.startAlpha + (cfg.fadeToAlpha - cfg.startAlpha) * progress;
        const float floatY = baseY + uiScaleY(
            cfg.stackSpacing * i + cfg.floatSpeedY * p.elapsedTicks);

        if (p.xpAmount > 0)
        {
            char xpBuf[32];
            snprintf(xpBuf, sizeof(xpBuf), "+%d XP", p.xpAmount);
            glm::vec4 xpCol = cfg.xpColor;
            xpCol.a = alpha;
            uiDrawText(xpBuf, baseX, floatY, cfg.fontScale, xpCol);
        }

        if (p.goldAmount > 0)
        {
            char goldBuf[32];
            snprintf(goldBuf, sizeof(goldBuf), "+%d Gold", p.goldAmount);
            glm::vec4 goldCol = cfg.goldColor;
            goldCol.a = alpha;
            uiDrawText(goldBuf, baseX,
                       floatY + uiScaleY(cfg.stackSpacing * 0.5f),
                       cfg.fontScale, goldCol);
        }
    }
}
