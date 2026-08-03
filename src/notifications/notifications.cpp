// 07 31 2026, 15 00
/* purpose
* Implements the in-game notification popup system.
* Rendering is fully driven by JSON: config/gui/notifications.json controls all
* GUI — the "render" object holds anchor/offsets/spacing/slide/panel-alpha/
* hover-brighten, and the elements define panel + title/text/close/action
* geometry, colors, and fonts. The tip text box uses notifText width (X) and
* height (Y): the message word-wraps to fit the width and the font auto-scales
* to fit the height. Behavior (max count, durations, fade, enabled, show in
* game) and tip scheduling come from config/notifications.json and
* config/tipsconfig.json. All configs hot-reload. Keeps an in-memory log of
* the last 100 popups and persists behavior settings to config/notifications.json.
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
#include "gui/font-stuff/font-loader.h"
#include "utils/tips.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

// Word-wrap `text` to fit `maxWidthPx` at `fontScale`, preserving explicit
// newlines. Greedy wrap: words that fit stay on the current line.
std::string wrapTipText(const std::string& text, float maxWidthPx, float fontScale)
{
    std::string out;
    std::string line;
    size_t i = 0;
    while (i < text.size()) {
        size_t start = i;
        while (i < text.size() && text[i] != ' ' && text[i] != '\n') ++i;
        std::string word = text.substr(start, i - start);
        if (i < text.size() && text[i] == ' ') { word += ' '; ++i; }
        const bool forcedBreak = i < text.size() && text[i] == '\n';
        if (forcedBreak) ++i;

        if (!line.empty() && uiMeasureText((line + word).c_str(), fontScale) > maxWidthPx) {
            if (!out.empty()) out += '\n';
            out += line;
            line = word;
        } else {
            line += word;
        }
        if (forcedBreak) {
            if (!out.empty()) out += '\n';
            out += line;
            line.clear();
        }
    }
    if (!line.empty()) {
        if (!out.empty()) out += '\n';
        out += line;
    }
    return out;
}

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t pos = 0, next;
    while ((next = text.find('\n', pos)) != std::string::npos) {
        lines.push_back(text.substr(pos, next - pos));
        pos = next + 1;
    }
    lines.push_back(text.substr(pos));
    return lines;
}

} // namespace

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
        mTipDurationTicks = j.value("tip_duration_ticks", 0u);
        mFadeInTicks = j.value("fade_in_ticks", 12u);
        mFadeOutTicks = j.value("fade_out_ticks", 30u);
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
        j["tip_duration_ticks"] = mTipDurationTicks;
        j["fade_in_ticks"] = mFadeInTicks;
        j["fade_out_ticks"] = mFadeOutTicks;
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

void NotificationSystem::loadGuiConfig()
{
    std::ifstream file(mGuiConfigPath);
    if (!file.is_open()) return;
    try {
        json j;
        file >> j;
        if (!j.contains("render")) return;
        const json& r = j["render"];
        mAnchor = r.value("anchor", std::string("bottom_right"));
        mOffsetRight = r.value("offset_right", 20.0f);
        mOffsetBottom = r.value("offset_bottom", 10.0f);
        mOffsetTop = r.value("offset_top", 10.0f);
        mOffsetLeft = r.value("offset_left", 20.0f);
        mSpacing = r.value("spacing", 8.0f);
        mSlideInPx = r.value("slide_in_px", 200.0f);
        mPanelAlpha = r.value("panel_alpha", 0.95f);
        mHoverBrighten = r.value("hover_brighten", 0.15f);
        std::error_code ec;
        mGuiConfigLastWrite = std::filesystem::last_write_time(mGuiConfigPath, ec);
        Debug::log(Debug::Category::Gui,
                   "[NOTIFS GUI] loaded anchor=%s offsets=(%g,%g,%g,%g) spacing=%g slide=%g\n",
                   mAnchor.c_str(), mOffsetRight, mOffsetBottom, mOffsetTop, mOffsetLeft,
                   mSpacing, mSlideInPx);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Gui, "[NOTIFS GUI] render config parse error: %s\n", e.what());
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
    {
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(mGuiConfigPath, ec);
        if (!ec && wt != mGuiConfigLastWrite) {
            mGuiConfigLastWrite = wt;
            loadGuiConfig();
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
    char exePath[MAX_PATH];
    std::string buildTime = "unknown";
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) != 0) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(exePath, GetFileExInfoStandard, &data)) {
            SYSTEMTIME st;
            if (FileTimeToSystemTime(&data.ftLastWriteTime, &st)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%02u-%02u-%04u %02u:%02u",
                         st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute);
                buildTime = buf;
            }
        }
    }

    Action a;
    a.type = ActionType::OpenUrl;
    a.payload = "https://www.mimita.fun";
    a.label = "mimita.fun";
    push("mimita.exe", std::string("build ") + buildTime,
         mDefaultDurationTicks, a);
}

void NotificationSystem::pushTip(bool force)
{
    if ((!force && !mTipsEnabled) || Tips::count() <= 0) return;
    std::string tip = Tips::getRandomTip();
    if (tip.empty()) return;

    char title[32];
    snprintf(title, sizeof(title), "TIP #%d", Tips::lastIndex() + 1);

    Action a;
    a.type = ActionType::Callback;
    a.label = "NEW TIP";
    a.callback = [this] { pushTip(); };

    const uint64_t duration = mTipDurationTicks > 0 ? mTipDurationTicks : mDefaultDurationTicks;
    push(title, tip, duration, a);
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
            // Tip text box: notifText.width (X) and notifText.height (Y) from
            // config/gui/notifications.json define the box. The message wraps to
            // fit the width and the font auto-scales down to fit the height.
            const float boxX = box.x + (textEl->x - originX);
            const float boxY = box.y + (textEl->y - originY);
            const float boxW = textEl->w > 0.0f ? textEl->w : (w - 56.0f);
            const float boxH = textEl->h > 0.0f ? textEl->h : 20.0f;
            const float padX = textEl->paddingX > 0.0f ? textEl->paddingX : 0.0f;
            const float padY = textEl->paddingY > 0.0f ? textEl->paddingY : 0.0f;
            float fontSize = textEl->fontSize > 0.0f ? textEl->fontSize : 0.26f;

            GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
            const float sx = cs.designToScreenX(boxX + padX);
            const float sy = cs.designToScreenY(boxY + padY);
            const float availWPx = cs.designToScreenX(boxW - padX * 2.0f);
            const float availHPx = cs.designToScreenY(boxH - padY * 2.0f);
            const float lineH = (float)fontLineHeight * fontSize;

            std::string wrapped = wrapTipText(notif.message, availWPx, fontSize);
            size_t lineCount = splitLines(wrapped).size();
            float needHPx = (float)lineCount * lineH;
            if (needHPx > availHPx && needHPx > 0.0f && availHPx > 0.0f) {
                fontSize = fontSize * (availHPx / needHPx);
                if (fontSize < 0.12f) fontSize = 0.12f;
                wrapped = wrapTipText(notif.message, availWPx, fontSize);
                lineCount = splitLines(wrapped).size();
                needHPx = (float)lineCount * ((float)fontLineHeight * fontSize);
            }

            glm::vec4 color = textEl->getTextColorVec();
            color.a *= alpha;
            float y = sy;
            if (textEl->verticalAlign == "middle")
                y += (availHPx - needHPx) * 0.5f;
            else if (textEl->verticalAlign == "bottom")
                y += availHPx - needHPx;
            for (const std::string& ln : splitLines(wrapped)) {
                float lx = sx;
                if (textEl->textAlign == "center")
                    lx += (availWPx - uiMeasureText(ln.c_str(), fontSize)) * 0.5f;
                else if (textEl->textAlign == "right")
                    lx += availWPx - uiMeasureText(ln.c_str(), fontSize);
                uiDrawText(ln.c_str(), lx, y, fontSize, color);
                y += (float)fontLineHeight * fontSize;
            }
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
