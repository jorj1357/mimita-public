#include "music-manager.h"

#include <cstdio>
#include <algorithm>

#include "gui/ui-system.h"
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"

void MusicManager::drawNowPlayingPopup()
{
    if (!mTrackJustChanged) return;

    float sw = uiScreenW();
    float sh = uiScreenH();

    float pw = 340.0f;
    float ph = 72.0f;
    float px = sw - pw - 20.0f + mPopupSlide;
    float py = sh - ph - 100.0f;

    UIRect popupRect = {px, py, pw, ph};
    GuiLayout& mLayout = GuiLayoutManager::instance().getLayout("config/gui/music-widget.json");

    // Popup panel with alpha
    const GuiElement* ppEl = mLayout.get("nowPlayingPopup");
    if (ppEl) {
        GuiElement tmp = *ppEl;
        if (tmp.backgroundColor.size() >= 4) tmp.backgroundColor[3] = mPopupAlpha * 0.92f;
        drawGuiElement(glfwGetCurrentContext(), tmp, nullptr, &popupRect);
    }

    UIRect hdrRect = {px + 14.0f, py + 8.0f, 200, 20};
    const GuiElement* hdrEl = mLayout.get("nowPlayingHeader");
    if (hdrEl) {
        GuiElement tmp = *hdrEl;
        tmp.textColor = {0.5f, 0.8f, 1.0f, mPopupAlpha};
        drawGuiElement(glfwGetCurrentContext(), tmp, nullptr, &hdrRect);
    }

    UIRect trkRect = {px + 14.0f, py + 36.0f, 300, 20};
    const GuiElement* trkEl = mLayout.get("nowPlayingTrack");
    if (trkEl) {
        GuiElement tmp = *trkEl;
        tmp.text = currentTrackInfo();
        tmp.textColor = {0.9f, 0.9f, 1.0f, mPopupAlpha};
        drawGuiElement(glfwGetCurrentContext(), tmp, nullptr, &trkRect);
    }
}

void MusicManager::drawMusicWidget()
{
    GLFWwindow* win = glfwGetCurrentContext();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& mLayout = GuiLayoutManager::instance().getLayout("config/gui/music-widget.json");

    // Get cursor in design coordinates
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    double fbx, fby;
    cs.cursorWindowToScreen(mx, my, fbx, fby);
    float cdx = cs.screenToDesignX((float)fbx);
    float cdy = cs.screenToDesignY((float)fby);

    // Widget layout in design coordinates (1920x1080)
    const float iconS = 36.0f;
    const float iconX = 1920.0f - iconS - 10.0f;
    const float iconY = 1080.0f - iconS - 10.0f;
    const UIRect iconRect = {iconX, iconY, iconS, iconS};

    const float panelW = 300.0f;
    const float panelH = 210.0f;
    const float panelX = iconX - (panelW - iconS);
    const float panelY = iconY - panelH - 6.0f;
    const UIRect panelRect = {panelX, panelY, panelW, panelH};

    // Hover detection
    bool hoverIcon = cdx >= iconRect.x && cdx <= iconRect.x + iconRect.w &&
                     cdy >= iconRect.y && cdy <= iconRect.y + iconRect.h;
    bool hoverPanel = cdx >= panelRect.x && cdx <= panelRect.x + panelRect.w &&
                      cdy >= panelRect.y && cdy <= panelRect.y + panelRect.h;
    bool hoverAny = hoverIcon || hoverPanel;

    if (hoverAny) {
        mWidgetCloseTimer = 2.0f;
        mWidgetPanelOpen = true;
    } else if (mWidgetPanelOpen) {
        mWidgetCloseTimer -= mWidgetDt;
        if (mWidgetCloseTimer <= 0.0f) {
            mWidgetPanelOpen = false;
            mWidgetCloseTimer = 0.0f;
        }
    }

    // Helpers: draw element with dynamic position
    auto dynEl = [&](const std::string& id, UIRect designRect) -> bool {
        const GuiElement* el = mLayout.get(id);
        return el && drawGuiElement(win, *el, nullptr, &designRect).clicked;
    };
    auto dynText = [&](const std::string& id, UIRect designRect, const std::string& text) {
        const GuiElement* el = mLayout.get(id);
        if (!el) return;
        GuiElement tmp = *el;
        tmp.text = text;
        drawGuiElement(win, tmp, nullptr, &designRect);
    };

    // Icon
    drawGuiElement(win, *mLayout.get("icon"), nullptr, &iconRect);
    uiDrawText("♪", uiScaleX(iconX + 7.0f), uiScaleY(iconY + 5.0f), 0.5f,
               {0.7f, 0.85f, 1.0f, 0.9f});

    if (!mWidgetPanelOpen) return;

    // Panel
    drawGuiElement(win, *mLayout.get("panel"), nullptr, &panelRect);

    float y = panelY + 10.0f;
    UIRect headerRect = {panelX + 12.0f, y, 200, 24};
    dynText("musicPlayerHeader", headerRect, "MUSIC PLAYER");
    y += 26.0f;

    UIRect trackRect = {panelX + 12.0f, y, 280, 20};
    dynText("trackInfo", trackRect, currentTrackInfo());
    y += 26.0f;

    // Transport buttons
    const float btnW = 50.0f;
    const float btnH = 26.0f;
    const float gap = 8.0f;
    UIRect pauseRect = {panelX + 12.0f, y, btnW, btnH};
    if (dynEl("musicPause", pauseRect)) {
        if (isPlaying()) pause(); else resume();
    }
    UIRect skipRect = {panelX + 12.0f + (btnW + gap), y, btnW, btnH};
    if (dynEl("musicSkip", skipRect)) skip();
    UIRect prevRect = {panelX + 12.0f + (btnW + gap) * 2, y, btnW, btnH};
    if (dynEl("musicPrev", prevRect)) previous();
    y += btnH + 14.0f;

    // Volume slider
    if (uiSlider(win, "", {panelX + 12.0f, y, panelW - 24.0f, 20.0f}, &mVolume, 0.0f, 1.0f))
        applyVolume();
    y += 28.0f;

    // Mute toggle
    UIRect muteRect = {panelX + 12.0f, y, 80.0f, 24.0f};
    {
        const GuiElement* muteEl = mLayout.get("musicMute");
        if (muteEl) {
            GuiElement tmp = *muteEl;
            tmp.text = mMuted ? "MUTED" : "MUTE ON";
            if (drawGuiElement(win, tmp, nullptr, &muteRect).clicked)
                setMuted(!mMuted);
        }
    }
    y += 30.0f;

    // Speed controls
    {
        char spBuf[64];
        snprintf(spBuf, sizeof(spBuf), "Speed: %.2fx", mPlaybackSpeed);
        UIRect spLabelRect = {panelX + 12.0f, y, 200, 20};
        dynText("speedLabel", spLabelRect, spBuf);
        y += 22.0f;

        const float spBtnW = 60.0f;
        const float spBtnH = 22.0f;
        UIRect slowRect = {panelX + 12.0f, y, spBtnW, spBtnH};
        if (dynEl("musicSlow", slowRect))
            setPlaybackSpeed(mPlaybackSpeed - 0.1f);
        UIRect resetRect = {panelX + 12.0f + (spBtnW + 6.0f), y, spBtnW, spBtnH};
        if (dynEl("musicReset", resetRect))
            setPlaybackSpeed(1.0f);
        UIRect fastRect = {panelX + 12.0f + (spBtnW + 6.0f) * 2, y, spBtnW, spBtnH};
        if (dynEl("musicFast", fastRect))
            setPlaybackSpeed(mPlaybackSpeed + 0.1f);
    }
}

void MusicManager::drawAllOverlay()
{
    drawNowPlayingPopup();
    drawMusicWidget();
}
