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
#include "persistence/persistence-rewards.h"
#include "debug/debug-log.h"

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
        readColor4("stat_color", d.statColor);
        readColor4("death_color", d.deathColor);
        d.killText = j.at("kill_text").get<std::string>();
        d.deathText = j.at("death_text").get<std::string>();
        d.xpText = j.at("xp_text").get<std::string>();
        d.goldText = j.at("gold_text").get<std::string>();
        d.attemptedText = j.at("attempted_text").get<std::string>();
        d.confirmedText = j.at("confirmed_text").get<std::string>();
        d.selfName = j.at("self_name").get<std::string>();
        d.environmentName = j.at("environment_name").get<std::string>();
        d.stacking = j.value("stacking", d.stacking);
        if (d.stacking != "vertical" && d.stacking != "overlap") return false;
        d.lineSpacing = std::max(0.0f, j.value("line_spacing", d.lineSpacing));
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
        Debug::logThrottled(Debug::Category::Gui,"reward-config-invalid",1.0f,
            "[REWARDS HUD] reload rejected path=%s lastGoodRetained=1\n",path.c_str());
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

void RewardPopupSystem::pushProgression(uint8_t kind, const std::string& name, const std::string& confirmedAt)
{
    ActiveRewardPopup p;
    const auto& rewards = getDefaultRewards();
    p.kind = kind; p.name = name; p.confirmedAt = confirmedAt;
    p.xpAmount = kind == 0 ? rewards.playerKillXp : (kind == 1 ? rewards.npcKillXp : 0);
    p.goldAmount = kind == 0 ? rewards.playerKillGold : (kind == 1 ? rewards.npcKillGold : -1);
    // Backend UTC ISO timestamp, displayed in the specification's exact format.
    if (confirmedAt.size() >= 19)
        p.confirmedAt = confirmedAt.substr(5,2) + "-" + confirmedAt.substr(8,2) + "-" +
            confirmedAt.substr(0,4) + ", " + confirmedAt.substr(11,2) + "-" +
            confirmedAt.substr(14,2) + "-" + confirmedAt.substr(17,2);
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
            (cfg.stacking == "overlap" ? 0.0f : cfg.stackSpacing * i) + cfg.floatSpeedY * p.elapsedTicks);

        unsigned line = 0;
        auto draw = [&](std::string text, glm::vec4 color, int amount) {
            const std::string displayName = p.kind == 2 && p.name == "yourself" ? cfg.selfName :
                (p.kind == 2 && p.name == "environment" ? cfg.environmentName : p.name);
            for (const auto& binding : {std::pair<std::string,std::string>{"{name}",displayName},
                     {"{amount}",std::to_string(amount)}, {"{time}",p.confirmedAt}})
            {
                size_t pos = 0;
                while ((pos = text.find(binding.first,pos)) != std::string::npos)
                { text.replace(pos,binding.first.size(),binding.second); pos += binding.second.size(); }
            }
            color.a *= alpha;
            uiDrawText(text.c_str(), baseX, floatY + uiScaleY(
                cfg.stacking == "overlap" ? 0.0f : cfg.lineSpacing * line), cfg.fontScale, color);
            ++line;
        };
        if (p.kind == 0) draw(cfg.killText,cfg.statColor,1);
        if (p.kind == 2) draw(cfg.deathText,cfg.deathColor,1);
        if (p.kind == 3) draw(cfg.attemptedText,cfg.statColor,0);
        if (p.kind == 4) draw(cfg.confirmedText,cfg.statColor,0);

        if (p.xpAmount > 0)
        {
            draw(cfg.xpText, cfg.xpColor, p.xpAmount);
        }

        // Zero gold is a real NPC reward and must remain visible as +0 Gold.
        if (p.goldAmount >= 0)
        {
            draw(cfg.goldText, cfg.goldColor, p.goldAmount);
        }
    }
}
