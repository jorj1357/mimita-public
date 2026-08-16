// 08 16 2026, 01 35
/* purpose
* Implements the in-game notification popup system. Rendering is driven by JSON:
* config/gui/notifications.json controls GUI (anchor/offsets/spacing/slide plus
* dynamic-layout keys and the panel/title/text/close/action elements) and
* config/notifications.json controls behavior (max count, durations, fade,
* typewriter speed, enabled). Panels auto-size to content: the message word-wraps
* to the text box width, the box grows with the wrapped lines up to
* max_text_height, and overlong content scrolls (wheel + scrollbar). The message
* types in over revealTicks and the duration timer starts after typing finishes.
* All configs hot-reload; keeps an in-memory log of the last 100 popups.
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
// newlines. Greedy wrap: words that fit stay on the current line. A single
// unbreakable word wider than the box hard-breaks across lines so callers can
// keep a fixed font size without overflow or clipping.
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

        if (uiMeasureText(word.c_str(), fontScale) > maxWidthPx) {
            if (!line.empty()) {
                if (!out.empty()) out += '\n';
                out += line;
                line.clear();
            }
            std::string chunk;
            for (char c : word) {
                if (!chunk.empty() &&
                    uiMeasureText((chunk + c).c_str(), fontScale) > maxWidthPx) {
                    if (!out.empty()) out += '\n';
                    out += chunk;
                    chunk.clear();
                }
                chunk += c;
            }
            if (!chunk.empty()) line = chunk;
        } else if (!line.empty() &&
                   uiMeasureText((line + word).c_str(), fontScale) > maxWidthPx) {
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
        mTypewriterEnabled = j.value("typewriter_enabled", true);
        mCharsPerTick = j.value("chars_per_tick", 1u);
        if (mCharsPerTick < 1) mCharsPerTick = 1;
        mTypewriterDelayTicks = j.value("typewriter_delay_ticks", 4u);
        float hours = j.value("temp_mute_hours", 0.0f);
        mTempMuteUntilTick = hours > 0.0f
            ? nowTick() + (uint64_t)(hours * 3600.0 * 60.0)
            : 0;
        std::error_code ec;
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        Debug::log(Debug::Category::Gui, "[NOTIFS CONFIG] loaded max=%d durationTicks=%llu enabled=%d showInGame=%d typewriter=%d\n",
                   mMaxCount, (unsigned long long)mDefaultDurationTicks, (int)mEnabled, (int)mShowInGame,
                   (int)mTypewriterEnabled);
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
        j["typewriter_enabled"] = mTypewriterEnabled;
        j["chars_per_tick"] = mCharsPerTick;
        j["typewriter_delay_ticks"] = mTypewriterDelayTicks;
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
        mPaddingX = r.value("padding_x", 12.0f);
        mPaddingY = r.value("padding_y", 8.0f);
        mGapTitleText = r.value("gap_title_text", 6.0f);
        mGapTextAction = r.value("gap_text_action", 8.0f);
        mMaxTextHeight = r.value("max_text_height", 120.0f);
        std::error_code ec;
        mGuiConfigLastWrite = std::filesystem::last_write_time(mGuiConfigPath, ec);
        Debug::log(Debug::Category::Gui,
                   "[NOTIFS GUI] loaded anchor=%s offsets=(%g,%g,%g,%g) spacing=%g slide=%g maxTextH=%g\n",
                   mAnchor.c_str(), mOffsetRight, mOffsetBottom, mOffsetTop, mOffsetLeft,
                   mSpacing, mSlideInPx, mMaxTextHeight);
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

    // Typewriter: reveal the message over revealTicks and start the lifetime
    // timer AFTER typing finishes so the full text stays visible for the
    // requested duration.
    n.revealTicks = 0;
    if (mTypewriterEnabled && !n.message.empty()) {
        uint64_t typeTicks =
            (uint64_t)((n.message.size() + mCharsPerTick - 1) / mCharsPerTick) +
            mTypewriterDelayTicks;
        n.revealTicks = typeTicks;
        n.durationTicks += typeTicks;
    }

    mNotifications.push_back(n);
    recordHistory(n);

    if ((int)mNotifications.size() > mMaxCount)
        mNotifications.erase(mNotifications.begin());
}

void NotificationSystem::pushCritical(const std::string& title,
                                      const std::string& message,
                                      uint64_t durationTicks)
{
    if (!mEnabled || tempMuted()) return;
    pruneExpired();

    Notification n;
    n.title = title;
    n.message = message;
    n.startTick = nowTick();
    n.durationTicks = durationTicks > 0 ? durationTicks : mDefaultDurationTicks;
    n.critical = true;

    // Critical notices type out faster than tips so the state lands quickly.
    n.revealTicks = 0;
    if (mTypewriterEnabled && !n.message.empty()) {
        uint64_t typeTicks =
            (uint64_t)((n.message.size() + mCharsPerTick - 1) / mCharsPerTick) +
            mTypewriterDelayTicks;
        n.revealTicks = typeTicks;
        n.durationTicks += typeTicks;
    }

    mNotifications.push_back(n);
    recordHistory(n);

    if ((int)mNotifications.size() > mMaxCount)
        mNotifications.erase(mNotifications.begin());
}

void NotificationSystem::pushImportant(const std::string& title,
                                       const std::string& message,
                                       uint64_t durationTicks)
{
    if (!mEnabled || tempMuted()) return;
    pruneExpired();
    Notification n;
    n.title = title;
    n.message = message;
    n.startTick = nowTick();
    n.durationTicks = durationTicks > 0 ? durationTicks : mDefaultDurationTicks;
    n.important = true;
    if (mTypewriterEnabled && !n.message.empty()) {
        n.revealTicks = (uint64_t)((n.message.size() + mCharsPerTick - 1) / mCharsPerTick) +
                        mTypewriterDelayTicks;
        n.durationTicks += n.revealTicks;
    }
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

NotificationSystem::NotificationLayout
NotificationSystem::computeLayout(size_t index, uint64_t elapsed)
{
    const Notification& notif = mNotifications[index];
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mGuiConfigPath);
    const GuiElement* panelEl = layout.get("notifPanel");
    const GuiElement* titleEl = layout.get("notifTitle");
    const GuiElement* textEl = layout.get("notifText");
    const GuiElement* actionEl = layout.get("notifAction");

    NotificationLayout L;

    if (elapsed < mFadeInTicks) {
        float t = mFadeInTicks > 0 ? (float)elapsed / (float)mFadeInTicks : 1.0f;
        L.alpha = t;
        L.slide = (1.0f - t) * mSlideInPx;
    } else if (elapsed >= notif.durationTicks - mFadeOutTicks) {
        float t = mFadeOutTicks > 0
            ? (float)(elapsed - (notif.durationTicks - mFadeOutTicks)) / (float)mFadeOutTicks
            : 1.0f;
        L.alpha = 1.0f - t;
    }

    // Typewriter reveal: the message is progressively exposed over revealTicks.
    // The reveal window is linear so the whole message is shown exactly when
    // revealTicks elapses.
    const size_t len = notif.message.size();
    size_t revealCount = len;
    if (notif.revealTicks > 0) {
        uint64_t prog = std::min<uint64_t>(elapsed, notif.revealTicks);
        revealCount = (size_t)(len * (double)prog / (double)notif.revealTicks);
        if (revealCount > len) revealCount = len;
    }
    const std::string revealed = notif.message.substr(0, revealCount);

    // Message box width comes from notifText.width (X); fallback near-full.
    const float w = panelEl && panelEl->w > 0.0f ? panelEl->w : 360.0f;
    L.padX = mPaddingX;
    L.boxW = textEl && textEl->w > 0.0f ? textEl->w : (w - 56.0f);
    const float availWPx = cs.designToScreenX(L.boxW - L.padX * 2.0f);

    // Always render at the configured font size; over-wide words are broken
    // across lines by wrapTipText so nothing shrinks or clips.
    L.fontSize = textEl && textEl->fontSize > 0.0f ? textEl->fontSize : 0.34f;
    L.lines = splitLines(wrapTipText(revealed, availWPx, L.fontSize));
    L.lineHPx = (float)fontLineHeight * L.fontSize;
    L.contentHPx = (float)L.lines.size() * L.lineHPx;

    // The message area grows with the wrapped content, capped, then scrolls.
    L.textH = std::min(L.contentHPx / cs.scaleY(), mMaxTextHeight);
    L.scrollable = L.contentHPx > cs.designToScreenY(L.textH) + 0.5f;
    L.hasAction = actionEl && notif.action.type != ActionType::None;

    const float padY = mPaddingY;
    const float titleH = titleEl && titleEl->h > 0.0f ? titleEl->h : 18.0f;
    const float actionH = actionEl && actionEl->h > 0.0f ? actionEl->h : 22.0f;
    L.textY = padY + titleH + mGapTitleText;
    L.panelH = L.textY + L.textH + padY;
    if (L.hasAction) L.panelH += mGapTextAction + actionH;
    return L;
}

void NotificationSystem::drawMessage(size_t index, const UIRect& box,
                                     const NotificationLayout& L,
                                     bool mouseUnlocked, float cursorX, float cursorY)
{
    Notification& notif = mNotifications[index];
    const uint64_t elapsed = mClock.getElapsedTicks(notif.startTick);
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const GuiElement* textEl = GuiLayoutManager::instance()
        .getLayout(mGuiConfigPath).get("notifText");
    if (!textEl) return;

    // Scissored box, wheel scroll, and auto-follow the newest text while the
    // typewriter is still revealing.
    const float drawX = box.x + L.padX;
    const float drawY = box.y + L.textY;
    const float drawW = (textEl->w > 0.0f ? textEl->w : L.boxW) - L.padX * 2.0f;
    const float sx = cs.designToScreenX(drawX);
    const float sy = cs.designToScreenY(drawY);
    const float clipW = cs.designToScreenX(drawW);
    const float clipH = cs.designToScreenY(L.textH);

    float maxScroll = std::max(0.0f, L.contentHPx - clipH);
    UIScrollState& sc = notif.scroll;

    // Wheel scroll while hovering the message area.
    if (mouseUnlocked && maxScroll > 0.0f &&
        pointIn(cursorX, cursorY, UIRect{sx, sy, clipW, clipH})) {
        double delta = UISys::gScrollYOffset;
        UISys::gScrollYOffset = 0.0;
        sc.scrollY = std::clamp(sc.scrollY - (float)(delta * 40.0), 0.0f, maxScroll);
    }

    // Auto-follow the newest text unless the user scrolled up.
    const bool typing = notif.revealTicks > 0 && elapsed < notif.revealTicks;
    const bool atBottom = sc.scrollY >= maxScroll - 1.0f;
    if ((typing || elapsed >= notif.revealTicks) && atBottom)
        sc.scrollY = maxScroll;
    sc.scrollY = std::clamp(sc.scrollY, 0.0f, maxScroll);

    // Vertical alignment only applies when the content fits the box.
    float vShift = 0.0f;
    if (!L.scrollable) {
        if (textEl->verticalAlign == "middle")
            vShift = (clipH - L.contentHPx) * 0.5f;
        else if (textEl->verticalAlign == "bottom")
            vShift = clipH - L.contentHPx;
    }

    // Scissor the message to its box so scrolled text stays clipped.
    GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLint scissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)sx, (int)((float)UISys::gFbH - sy - clipH),
              std::max(0, (int)clipW), std::max(0, (int)clipH));

    glm::vec4 color = textEl->getTextColorVec();
    color.a *= L.alpha;
    if (notif.critical)
    {
        color.r = 1.0f;
        color.g = 0.30f;
        color.b = 0.25f;
    }
    else if (notif.important)
    {
        color.r = 1.0f;
        color.g = 0.86f;
        color.b = 0.15f;
    }
    float yPx = sy + vShift - sc.scrollY;
    for (const std::string& ln : L.lines) {
        float lx = sx;
        if (textEl->textAlign == "center")
            lx += (clipW - uiMeasureText(ln.c_str(), L.fontSize)) * 0.5f;
        else if (textEl->textAlign == "right")
            lx += clipW - uiMeasureText(ln.c_str(), L.fontSize);
        uiDrawText(ln.c_str(), lx, yPx, L.fontSize, color);
        yPx += L.lineHPx;
    }

    // Restore scissor.
    if (!scissorWas)
        glDisable(GL_SCISSOR_TEST);
    else
        glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);

    // Scrollbar, drawn only when the content overflows the box.
    if (maxScroll > 0.0f) {
        const float sbW = 6.0f;
        const float sbX = sx + clipW - sbW - 2.0f;
        const float thumbH = std::min(clipH, std::max(24.0f, clipH * (clipH / L.contentHPx)));
        const float track = clipH - thumbH;
        const float thumbY = sy + (track > 0.0f ? (sc.scrollY / maxScroll) * track : 0.0f);
        uiDrawRect({sbX, sy, sbW, clipH}, {0.10f, 0.12f, 0.16f, 0.35f}, "notif-scroll-track");
        uiDrawRect({sbX, thumbY, sbW, thumbH}, {0.3f, 0.5f, 0.7f, 0.6f}, "notif-scroll-thumb");
    }
}

void NotificationSystem::render(bool inGameplay)
{
    if (!mEnabled) { clear(); return; }
    if (inGameplay && !mShowInGame) return;
    pruneExpired();
    if (tempMuted() || mNotifications.empty()) return;

    GLFWwindow* win = glfwGetCurrentContext();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout(mGuiConfigPath);
    const GuiElement* panelEl = layout.get("notifPanel");
    const GuiElement* titleEl = layout.get("notifTitle");
    const GuiElement* closeEl = layout.get("notifClose");
    const GuiElement* actionEl = layout.get("notifAction");

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    const float w = panelEl && panelEl->w > 0.0f ? panelEl->w : 360.0f;
    const float gap = mSpacing;
    const bool anchoredBottom = mAnchor == "bottom_right" || mAnchor == "bottom_left";
    const bool anchoredLeft = mAnchor == "bottom_left" || mAnchor == "top_left";
    const float x = anchoredLeft ? mOffsetLeft : (1920.0f - w - mOffsetRight);
    const float stackEdge = anchoredBottom ? (1080.0f - mOffsetBottom) : mOffsetTop;

    // Only honor clicks when the mouse is unlocked (menus/chat open). During
    // gameplay the locked cursor sits at screen center, far from these boxes.
    const bool mouseUnlocked = win &&
        glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    // Cursor position in screen space for hover detection.
    double mx = 0.0, my = 0.0;
    double cursorX = 0.0, cursorY = 0.0;
    if (win && mouseUnlocked) {
        glfwGetCursorPos(win, &mx, &my);
        cs.cursorWindowToScreen(mx, my, cursorX, cursorY);
    }

    // Title/action/close geometry derived from the layout elements.
    const float padX = mPaddingX;
    const float padY = mPaddingY;
    const float titleH = titleEl && titleEl->h > 0.0f ? titleEl->h : 18.0f;
    const float closeW = closeEl && closeEl->w > 0.0f ? closeEl->w : 26.0f;
    const float closeH = closeEl && closeEl->h > 0.0f ? closeEl->h : 26.0f;
    const float actionW = actionEl && actionEl->w > 0.0f ? actionEl->w : 100.0f;
    const float actionH = actionEl && actionEl->h > 0.0f ? actionEl->h : 22.0f;

    // ── Pass 1: layout each notification. Panel height grows with the wrapped
    // message lines (capped by max_text_height) plus title and optional action
    // row, so each panel sizes itself to its content. ──
    const size_t n = mNotifications.size();
    std::vector<NotificationLayout> info(n);
    for (size_t i = 0; i < n; ++i) {
        const uint64_t elapsed = mClock.getElapsedTicks(mNotifications[i].startTick);
        info[i] = computeLayout(i, elapsed);
    }

    // ── Stack panels from the anchor edge using each panel's own height. ──
    std::vector<float> boxY(n);
    {
        float yCursor = stackEdge;
        for (size_t k = n; k-- > 0;) { // newest (i = n-1) sits nearest the anchor edge
            if (anchoredBottom) {
                boxY[k] = yCursor - info[k].panelH;
                yCursor = boxY[k] - gap;
            } else {
                boxY[k] = yCursor;
                yCursor = boxY[k] + info[k].panelH + gap;
            }
        }
    }

    std::vector<size_t> toRemove;
    for (size_t i = 0; i < n; ++i) {
        Notification& notif = mNotifications[i];
        NotificationLayout& L = info[i];
        if (L.alpha < 0.01f) continue;

        const UIRect box = {x + L.slide, boxY[i], w, L.panelH};

        const bool hovered = mouseUnlocked && pointIn(cursorX, cursorY,
            cs.designToScreen(box));

        if (panelEl) {
            GuiElement tmp = *panelEl;
            if (tmp.backgroundColor.size() >= 4) {
                tmp.backgroundColor[3] = L.alpha * mPanelAlpha;
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
            if (tmp.textColor.size() >= 4) {
                tmp.textColor[3] = L.alpha;
                if (notif.critical) {
                    tmp.textColor[0] = 1.0f;
                    tmp.textColor[1] = 0.30f;
                    tmp.textColor[2] = 0.25f;
                } else if (notif.important) {
                    tmp.textColor[0] = 1.0f;
                    tmp.textColor[1] = 0.86f;
                    tmp.textColor[2] = 0.15f;
                }
            }
            UIRect r = {box.x + padX, box.y + padY,
                        w - padX * 2.0f - closeW - 6.0f, titleH};
            drawGuiElement(win, tmp, nullptr, &r);
        }

        // Message area: scissored box, wheel scroll, and auto-follow while the
        // typewriter reveals (see drawMessage).
        drawMessage(i, box, L, mouseUnlocked, cursorX, cursorY);

        if (closeEl) {
            GuiElement tmp = *closeEl;
            UIRect r = {box.x + w - padX - closeW, box.y + padY, closeW, closeH};
            UIButtonState s = drawGuiElement(win, tmp, nullptr, &r);
            if (mouseUnlocked && s.clicked) toRemove.push_back(i);
        }

        if (actionEl && notif.action.type != ActionType::None) {
            GuiElement tmp = *actionEl;
            tmp.text = notif.action.label.empty() ? "OPEN" : notif.action.label;
            UIRect r = {box.x + w - padX - actionW,
                        box.y + L.panelH - padY - actionH, actionW, actionH};
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
