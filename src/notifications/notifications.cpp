// 07 31 2026, 15 00
/* purpose
* Implements the in-game notification popup system.
* Rendering is fully driven by JSON: panel position/anchor/offsets/spacing/
* slide/alpha/hover-brighten from config/notifications.json and element
* geometry/colors/fonts from config/gui/notifications.json, both hot-reloaded.
* Renders a stack (max 3) with tick-based durations so timing is identical at
* any framerate, plus per-notification close and action buttons, hover
* brightening, and auto-shrinking text so long messages stay on the panel.
* Schedules periodic gameplay tips from config/tips.json at random tick
* intervals from config/tipsconfig.json, each carrying a "NEW TIP" action that
* immediately swaps in another random tip. Persists behavior settings to
* config/notifications.json. Keeps an in-memory log of the last 100 popups.
* Does NOT build notifications for other subsystems or drive any audio.
* Does NOT mutate player settings, network state, or world data.
*/
#include "notifications/notifications.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <windows.h>
#include <GLFW/glfw3.h>

#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/gui-coord.h"
#include "gui/ui-system-internal.h"
#include "utils/tips.h"
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
        mAnchor = j.value("anchor", std::string("bottom_right"));
        mOffsetRight = j.value("offset_right", 20.0f);
        mOffsetBottom = j.value("offset_bottom", 10.0f);
        mOffsetTop = j.value("offset_top", 10.0f);
        mOffsetLeft = j.value("offset_left", 20.0f);
        mSpacing = j.value("spacing", 8.0f);
        mSlideInPx = j.value("slide_in_px", 200.0f);
        mPanelAlpha = j.value("panel_alpha", 0.95f);
        mHoverBrighten = j.value("hover_brighten", 0.15f);
        mEnabled = j.value("enabled", true);
        mShowInGame = j.value("show_in_game", true);
        float hours = j.value("temp_mute_hours", 0.0f);
        mTempMuteUntilTick = hours > 0.0f
            ? nowTick() + (uint64_t)(hours * 3600.0 * 60.0)
            : 0;
        std::error_code ec;
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        Debug::log(Debug::Category::Gui, "[NOTIFS CONFIG] loaded max=%d durationTicks=%llu enabled=%d showInGame=%d\n",
                   mMaxCount, (unsigned long long)mDefaultDurationTicks, (int)mEnabled, (int)mShowInGame);
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
        j["anchor"] = mAnchor;
        j["offset_right"] = mOffsetRight;
        j["offset_bottom"] = mOffsetBottom;
        j["offset_top"] = mOffsetTop;
        j["offset_left"] = mOffsetLeft;
        j["spacing"] = mSpacing;
        j["slide_in_px"] = mSlideInPx;
        j["panel_alpha"] = mPanelAlpha;
        j["hover_brighten"] = mHoverBrighten;
        j["enabled"] = mEnabled;
        j["show_in_game"] = mShowInGame;
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

void NotificationSystem::loadTipsConfig()
{
    std::ifstream file(mTipsConfigPath);
    if (!file.is_open()) {
        saveTipsConfig();
        return;
    }
    try {
        json j;
        file >> j;
        mTipsEnabled = j.value("enabled", true);
        mTipMinTicks = j.value("minimum_ticks_between_tips", 60u);
        mTipMaxTicks = j.value("maximum_ticks_between_tips", 1800u);
        if (mTipMinTicks < 1) mTipMinTicks = 1;
        if (mTipMaxTicks < mTipMinTicks) mTipMaxTicks = mTipMinTicks;
        std::error_code ec;
        mTipsConfigLastWrite = std::filesystem::last_write_time(mTipsConfigPath, ec);
        Debug::log(Debug::Category::Gui, "[TIPS CONFIG] loaded enabled=%d min=%llu max=%llu\n",
                   (int)mTipsEnabled, (unsigned long long)mTipMinTicks,
                   (unsigned long long)mTipMaxTicks);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Gui, "[TIPS CONFIG] parse error: %s\n", e.what());
    }
}

void NotificationSystem::saveTipsConfig()
{
    try {
        json j;
        j["enabled"] = mTipsEnabled;
        j["minimum_ticks_between_tips"] = mTipMinTicks;
        j["maximum_ticks_between_tips"] = mTipMaxTicks;
        std::ofstream file(mTipsConfigPath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
        std::error_code ec;
        mTipsConfigLastWrite = std::filesystem::last_write_time(mTipsConfigPath, ec);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Gui, "[TIPS CONFIG] save error: %s\n", e.what());
    }
}

void NotificationSystem::pollReload()
{
    {
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(mConfigPath, ec);
        if (!ec && wt != mConfigLastWrite) {
            mConfigLastWrite = wt;
            loadConfig();
        }
    }
    {
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(mTipsConfigPath, ec);
        if (!ec && wt != mTipsConfigLastWrite) {
            mTipsConfigLastWrite = wt;
            loadTipsConfig();
        }
    }
}

void NotificationSystem::advanceTicks()
{
    mClock.tick();
}

void NotificationSystem::updateTips()
{
    if (!mTipsEnabled || Tips::count() <= 0) return;
    if (mNextTipTick == 0) {
        scheduleNextTip();
        return;
    }
    if (nowTick() >= mNextTipTick) {
        pushTip();
        scheduleNextTip();
    }
}

void NotificationSystem::scheduleNextTip()
{
    static bool seeded = false;
    if (!seeded) {
        std::srand((unsigned)std::time(nullptr));
        seeded = true;
    }
    uint64_t range = mTipMaxTicks - mTipMinTicks;
    uint64_t extra = range > 0 ? (uint64_t)(std::rand() % (int)(range + 1)) : 0;
    mNextTipTick = nowTick() + mTipMinTicks + extra;
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
    recordHistory(n);

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

void NotificationSystem::pushTip()
{
    if (!mTipsEnabled || Tips::count() <= 0) return;
    std::string tip = Tips::getRandomTip();
    if (tip.empty()) return;

    char title[32];
    snprintf(title, sizeof(title), "TIP #%d", Tips::lastIndex() + 1);

    Action a;
    a.type = ActionType::Callback;
    a.label = "NEW TIP";
    a.callback = [this] { pushTip(); };

    push(title, tip, mDefaultDurationTicks, a);
}

void NotificationSystem::recordHistory(const Notification& n)
{
    HistoryEntry e;
    e.title = n.title;
    e.message = n.message;
    e.tick = n.startTick;
    mHistory.push_back(e);
    if (mHistory.size() > kMaxHistory)
        mHistory.erase(mHistory.begin(), mHistory.begin() + (long)(mHistory.size() - kMaxHistory));
}

void NotificationSystem::setEnabled(bool on)
{
    mEnabled = on;
    saveConfig();
    if (!on) clear();
}

void NotificationSystem::setShowInGame(bool on)
{
    mShowInGame = on;
    saveConfig();
}

void NotificationSystem::setTipsEnabled(bool on)
{
    mTipsEnabled = on;
    saveTipsConfig();
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

void NotificationSystem::render(bool inGameplay)
{
    if (!mEnabled) { clear(); return; }
    if (inGameplay && !mShowInGame) return;
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
    const bool anchoredBottom = mAnchor == "bottom_right" || mAnchor == "bottom_left";
    const bool anchoredLeft = mAnchor == "bottom_left" || mAnchor == "top_left";
    const float x = anchoredLeft ? mOffsetLeft : (1920.0f - w - mOffsetRight);
    const float stackEdge = anchoredBottom ? (1080.0f - mOffsetBottom) : mOffsetTop;
    const float originX = panelEl ? panelEl->x : x;
    const float originY = panelEl ? panelEl->y
        : (anchoredBottom ? (stackEdge - h) : stackEdge);

    // Only honor clicks when the mouse is unlocked (menus/chat open). During
    // gameplay the locked cursor sits at screen center, far from these boxes.
    const bool mouseUnlocked = win &&
        glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    // Cursor position in screen space for hover detection.
    double mx = 0.0, my = 0.0;
    double cursorX = 0.0, cursorY = 0.0;
    if (win && mouseUnlocked) {
        glfwGetCursorPos(win, &mx, &my);
        GuiCoordinateSystem::instance().cursorWindowToScreen(mx, my, cursorX, cursorY);
    }

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
            slide = (1.0f - t) * mSlideInPx;
        } else if (elapsed >= notif.durationTicks - mFadeOutTicks) {
            float t = mFadeOutTicks > 0
                ? (float)(elapsed - (notif.durationTicks - mFadeOutTicks)) / (float)mFadeOutTicks
                : 1.0f;
            alpha = 1.0f - t;
        }
        if (alpha < 0.01f) continue;

        // Stack from the anchor edge: bottom anchors grow upward (newest at the
        // bottom), top anchors grow downward (newest at the top). The panel's
        // bottom/top edge sits exactly at the anchor offset.
        float y;
        if (anchoredBottom)
            y = stackEdge - h - (n - 1 - i) * (h + gap);
        else
            y = stackEdge + (n - 1 - i) * (h + gap);
        const UIRect box = {x + slide, y, w, h};

        // Child element rects are derived from the GUI layout config, relative
        // to the panel's configured origin. Fallbacks only apply if an element
        // is missing from the layout file.
        auto childRect = [&](const GuiElement* el, float fbX, float fbY, float fbW, float fbH) {
            if (el && el->w > 0.0f && el->h > 0.0f)
                return UIRect{box.x + (el->x - originX), box.y + (el->y - originY), el->w, el->h};
            return UIRect{box.x + fbX, box.y + fbY, fbW, fbH};
        };

        const bool hovered = mouseUnlocked && pointIn(cursorX, cursorY,
            GuiCoordinateSystem::instance().designToScreen(box));

        if (panelEl) {
            GuiElement tmp = *panelEl;
            if (tmp.backgroundColor.size() >= 4) {
                tmp.backgroundColor[3] = alpha * mPanelAlpha;
                if (hovered) {
                    tmp.backgroundColor[0] = std::min(1.0f, tmp.backgroundColor[0] + mHoverBrighten);
                    tmp.backgroundColor[1] = std::min(1.0f, tmp.backgroundColor[1] + mHoverBrighten);
                    tmp.backgroundColor[2] = std::min(1.0f, tmp.backgroundColor[2] + mHoverBrighten);
                }
            }
            if (hovered && tmp.outlineColor.size() >= 4)
                tmp.outlineColor[3] = 1.0f;
            drawGuiElement(win, tmp, nullptr, &box);
        }

        if (titleEl) {
            GuiElement tmp = *titleEl;
            tmp.text = notif.title;
            if (tmp.textColor.size() >= 4) tmp.textColor[3] = alpha;
            UIRect r = childRect(titleEl, 12.0f, 8.0f, w - 56.0f, 18.0f);
            drawGuiElement(win, tmp, nullptr, &r);
        }

        if (textEl) {
            GuiElement tmp = *textEl;
            tmp.text = notif.message;
            if (tmp.textColor.size() >= 4) tmp.textColor[3] = alpha;
            UIRect r = childRect(textEl, 12.0f, 30.0f, w - 56.0f, 16.0f);
            // Auto-shrink the font so long tip messages (with date stamps) fit.
            if (tmp.fontSize > 0.0f) {
                float availScreen = GuiCoordinateSystem::instance().designToScreenX(r.w);
                float measured = uiMeasureText(notif.message.c_str(), tmp.fontSize);
                if (measured > availScreen && measured > 0.0f)
                    tmp.fontSize = tmp.fontSize * (availScreen / measured);
            }
            drawGuiElement(win, tmp, nullptr, &r);
        }

        if (closeEl) {
            GuiElement tmp = *closeEl;
            UIRect r = childRect(closeEl, w - 32.0f, 6.0f, 26.0f, 26.0f);
            UIButtonState s = drawGuiElement(win, tmp, nullptr, &r);
            if (mouseUnlocked && s.clicked) toRemove.push_back(i);
        }

        if (actionEl && notif.action.type != ActionType::None) {
            GuiElement tmp = *actionEl;
            tmp.text = notif.action.label.empty() ? "OPEN" : notif.action.label;
            UIRect r = childRect(actionEl, w - 96.0f - 12.0f, h - 26.0f - 8.0f, 96.0f, 26.0f);
            UIButtonState s = drawGuiElement(win, tmp, nullptr, &r);
            if (mouseUnlocked && s.clicked) {
                if (notif.action.type == ActionType::OpenUrl && !notif.action.payload.empty())
                    ShellExecuteA(nullptr, "open", notif.action.payload.c_str(),
                                  nullptr, nullptr, SW_SHOWNORMAL);
                else if (notif.action.type == ActionType::Callback && notif.action.callback)
                    notif.action.callback();
                toRemove.push_back(i);
            }
        }
    }

    if (!toRemove.empty()) {
        for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it)
            mNotifications.erase(mNotifications.begin() + (long)*it);
    }
}
