#include "killfeed.h"

#include <algorithm>
#include <cstdio>

#include "gui/ui-system.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "terminal/terminal-state.h"

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
    KillfeedEntry entry;
    entry.killerName = killerName;
    entry.victimName = victimName;
    entry.weaponName = weaponName;
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
            std::string text = entry.killerName + " killed " +
                               entry.victimName + " with " +
                               entry.weaponName;
            float textW = uiMeasureText(text.c_str(), ENTRY_FONT_SCALE);
            float x = screenW - textW - ENTRY_X_OFFSET;
            float textOpacity = std::max(opacity, 0.05f);
            glm::vec4 color = {1.0f, 1.0f, 1.0f, textOpacity};
            uiDrawText(text.c_str(), x, y, ENTRY_FONT_SCALE, color);
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
