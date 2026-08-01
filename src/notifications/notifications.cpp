// 07 31 2026, 15 00
/* purpose
* Implements the in-game notification popup system.
* Renders a bottom-right stack (max 3) with tick-based durations so timing is
* identical at any framerate, plus per-notification close and action buttons.
* Persists behavior settings to config/notifications.json and restyles from the
* config/gui/notifications.json GUI layout.
* Does NOT build notifications for other subsystems or drive any audio.
* Does NOT mutate player settings, network state, or world data.
*/
#include "notifications/notifications.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <windows.h>
#include <GLFW/glfw3.h>

#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "game/build-stamp.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

NotificationSystem& NotificationSystem::instance()
{
    static NotificationSystem sys;
    return sys;
}

void NotificationSystem::loadConfig()
{
    std::ifstream file(mConfigPath);
    if (!file.is_open()) {
        saveConfig();
        return;
    }
    try {
        json j;
        file >> j;
        mMaxCount = j.value("max_count", 3);
        mDefaultDurationTicks = j.value("default_duration_ticks", 300u);
        mFadeInTicks = j.value("fade_in_ticks", 12u);
        mFadeOutTicks = j.value("fade_out_ticks", 30u);
        mOffsetRight = j.value("offset_right", 20.0f);
        mOffsetBottom = j.value("offset_bottom", 10.0f);
        mSpacing = j.value("spacing", 8.0f);
        mEnabled = j.value("enabled", true);
        float hours = j.value("temp_mute_hours", 0.0f);
        mTempMuteUntilTick = hours > 0.0f
            ? nowTick() + (uint64_t)(hours * 3600.0 * 60.0)
            : 0;
        std::error_code ec;
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        Debug::log(Debug::Category::Gui, "[NOTIFS CONFIG] loaded max=%d durationTicks=%llu enabled=%d\n",
                   mMaxCount, (unsigned long long)mDefaultDurationTicks, (int)mEnabled);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Gui, "[NOTIFS] config parse error: %s\n", e.what());
    }
}

void NotificationSystem::saveConfig()
{
    try {
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(mConfigPath).parent_path(), ec);
        json j;
        j["max_count"] = mMaxCount;
        j["default_duration_ticks"] = mDefaultDurationTicks;
        j["fade_in_ticks"] = mFadeInTicks;
        j["fade_out_ticks"] = mFadeOutTicks;
        j["offset_right"] = mOffsetRight;
        j["offset_bottom"] = mOffsetBottom;
        j["spacing"] = mSpacing;
        j["enabled"] = mEnabled;
        j["temp_mute_hours"] = (float)tempMuteRemainingTicks() / (3600.0f * 60.0f);
        std::ofstream file(mConfigPath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Gui, "[NOTIFS] config save error: %s\n", e.what());
    }
}

void NotificationSystem::pollReload()
{
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(mConfigPath, ec);
    if (ec) return;
    if (wt != mConfigLastWrite) {
        mConfigLastWrite = wt;
        loadConfig();
    }
}

void NotificationSystem::advanceTicks()
{
    mClock.tick();
}

void NotificationSystem::push(const std::string& title, const std::string& message,
                              uint64_t durationTicks, const Action& action)
{
    if (!mEnabled || tempMuted()) return;
    pruneExpired();

    Notification n;
    n.title = title;
    n.message = message;
    n.startTick = nowTick();
    n.durationTicks = durationTicks > 0 ? durationTicks : mDefaultDurationTicks;
    n.action = action;
    mNotifications.push_back(n);

    if ((int)mNotifications.size() > mMaxCount)
        mNotifications.erase(mNotifications.begin());
}

void NotificationSystem::pushBuildNotice()
{
    Action a;
    a.type = ActionType::OpenUrl;
    a.payload = "https://www.mimita.fun";
    a.label = "mimita.fun";
    push("mimita.exe", std::string("build ") + MIMITA_BUILD_TIME,
         mDefaultDurationTicks, a);
}

void NotificationSystem::setEnabled(bool on)
{
    mEnabled = on;
    saveConfig();
    if (!on) clear();
}

void NotificationSystem::setTempMuteHours(float hours)
{
    mTempMuteUntilTick = hours > 0.0f
        ? nowTick() + (uint64_t)(hours * 3600.0 * 60.0)
        : 0;
    saveConfig();
}

uint64_t NotificationSystem::tempMuteRemainingTicks() const
{
    return mTempMuteUntilTick > nowTick() ? mTempMuteUntilTick - nowTick() : 0;
}

void NotificationSystem::pruneExpired()
{
    mNotifications.erase(
        std::remove_if(mNotifications.begin(), mNotifications.end(),
            [&](const Notification& n) {
                return mClock.getElapsedTicks(n.startTick) >= n.durationTicks;
            }),
        mNotifications.end());
}

void NotificationSystem::render()
{
    if (!mEnabled) { clear(); return; }
    pruneExpired();
    if (tempMuted() || mNotifications.empty()) return;

    GLFWwindow* win = glfwGetCurrentContext();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/notifications.json");
    const GuiElement* panelEl = layout.get("notifPanel");
    const GuiElement* titleEl = layout.get("notifTitle");
    const GuiElement* textEl = layout.get("notifText");
    const GuiElement* closeEl = layout.get("notifClose");
    const GuiElement* actionEl = layout.get("notifAction");

    const float w = panelEl && panelEl->w > 0.0f ? panelEl->w : 360.0f;
    const float h = panelEl && panelEl->h > 0.0f ? panelEl->h : 76.0f;
    const float gap = mSpacing;
    const float x = 1920.0f - w - mOffsetRight;
    const float bottomY = 1080.0f - mOffsetBottom;

    // Only honor clicks when the mouse is unlocked (menus/chat open). During
    // gameplay the locked cursor sits at screen center, far from these boxes.
    const bool mouseUnlocked = win &&
        glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    std::vector<size_t> toRemove;
    const size_t n = mNotifications.size();
    for (size_t i = 0; i < n; ++i) {
        const Notification& notif = mNotifications[i];
        uint64_t elapsed = mClock.getElapsedTicks(notif.startTick);

        float alpha = 1.0f;
        float slide = 0.0f;
        if (elapsed < mFadeInTicks) {
            float t = mFadeInTicks > 0 ? (float)elapsed / (float)mFadeInTicks : 1.0f;
            alpha = t;
            slide = (1.0f - t) * 200.0f;
        } else if (elapsed >= notif.durationTicks - mFadeOutTicks) {
            float t = mFadeOutTicks > 0
                ? (float)(elapsed - (notif.durationTicks - mFadeOutTicks)) / (float)mFadeOutTicks
                : 1.0f;
            alpha = 1.0f - t;
        }
        if (alpha < 0.01f) continue;

        const float y = bottomY - (n - 1 - i) * (h + gap);
        const UIRect box = {x + slide, y, w, h};

        if (panelEl) {
            GuiElement tmp = *panelEl;
            if (tmp.backgroundColor.size() >= 4) tmp.backgroundColor[3] = alpha * 0.95f;
            drawGuiElement(win, tmp, nullptr, &box);
        }

        if (titleEl) {
            GuiElement tmp = *titleEl;
            tmp.text = notif.title;
            if (tmp.textColor.size() >= 4) tmp.textColor[3] = alpha;
            UIRect r = {box.x + 12.0f, box.y + 8.0f, w - 56.0f, 18.0f};
            drawGuiElement(win, tmp, nullptr, &r);
        }

        if (textEl) {
            GuiElement tmp = *textEl;
            tmp.text = notif.message;
            if (tmp.textColor.size() >= 4) tmp.textColor[3] = alpha;
            UIRect r = {box.x + 12.0f, box.y + 30.0f, w - 56.0f, 16.0f};
            drawGuiElement(win, tmp, nullptr, &r);
        }

        if (closeEl) {
            GuiElement tmp = *closeEl;
            UIRect r = {box.x + w - 32.0f, box.y + 6.0f, 26.0f, 26.0f};
            UIButtonState s = drawGuiElement(win, tmp, nullptr, &r);
            if (mouseUnlocked && s.clicked) toRemove.push_back(i);
        }

        if (actionEl && notif.action.type != ActionType::None) {
            GuiElement tmp = *actionEl;
            tmp.text = notif.action.label.empty() ? "OPEN" : notif.action.label;
            const float aw = tmp.w > 0.0f ? tmp.w : 96.0f;
            const float ah = tmp.h > 0.0f ? tmp.h : 26.0f;
            UIRect r = {box.x + w - aw - 12.0f, box.y + h - ah - 8.0f, aw, ah};
            UIButtonState s = drawGuiElement(win, tmp, nullptr, &r);
            if (mouseUnlocked && s.clicked) {
                if (notif.action.type == ActionType::OpenUrl && !notif.action.payload.empty())
                    ShellExecuteA(nullptr, "open", notif.action.payload.c_str(),
                                  nullptr, nullptr, SW_SHOWNORMAL);
                toRemove.push_back(i);
            }
        }
    }

    if (!toRemove.empty()) {
        for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it)
            mNotifications.erase(mNotifications.begin() + (long)*it);
    }
}
