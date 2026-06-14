#include "settings-menu.h"
#include "../gui-layout.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include "camera.h"
#include "config/player-settings.h"
#include "audio/music-manager.h"
#include "video/video-settings.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char* kOnOff[] = { "OFF", "ON" };

static const char* kResolutionLabels[] = {
    "800x600",
    "1024x768",
    "1280x720",
    "1920x1080"
};

static const char* kGraphicsPresets[] = {
    "Minimal",
    "Low",
    "Medium", 
    "High",
    "Max"
};
static const int kNumGraphicsPresets = 5;

static void drawHeading(const char* text, float x, float y)
{
    uiDrawText(text, uiScaleX(x), uiScaleY(y), 0.48f, {0.65f, 0.85f, 1.0f, 1.0f});
}

static void drawLabel(const char* text, float x, float y)
{
    uiDrawText(text, uiScaleX(x), uiScaleY(y + 4.0f), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});
}

// Draw a dropdown option list. Uses design coordinates internally,
// converts to screen coordinates for rendering.
static int drawDropdown(GLFWwindow* win, const char* id,
                         const char** options, int numOptions, int currentIdx,
                         float x, float y, float w)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int result = -1;
    for (int i = 0; i < numOptions; ++i)
    {
        float rowH = 26.0f;
        float ry = y + i * (rowH + 2.0f);
        UIRect r = {x, ry, w, rowH};
        UIRect sr = cs.designToScreen(r);
        glm::vec4 bg = (i == currentIdx)
            ? glm::vec4{0.25f, 0.55f, 0.85f, 1.0f}
            : glm::vec4{0.12f, 0.15f, 0.2f, 1.0f};
        uiDrawRect(sr, bg, id);
        if (i == currentIdx)
            uiDrawRectOutline(sr, {0.4f, 0.8f, 1.0f, 1.0f}, id);
        uiDrawText(options[i], uiScaleX(x + 8.0f), uiScaleY(ry + 4.0f), 0.30f,
                   {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "", r, bg, options[i]).clicked)
            result = i;
    }
    return result;
}

// Draw a slider with its current value displayed to the right.
// Returns true if value changed.
static bool drawSliderWithValue(GLFWwindow* win, const char* id,
                                 float x, float y, float w,
                                 float* value, float minVal, float maxVal)
{
    bool changed = uiSlider(win, id, {x, y, w - 60.0f, 28.0f}, value, minVal, maxVal);
    char buf[32];
    if (maxVal <= 1.0f)
        snprintf(buf, sizeof(buf), "%.2f", *value);
    else
        snprintf(buf, sizeof(buf), "%.0f", *value);
    uiDrawText(buf, uiScaleX(x + w - 56.0f), uiScaleY(y + 4.0f), 0.30f,
               {0.9f, 0.95f, 1.0f, 1.0f});
    return changed;
}

SettingsMenuResult drawSettingsMenu(GLFWwindow* win)
{
    SettingsMenuResult r{};
    PlayerSettings& settings = GetPlayerSettings();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/settings-menu.json");

    float fbW = uiScreenW(), fbH = uiScreenH();
    uiDrawRect({0, 0, fbW, fbH}, {0.02f, 0.025f, 0.035f, 1.0f}, "settings-bg");

    // Title
    uiDrawText("SETTINGS", uiScaleX(840.0f), uiScaleY(40.0f), 0.65f, {0.9f, 0.95f, 1.0f, 1.0f});
    uiDrawRect({uiScaleX(120.0f), uiScaleY(90.0f), uiScaleX(1680.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.5f}, "settings-sep");

    // Column layout at design coordinates
    const float c1X = 120.0f;
    const float c2X = 640.0f;
    const float ctrlW = 480.0f;
    const float rowH = 28.0f;
    const float labelH = 24.0f;
    const float gap = 6.0f;
    const float sectionGap = 20.0f;

    // ===== LEFT COLUMN: AUDIO =====
    float y1 = 120.0f;
    drawHeading("AUDIO", c1X, y1); y1 += 36.0f;

    drawLabel("Master Volume", c1X, y1); y1 += labelH;
    if (drawSliderWithValue(win, "Master Volume",
        c1X, y1, ctrlW, &settings.masterVolume, 0.0f, 1.0f))
        SavePlayerSettings();
    y1 += rowH + gap;

    drawLabel("Music Volume", c1X, y1); y1 += labelH;
    if (drawSliderWithValue(win, "Music Volume",
        c1X, y1, ctrlW, &settings.musicVolume, 0.0f, 1.0f))
    {
        MusicManager::instance().setVolume(settings.musicVolume);
        SavePlayerSettings();
    }
    y1 += rowH + gap;

    drawLabel("Mute", c1X, y1); y1 += labelH;
    if (uiCheckbox(win, "MUTE",
        {c1X, y1, 72.0f, rowH + 4.0f}, &settings.musicMuted))
    {
        MusicManager::instance().setMuted(settings.musicMuted);
        SavePlayerSettings();
    }
    y1 += rowH + sectionGap;

    // ===== LEFT COLUMN: GAMEPLAY =====
    drawHeading("GAMEPLAY", c1X, y1); y1 += 36.0f;

    drawLabel("Field of View", c1X, y1); y1 += labelH;
    if (drawSliderWithValue(win, "FOV",
        c1X, y1, ctrlW, &settings.fov, 10.0f, 350.0f))
    {
        CAMERA_FOV = settings.fov;
        SavePlayerSettings();
    }
    y1 += rowH + gap;

    drawLabel("Sensitivity", c1X, y1); y1 += labelH;
    if (drawSliderWithValue(win, "Sensitivity",
        c1X, y1, ctrlW, &settings.sensitivity, 0.1f, 10.0f))
    {
        CAMERA_SENS = settings.sensitivity;
        SavePlayerSettings();
    }
    y1 += rowH + sectionGap;

    // ===== RIGHT COLUMN: VIDEO =====
    float y2 = 120.0f;
    drawHeading("VIDEO", c2X, y2); y2 += 36.0f;

    drawLabel("Fullscreen", c2X, y2); y2 += labelH;
    {
        int fsIdx = VideoSettings::instance().fullscreen() ? 1 : 0;
        int next = drawDropdown(win, "fullscreen",
            kOnOff, 2, fsIdx, c2X, y2, 120.0f);
        if (next >= 0 && next != fsIdx)
            VideoSettings::instance().setFullscreen(next == 1);
    }
    y2 += 2 * (rowH + 2.0f) + gap;

    drawLabel("Resolution", c2X, y2); y2 += labelH;
    {
        int idx = VideoSettings::instance().resolutionIndex() - 1;
        if (idx < 0 || idx >= VideoSettings::NUM_RESOLUTIONS) idx = 0;
        int next = drawDropdown(win, "resolution",
            kResolutionLabels, VideoSettings::NUM_RESOLUTIONS, idx, c2X, y2, 240.0f);
        if (next >= 0 && next != idx)
            VideoSettings::instance().setResolution(next + 1);
    }
    y2 += VideoSettings::NUM_RESOLUTIONS * (rowH + 2.0f) + gap;

    drawLabel("Graphics Preset", c2X, y2); y2 += labelH;
    {
        int idx = -1;
        for (int i = 0; i < kNumGraphicsPresets; ++i)
            if (settings.graphicsPreset == kGraphicsPresets[i]) { idx = i; break; }
        if (idx < 0) idx = 2;
        int next = drawDropdown(win, "gfxpreset",
            kGraphicsPresets, kNumGraphicsPresets, idx, c2X, y2, 240.0f);
        if (next >= 0 && next != idx)
        {
            settings.graphicsPreset = kGraphicsPresets[next];
            SavePlayerSettings();
        }
    }
    y2 += kNumGraphicsPresets * (rowH + 2.0f) + sectionGap;

    // ===== RIGHT COLUMN: DEBUG =====
    drawHeading("DEBUG", c2X, y2); y2 += 36.0f;

    drawLabel("Physics Debug", c2X, y2); y2 += labelH;
    {
        static bool physicsDebug = true;
        if (uiButton(win, physicsDebug ? "ON" : "OFF",
            {c2X, y2, 72.0f, rowH + 4.0f},
            physicsDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "physics_debug").clicked)
            physicsDebug = !physicsDebug;
    }
    y2 += rowH + gap;

    drawLabel("Render Debug", c2X, y2); y2 += labelH;
    {
        static bool renderDebug = true;
        if (uiButton(win, renderDebug ? "ON" : "OFF",
            {c2X, y2, 72.0f, rowH + 4.0f},
            renderDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "render_debug").clicked)
            renderDebug = !renderDebug;
    }
    y2 += rowH + sectionGap;

    // ===== BACK BUTTON (centered) =====
    {
        UIRect br = layout.getRectDesign("backButton", {840.0f, 980.0f, 240.0f, 48.0f});
        if (uiButton(win, "BACK", br, {0.7f, 0.2f, 0.2f, 1.0f}).clicked)
        {
            printf("[SETTINGS MENU] Back pressed\n");
            r.goBack = true;
        }
    }

    return r;
}
