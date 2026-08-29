#include "settings-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include "../ui-tooltip.h"
#include "analytics/analytics-manager.h"
#include "camera.h"
#include "config/player-settings.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "audio/music-manager.h"
#include "video/video-settings.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* kOnOff[] = { "OFF", "ON" };

static const char* kResolutionLabels[] = {
    "800x600", "1024x768", "1280x720", "1920x1080"
};

static const char* kGraphicsPresets[] = {
    "Minimal", "Low", "Medium", "High", "Max"
};
static const int kNumGraphicsPresets = 5;

enum class SettingsTab { Video, Graphics, Audio, Game, Extra };
static SettingsTab gCurrentTab = SettingsTab::Video;

static UIScrollState gScrollVideo;
static UIScrollState gScrollGraphics;
static UIScrollState gScrollAudio;
static UIScrollState gScrollGame;
static UIScrollState gScrollExtra;

static const char* tabIdForIndex(int i)
{
    const char* ids[] = {"tabVideo", "tabGraphics", "tabAudio", "tabGame", "tabExtra"};
    return (i >= 0 && i < 5) ? ids[i] : ids[0];
}

static const float CONTENT_Y = 160.0f;
static const float CONTENT_H = 820.0f;

static UIScrollState& scrollForTab(SettingsTab t)
{
    switch (t) {
        case SettingsTab::Video:    return gScrollVideo;
        case SettingsTab::Graphics: return gScrollGraphics;
        case SettingsTab::Audio:    return gScrollAudio;
        case SettingsTab::Game:     return gScrollGame;
        case SettingsTab::Extra:    return gScrollExtra;
    }
    return gScrollVideo;
}

static void drawSectionTitle(const char* text, float y)
{
    uiDrawText(text, uiScaleX(60.0f), uiScaleY(y), 0.44f, {0.65f, 0.85f, 1.0f, 1.0f});
}

static void drawSliderWithValue(GLFWwindow* win, const char* label, float y, float* value, float minVal, float maxVal, bool& changed)
{
    float labelW = 300.0f;
    float sliderW = 380.0f;
    float sliderH = 28.0f;
    float valueX = 60.0f + labelW + sliderW + 16.0f;

    uiDrawText(label, uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});
    if (uiSlider(win, label, {60.0f + labelW, y + sliderH * 0.5f - 14.0f, sliderW, sliderH}, value, minVal, maxVal))
        changed = true;

    char buf[32];
    if (maxVal <= 1.0f)
        snprintf(buf, sizeof(buf), "%.2f", *value);
    else
        snprintf(buf, sizeof(buf), "%.0f", *value);
    uiDrawText(buf, uiScaleX(valueX), uiScaleY(y + 2.0f), 0.30f, {0.9f, 0.95f, 1.0f, 1.0f});
}

static void drawCheckbox(GLFWwindow* win, const char* label, float y, bool* value, bool& changed)
{
    uiDrawText(label, uiScaleX(60.0f), uiScaleY(y + 4.0f), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});
    if (uiCheckbox(win, label, {60.0f + 300.0f, y, 72.0f, 32.0f}, value))
        changed = true;
}

static void drawDropdownSimple(GLFWwindow* win, const char* label, float y, const char** options, int numOptions, int& currentIdx, bool& changed)
{
    float ddX = 60.0f + 300.0f;
    float ddW = 240.0f;
    float rowH = 24.0f;
    float ddH = numOptions * rowH;

    uiDrawText(label, uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});

    for (int i = 0; i < numOptions; ++i)
    {
        float ry = y + 26.0f + i * rowH;
        UIRect rowRect = {ddX, ry, ddW, rowH - 2.0f};
        glm::vec4 bg = (i == currentIdx)
            ? glm::vec4{0.25f, 0.55f, 0.85f, 1.0f}
            : glm::vec4{0.12f, 0.15f, 0.2f, 1.0f};
        uiDrawRect(rowRect, bg, "dropdown-row");
        uiDrawText(options[i], uiScaleX(ddX + 8.0f), uiScaleY(ry + 2.0f), 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "", rowRect, bg, options[i]).clicked && i != currentIdx)
        {
            currentIdx = i;
            changed = true;
        }
    }
}

static void drawVideoTab(GLFWwindow* win)
{
    UIScrollState& scroll = gScrollVideo;
    float colH = 900.0f;
    UIRect area = {40.0f, CONTENT_Y, 1840.0f, CONTENT_H};
    uiBeginScrollArea(win, area, colH, scroll);

    float y = CONTENT_Y;
    bool changed = false;
    drawSectionTitle("VIDEO", y); y += 50.0f;

    // Resolution
    {
        int idx = VideoSettings::instance().resolutionIndex() - 1;
        if (idx < 0 || idx >= VideoSettings::NUM_RESOLUTIONS) idx = 0;
        drawDropdownSimple(win, "Resolution", y, kResolutionLabels, VideoSettings::NUM_RESOLUTIONS, idx, changed);
        if (changed) VideoSettings::instance().setResolution(idx + 1);
        y += 120.0f;
    }

    // Fullscreen
    {
        int fsIdx = VideoSettings::instance().fullscreen() ? 1 : 0;
        drawDropdownSimple(win, "Fullscreen", y, kOnOff, 2, fsIdx, changed);
        if (changed) VideoSettings::instance().setFullscreen(fsIdx == 1);
        y += 120.0f;
    }

    // VSync — forced OFF, no toggle
    {
        uiDrawText("VSync", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});
        uiDrawText("OFF (forced)", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.5f, 0.5f, 0.55f, 1.0f});
        y += 120.0f;
    }

    // Frame Rate Limit
    {
        float fps = (float)VideoSettings::instance().maxFrames();
        drawSliderWithValue(win, "Frame Rate Limit", y, &fps, 30.0f, 999.0f, changed);
        if (changed) VideoSettings::instance().setMaxFrames((int)fps);
        y += 60.0f;
    }

    y += 40.0f;
    uiEndScrollArea(area, colH, scroll);
}

static void drawGraphicsTab(GLFWwindow* win)
{
    UIScrollState& scroll = gScrollGraphics;
    float colH = 600.0f;
    UIRect area = {40.0f, CONTENT_Y, 1840.0f, CONTENT_H};
    uiBeginScrollArea(win, area, colH, scroll);

    PlayerSettings& settings = GetPlayerSettings();
    float y = CONTENT_Y;
    bool changed = false;

    drawSectionTitle("GRAPHICS", y); y += 50.0f;

    {
        int idx = -1;
        for (int i = 0; i < kNumGraphicsPresets; ++i)
            if (settings.graphicsPreset == kGraphicsPresets[i]) { idx = i; break; }
        if (idx < 0) idx = 2;
        drawDropdownSimple(win, "Graphics Preset", y, kGraphicsPresets, kNumGraphicsPresets, idx, changed);
        if (changed)
        {
            settings.graphicsPreset = kGraphicsPresets[idx];
            SavePlayerSettings();
        }
        y += 120.0f;
    }

    // Future graphics settings (placeholder labels)
    uiDrawText("Shadows", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});
    y += 40.0f;
    uiDrawText("Post Processing", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});
    y += 40.0f;
    uiDrawText("Anti-Aliasing", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});
    y += 40.0f;
    uiDrawText("View Distance", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});

    y += 40.0f;
    uiEndScrollArea(area, colH, scroll);
}

static void drawAudioTab(GLFWwindow* win)
{
    UIScrollState& scroll = gScrollAudio;
    float colH = 600.0f;
    UIRect area = {40.0f, CONTENT_Y, 1840.0f, CONTENT_H};
    uiBeginScrollArea(win, area, colH, scroll);

    PlayerSettings& settings = GetPlayerSettings();
    float y = CONTENT_Y;
    bool changed = false;

    drawSectionTitle("AUDIO", y); y += 50.0f;

    drawSliderWithValue(win, "Master Volume", y, &settings.masterVolume, 0.0f, 1.0f, changed);
    if (changed) SavePlayerSettings();
    y += 60.0f;

    drawSliderWithValue(win, "Music Volume", y, &settings.musicVolume, 0.0f, 1.0f, changed);
    if (changed)
    {
        MusicManager::instance().setVolume(settings.musicVolume);
        SavePlayerSettings();
    }
    y += 60.0f;

    drawSliderWithValue(win, "SFX Volume", y, &settings.sfxVolume, 0.0f, 1.0f, changed);
    if (changed) SavePlayerSettings();
    y += 60.0f;

    drawCheckbox(win, "Mute", y, &settings.musicMuted, changed);
    if (changed)
    {
        MusicManager::instance().setMuted(settings.musicMuted);
        SavePlayerSettings();
    }
    y += 50.0f;

    // Future audio settings placeholders
    y += 20.0f;
    uiDrawText("UI Volume", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});
    y += 40.0f;
    uiDrawText("Voice Chat Volume", uiScaleX(60.0f), uiScaleY(y + 2.0f), 0.32f, {0.5f, 0.55f, 0.65f, 1.0f});
    uiDrawText("Coming soon", uiScaleX(60.0f + 300.0f), uiScaleY(y + 2.0f), 0.28f, {0.4f, 0.45f, 0.55f, 1.0f});

    y += 40.0f;
    uiEndScrollArea(area, colH, scroll);
}

static void drawGameTab(GLFWwindow* win)
{
    UIScrollState& scroll = gScrollGame;
    float colH = 1400.0f;
    UIRect area = {40.0f, CONTENT_Y, 1840.0f, CONTENT_H};
    uiBeginScrollArea(win, area, colH, scroll);

    PlayerSettings& settings = GetPlayerSettings();
    float y = CONTENT_Y;
    bool changed = false;

    drawSectionTitle("GAMEPLAY", y); y += 50.0f;

    drawSliderWithValue(win, "Field of View", y, &settings.fov, 10.0f, 350.0f, changed);
    if (changed) { SavePlayerSettings(); }
    y += 60.0f;

    drawSliderWithValue(win, "Sensitivity", y, &settings.sensitivity, 0.1f, 10.0f, changed);
    if (changed) { CAMERA_SENS = settings.sensitivity; SavePlayerSettings(); }
    y += 60.0f;

    drawCheckbox(win, "Blood Effects", y, &settings.bloodFX, changed);
    if (changed)
    {
        gBloodFXEnabled = settings.bloodFX;
        SavePlayerSettings();
        Debug::log(Debug::Category::NpcCombat, "[BLOODFX] %s",
            gBloodFXEnabled ? "Enabled" : "Disabled");
    }
    y += 50.0f;

    y += 30.0f;
    drawSectionTitle("CROSSHAIR", y); y += 50.0f;

    auto& config = CrosshairConfig::instance();
    auto& crosshair = config.edit();

    // Crosshair preview
    UIRect previewRect = {uiScaleX(60.0f + 300.0f + 380.0f + 30.0f),
                          uiScaleY(y), uiScaleX(300.0f), uiScaleY(220.0f)};
    uiDrawRect(previewRect, {0.08f, 0.09f, 0.12f, 1.0f}, "crosshair-preview");
    drawCrosshairPreview(previewRect.x + previewRect.w * 0.5f,
                         previewRect.y + previewRect.h * 0.5f,
                         std::min(uiScaleX(2.0f), uiScaleY(2.0f)));

    drawSliderWithValue(win, "Size", y, &crosshair.size, 0.0f, 32.0f, changed);
    y += 60.0f;
    drawSliderWithValue(win, "Gap", y, &crosshair.gap, 0.0f, 32.0f, changed);
    y += 60.0f;
    drawSliderWithValue(win, "Thickness", y, &crosshair.thickness, 1.0f, 10.0f, changed);
    y += 60.0f;

    float red = (float)crosshair.red;
    float green = (float)crosshair.green;
    float blue = (float)crosshair.blue;
    drawSliderWithValue(win, "Red", y, &red, 0.0f, 255.0f, changed);
    y += 60.0f;
    drawSliderWithValue(win, "Green", y, &green, 0.0f, 255.0f, changed);
    y += 60.0f;
    drawSliderWithValue(win, "Blue", y, &blue, 0.0f, 255.0f, changed);
    y += 60.0f;
    crosshair.red = (int)red;
    crosshair.green = (int)green;
    crosshair.blue = (int)blue;

    drawCheckbox(win, "Dot", y, &crosshair.dot, changed); y += 40.0f;
    drawCheckbox(win, "Outline", y, &crosshair.outline, changed); y += 40.0f;
    drawCheckbox(win, "Dynamic", y, &crosshair.dynamic, changed); y += 40.0f;
    drawCheckbox(win, "Top", y, &crosshair.showTop, changed); y += 40.0f;
    drawCheckbox(win, "Bottom", y, &crosshair.showBottom, changed); y += 40.0f;
    drawCheckbox(win, "Left", y, &crosshair.showLeft, changed); y += 40.0f;
    drawCheckbox(win, "Right", y, &crosshair.showRight, changed); y += 40.0f;

    if (changed) config.save();

    y += 20.0f;
    uiEndScrollArea(area, colH, scroll);
}

static void drawExtraTab(GLFWwindow* win, SettingsMenuResult& r)
{
    UIScrollState& scroll = gScrollExtra;
    float colH = 1000.0f;
    UIRect area = {40.0f, CONTENT_Y, 1840.0f, CONTENT_H};
    uiBeginScrollArea(win, area, colH, scroll);

    float y = CONTENT_Y;

    drawSectionTitle("ANALYTICS", y); y += 50.0f;
    AnalyticsManager::instance().drawSettingsPanel(win);
    y += 200.0f;

    drawSectionTitle("DEBUG", y); y += 50.0f;

    {
        static bool physicsDebug = true;
        bool pbChanged = false;
        drawCheckbox(win, "Physics Debug", y, &physicsDebug, pbChanged);
        if (pbChanged) physicsDebug = !physicsDebug;
        y += 40.0f;
    }

    {
        static bool renderDebug = true;
        bool rdChanged = false;
        drawCheckbox(win, "Render Debug", y, &renderDebug, rdChanged);
        if (rdChanged) renderDebug = !renderDebug;
        y += 40.0f;
    }

    y += 30.0f;
    drawSectionTitle("ACCOUNT", y); y += 50.0f;

    // Sign Out button
    if (uiButton(win, "SIGN OUT", {60.0f, y, 220.0f, 48.0f},
                 {0.5f, 0.15f, 0.15f, 1.0f}, "settings-signout").clicked)
    {
        printf("[SETTINGS MENU] Sign Out pressed\n");
        r.signOut = true;
    }
    y += 80.0f;

    y += 40.0f;
    uiEndScrollArea(area, colH, scroll);
}

SettingsMenuResult drawSettingsMenu(GLFWwindow* win)
{
    SettingsMenuResult r{};
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/settings-menu.json");

    // Draw static background, title, and back button from layout
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        if (elem->type == "panel" || elem->type == "text" || elem->type == "label")
        {
            drawGuiElement(win, *elem);
            continue;
        }

        // Handle tab buttons
        if (id.find("tab") == 0 && id != "tabVideo")
        {
            // Tab buttons: highlight the active one
            int tabIdx = -1;
            for (int i = 0; i < 5; ++i) {
                if (id == tabIdForIndex(i)) { tabIdx = i; break; }
            }
            bool active = (tabIdx >= 0 && gCurrentTab == (SettingsTab)tabIdx);
            glm::vec4 bg = active
                ? glm::vec4{0.2f, 0.35f, 0.55f, 1.0f}
                : elem->getBackgroundColorVec();

            if (uiButton(win, elem->text.c_str(), {elem->x, elem->y, elem->w, elem->h},
                          bg, elem->id.c_str()).clicked && tabIdx >= 0)
            {
                gCurrentTab = (SettingsTab)tabIdx;
            }
            continue;
        }

        // First tab (tabVideo) handles click detection too
        if (id == "tabVideo")
        {
            bool active = (gCurrentTab == SettingsTab::Video);
            glm::vec4 bg = active
                ? glm::vec4{0.2f, 0.35f, 0.55f, 1.0f}
                : elem->getBackgroundColorVec();
            if (uiButton(win, elem->text.c_str(), {elem->x, elem->y, elem->w, elem->h},
                          bg, elem->id.c_str()).clicked)
            {
                gCurrentTab = SettingsTab::Video;
            }
            continue;
        }

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "backButton")
        {
            printf("[SETTINGS MENU] Back pressed\n");
            r.goBack = true;
        }
    }

    // Tab content (still hardcoded for now — uses sliders/checkboxes/dropdowns with runtime state)
    switch (gCurrentTab)
    {
        case SettingsTab::Video:    drawVideoTab(win); break;
        case SettingsTab::Graphics: drawGraphicsTab(win); break;
        case SettingsTab::Audio:    drawAudioTab(win); break;
        case SettingsTab::Game:     drawGameTab(win); break;
        case SettingsTab::Extra:    drawExtraTab(win, r); break;
    }

    return r;
}
