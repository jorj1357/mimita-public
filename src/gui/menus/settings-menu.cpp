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
    "Minimal", "Low", "Medium", "High", "Max"
};
static const int kNumGraphicsPresets = 5;

static void drawHeading(const char* text, const GuiElement* elem)
{
    if (!elem) return;
    uiDrawText(text, uiScaleX(elem->x), uiScaleY(elem->y), 0.48f, {0.65f, 0.85f, 1.0f, 1.0f});
}

static void drawLabel(const char* text, const GuiElement* elem)
{
    if (!elem) return;
    uiDrawText(text, uiScaleX(elem->x), uiScaleY(elem->y), 0.32f, {0.8f, 0.85f, 0.95f, 1.0f});
}

static int drawDropdown(GLFWwindow* win, const GuiElement* elem,
                         const char** options, int numOptions, int currentIdx)
{
    if (!elem) return -1;
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    int result = -1;
    float rowH = elem->h / (float)numOptions;
    for (int i = 0; i < numOptions; ++i)
    {
        float ry = elem->y + i * rowH;
        UIRect rowRect = {elem->x, ry, elem->w, rowH - 2.0f};
        UIRect sr = cs.designToScreen(rowRect);
        glm::vec4 bg = (i == currentIdx)
            ? glm::vec4{0.25f, 0.55f, 0.85f, 1.0f}
            : glm::vec4{0.12f, 0.15f, 0.2f, 1.0f};
        uiDrawRect(sr, bg, "dropdown-row");
        if (i == currentIdx)
            uiDrawRectOutline(sr, {0.4f, 0.8f, 1.0f, 1.0f}, "dropdown-sel");
        uiDrawText(options[i],
            uiScaleX(elem->x + elem->textOffsetX),
            uiScaleY(ry + elem->textOffsetY),
            0.30f, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "", rowRect, bg, options[i]).clicked)
            result = i;
    }
    return result;
}

static bool drawSliderWithValue(GLFWwindow* win, const GuiElement* sliderElem,
                                 const GuiElement* valueElem,
                                 float* value, float minVal, float maxVal)
{
    if (!sliderElem) return false;
    bool changed = uiSlider(win, "slider",
        {sliderElem->x, sliderElem->y, sliderElem->w, sliderElem->h},
        value, minVal, maxVal);
    if (valueElem) {
        char buf[32];
        if (maxVal <= 1.0f)
            snprintf(buf, sizeof(buf), "%.2f", *value);
        else
            snprintf(buf, sizeof(buf), "%.0f", *value);
        uiDrawText(buf, uiScaleX(valueElem->x), uiScaleY(valueElem->y), 0.30f,
                   {0.9f, 0.95f, 1.0f, 1.0f});
    }
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
    const GuiElement* titleEl = layout.get("settingsTitle");
    if (titleEl)
        uiDrawText("SETTINGS", uiScaleX(titleEl->x), uiScaleY(titleEl->y), 0.65f, {0.9f, 0.95f, 1.0f, 1.0f});

    // ===== LEFT COLUMN: AUDIO =====
    drawHeading("AUDIO", layout.get("audioHeader"));

    drawLabel("Master Volume", layout.get("masterVolumeLabel"));
    if (drawSliderWithValue(win,
        layout.get("masterVolumeSlider"),
        layout.get("masterVolumeValue"),
        &settings.masterVolume, 0.0f, 1.0f))
        SavePlayerSettings();

    drawLabel("Music Volume", layout.get("musicVolumeLabel"));
    if (drawSliderWithValue(win,
        layout.get("musicVolumeSlider"),
        layout.get("musicVolumeValue"),
        &settings.musicVolume, 0.0f, 1.0f))
    {
        MusicManager::instance().setVolume(settings.musicVolume);
        SavePlayerSettings();
    }

    drawLabel("Mute", layout.get("muteLabel"));
    {
        const GuiElement* mb = layout.get("muteToggle");
        if (mb && uiCheckbox(win, "MUTE", {mb->x, mb->y, mb->w, mb->h}, &settings.musicMuted))
        {
            MusicManager::instance().setMuted(settings.musicMuted);
            SavePlayerSettings();
        }
    }

    // ===== LEFT COLUMN: GAMEPLAY =====
    drawHeading("GAMEPLAY", layout.get("gameplayHeader"));

    drawLabel("Field of View", layout.get("fovLabel"));
    if (drawSliderWithValue(win,
        layout.get("fovSlider"),
        layout.get("fovValue"),
        &settings.fov, 10.0f, 350.0f))
    {
        CAMERA_FOV = settings.fov;
        SavePlayerSettings();
    }

    drawLabel("Sensitivity", layout.get("sensitivityLabel"));
    if (drawSliderWithValue(win,
        layout.get("sensitivitySlider"),
        layout.get("sensitivityValue"),
        &settings.sensitivity, 0.1f, 10.0f))
    {
        CAMERA_SENS = settings.sensitivity;
        SavePlayerSettings();
    }

    // ===== RIGHT COLUMN: VIDEO =====
    drawHeading("VIDEO", layout.get("videoHeader"));

    drawLabel("Fullscreen", layout.get("fullscreenLabel"));
    {
        int fsIdx = VideoSettings::instance().fullscreen() ? 1 : 0;
        int next = drawDropdown(win, layout.get("fullscreenToggle"),
            kOnOff, 2, fsIdx);
        if (next >= 0 && next != fsIdx)
            VideoSettings::instance().setFullscreen(next == 1);
    }

    drawLabel("Resolution", layout.get("resolutionLabel"));
    {
        int idx = VideoSettings::instance().resolutionIndex() - 1;
        if (idx < 0 || idx >= VideoSettings::NUM_RESOLUTIONS) idx = 0;
        int next = drawDropdown(win, layout.get("resolutionDropdown"),
            kResolutionLabels, VideoSettings::NUM_RESOLUTIONS, idx);
        if (next >= 0 && next != idx)
            VideoSettings::instance().setResolution(next + 1);
    }

    drawLabel("Graphics Preset", layout.get("graphicsLabel"));
    {
        int idx = -1;
        for (int i = 0; i < kNumGraphicsPresets; ++i)
            if (settings.graphicsPreset == kGraphicsPresets[i]) { idx = i; break; }
        if (idx < 0) idx = 2;
        int next = drawDropdown(win, layout.get("graphicsDropdown"),
            kGraphicsPresets, kNumGraphicsPresets, idx);
        if (next >= 0 && next != idx)
        {
            settings.graphicsPreset = kGraphicsPresets[next];
            SavePlayerSettings();
        }
    }

    // ===== RIGHT COLUMN: DEBUG =====
    drawHeading("DEBUG", layout.get("debugHeader"));

    drawLabel("Physics Debug", layout.get("physicsDebugLabel"));
    {
        static bool physicsDebug = true;
        const GuiElement* tb = layout.get("physicsDebugToggle");
        if (tb && uiButton(win, physicsDebug ? "ON" : "OFF",
            {tb->x, tb->y, tb->w, tb->h},
            physicsDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "physics_debug").clicked)
            physicsDebug = !physicsDebug;
    }

    drawLabel("Render Debug", layout.get("renderDebugLabel"));
    {
        static bool renderDebug = true;
        const GuiElement* tb = layout.get("renderDebugToggle");
        if (tb && uiButton(win, renderDebug ? "ON" : "OFF",
            {tb->x, tb->y, tb->w, tb->h},
            renderDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "render_debug").clicked)
            renderDebug = !renderDebug;
    }

    // ===== BACK BUTTON =====
    {
        const GuiElement* bb = layout.get("backButton");
        if (bb && uiButton(win, "BACK", {bb->x, bb->y, bb->w, bb->h}, {0.7f, 0.2f, 0.2f, 1.0f}).clicked)
        {
            printf("[SETTINGS MENU] Back pressed\n");
            r.goBack = true;
        }
    }

    return r;
}
