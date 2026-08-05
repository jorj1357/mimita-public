// 08 03 2026, 17 20
/* purpose
* Implements the short-lived on-screen killfeed renderer.
* Draws killer and victim names with compact VIP appearance metadata.
* Keeps replay and live killfeed presentation through one manager.
* DOES NOT own damage resolution, packet parsing, or entitlement verification.
* DOES NOT persist VIP state or full style presets in replay files.
* DOES NOT render chat, nameplates, or menu account panels.
*/

#include "killfeed.h"

#include <algorithm>
#include <cstdio>

#include "gui/ui-system.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "terminal/terminal-state.h"
#include "vip/vip-name-render.h"

KillfeedManager& KillfeedManager::instance()
{
    static KillfeedManager mgr;
    return mgr;
}

void KillfeedManager::onKill(const std::string& killerName,
                              const std::string& victimName,
                              const std::string& weaponName,
                              bool fromReplay)
{
    onKillStyled(killerName, MimitaVip::freeAppearance(),
                 victimName, MimitaVip::freeAppearance(),
                 weaponName, fromReplay);
}

void KillfeedManager::onKillStyled(const std::string& killerName,
                                   const MimitaVip::VipAppearance& killerVipAppearance,
                                   const std::string& victimName,
                                   const MimitaVip::VipAppearance& victimVipAppearance,
                                   const std::string& weaponName,
                                   bool fromReplay)
{
    onKillStyled(killerName, killerVipAppearance, MimitaVip::VipStyleDetail{},
                 victimName, victimVipAppearance, MimitaVip::VipStyleDetail{},
                 weaponName, fromReplay);
}

void KillfeedManager::onKillStyled(const std::string& killerName,
                                   const MimitaVip::VipAppearance& killerVipAppearance,
                                   const MimitaVip::VipStyleDetail& killerVipStyleDetail,
                                   const std::string& victimName,
                                   const MimitaVip::VipAppearance& victimVipAppearance,
                                   const MimitaVip::VipStyleDetail& victimVipStyleDetail,
                                   const std::string& weaponName,
                                   bool fromReplay)
{
    KillfeedEntry entry;
    entry.killerName = killerName;
    entry.victimName = victimName;
    entry.weaponName = weaponName;
    entry.killerVipAppearance = killerVipAppearance;
    entry.victimVipAppearance = victimVipAppearance;
    entry.killerVipStyleDetail = killerVipStyleDetail;
    entry.victimVipStyleDetail = victimVipStyleDetail;
    entry.age = 0.0f;
    entry.opacity = STILL_OPACITY;
    mEntries.push_back(std::move(entry));

    if (mEntries.size() > MAX_ENTRIES)
        mEntries.erase(mEntries.begin());

    RPLXDEBUG("KILLFEED_EVENT killer=%s victim=%s weapon=%s source=%s\n",
              killerName.c_str(), victimName.c_str(), weaponName.c_str(),
              fromReplay ? "replay" : "live");
}

void KillfeedManager::update(float dt)
{
    for (auto it = mEntries.begin(); it != mEntries.end();) {
        it->age += dt;
        if (it->age >= TOTAL_DURATION) {
            it = mEntries.erase(it);
            continue;
        }
        if (it->age >= STILL_DURATION) {
            float fadeProgress = (it->age - STILL_DURATION) / FADE_DURATION;
            it->opacity = STILL_OPACITY * (1.0f - fadeProgress);
        } else {
            it->opacity = STILL_OPACITY;
        }
        ++it;
    }
}

void KillfeedManager::render()
{
    if (mEntries.empty())
        return;

    const bool isReplay = gpReplayPlayer && gpReplayPlayer->isPlaying();
    const uint32_t currentTick = isReplay
        ? gpReplayPlayer->currentTick()
        : (gActiveReplayRecorder ? gActiveReplayRecorder->currentTick() : 0);

    float y = ENTRY_Y_OFFSET;
    for (size_t i = 0; i < mEntries.size(); ++i) {
        const KillfeedEntry& entry = mEntries[i];
        const float opacity = std::clamp(entry.opacity, 0.0f, STILL_OPACITY);
        const bool rendered = opacity > 0.01f;

        if (rendered) {
            float screenW = uiScreenW();
            float textOpacity = std::max(opacity, 0.05f);
            VipNameDrawOptions nameOptions;
            nameOptions.scale = ENTRY_FONT_SCALE;
            nameOptions.alpha = textOpacity;
            nameOptions.phase = 0.0f;
            nameOptions.detail = &entry.killerVipStyleDetail;
            const char* killedText = " killed ";
            const char* withText = " with ";
            const float totalW =
                vipMeasureStyledName(entry.killerName, entry.killerVipAppearance, nameOptions) +
                uiMeasureText(killedText, ENTRY_FONT_SCALE) +
                vipMeasureStyledName(entry.victimName, entry.victimVipAppearance, nameOptions) +
                uiMeasureText(withText, ENTRY_FONT_SCALE) +
                uiMeasureText(entry.weaponName.c_str(), ENTRY_FONT_SCALE);
            float x = screenW - totalW - ENTRY_X_OFFSET;
            glm::vec4 color = {1.0f, 1.0f, 1.0f, textOpacity};
            vipDrawStyledName(entry.killerName, entry.killerVipAppearance, x, y, nameOptions);
            x += vipMeasureStyledName(entry.killerName, entry.killerVipAppearance, nameOptions);
            uiDrawText(killedText, x, y, ENTRY_FONT_SCALE, color);
            x += uiMeasureText(killedText, ENTRY_FONT_SCALE);
            nameOptions.detail = &entry.victimVipStyleDetail;
            vipDrawStyledName(entry.victimName, entry.victimVipAppearance, x, y, nameOptions);
            x += vipMeasureStyledName(entry.victimName, entry.victimVipAppearance, nameOptions);
            uiDrawText(withText, x, y, ENTRY_FONT_SCALE, color);
            x += uiMeasureText(withText, ENTRY_FONT_SCALE);
            uiDrawText(entry.weaponName.c_str(), x, y, ENTRY_FONT_SCALE, color);
            y += ENTRY_HEIGHT;
        }

        RPLXDEBUG("KILLFEVENT_DETAIL tick=%u frame=%u killer=%s victim=%s weapon=%s source=%s rendered=%s opacity=%.2f age=%.2f\n",
                  currentTick, currentTick,
                  entry.killerName.c_str(), entry.victimName.c_str(), entry.weaponName.c_str(),
                  isReplay ? "replay" : "live",
                  rendered ? "yes" : "no", opacity, entry.age);
    }
}

void KillfeedManager::clear()
{
    mEntries.clear();
}
