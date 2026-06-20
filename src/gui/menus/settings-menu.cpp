#include "settings-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "../ui-system.h"
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

    // Render all static text/heading elements using the unified renderer
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        // Skip non-text types and special interactive elements
        if (elem->type != "text" && elem->type != "label") continue;

        drawGuiElement(win, *elem);
    }

    // ===== LEFT COLUMN: AUDIO =====
    if (drawSliderWithValue(win,
        layout.get("masterVolumeSlider"),
        layout.get("masterVolumeValue"),
        &settings.masterVolume, 0.0f, 1.0f))
        SavePlayerSettings();

    if (drawSliderWithValue(win,
        layout.get("musicVolumeSlider"),
        layout.get("musicVolumeValue"),
        &settings.musicVolume, 0.0f, 1.0f))
    {
        MusicManager::instance().setVolume(settings.musicVolume);
        SavePlayerSettings();
    }

    {
        const GuiElement* mb = layout.get("muteToggle");
        if (mb && uiCheckbox(win, "MUTE", {mb->x, mb->y, mb->w, mb->h}, &settings.musicMuted))
        {
            MusicManager::instance().setMuted(settings.musicMuted);
            SavePlayerSettings();
        }
    }

    // ===== LEFT COLUMN: GAMEPLAY =====
    if (drawSliderWithValue(win,
        layout.get("fovSlider"),
        layout.get("fovValue"),
        &settings.fov, 10.0f, 350.0f))
    {
        CAMERA_FOV = settings.fov;
        SavePlayerSettings();
    }

    if (drawSliderWithValue(win,
        layout.get("sensitivitySlider"),
        layout.get("sensitivityValue"),
        &settings.sensitivity, 0.1f, 10.0f))
    {
        CAMERA_SENS = settings.sensitivity;
        SavePlayerSettings();
    }

    {
        const GuiElement* cb = layout.get("bloodFxToggle");
        if (cb && uiCheckbox(win, "BLOOD", {cb->x, cb->y, cb->w, cb->h}, &settings.bloodFX))
        {
            gBloodFXEnabled = settings.bloodFX;
            SavePlayerSettings();
            Debug::log(Debug::Category::NpcCombat, "[BLOODFX] %s",
                gBloodFXEnabled ? "Enabled" : "Disabled");
        }
    }

    if (drawSliderWithValue(win,
        layout.get("sfxSlider"),
        layout.get("sfxValue"),
        &settings.sfxVolume, 0.0f, 1.0f))
        SavePlayerSettings();

    // ===== RIGHT COLUMN: VIDEO =====
    {
        int fsIdx = VideoSettings::instance().fullscreen() ? 1 : 0;
        int next = drawDropdown(win, layout.get("fullscreenToggle"),
            kOnOff, 2, fsIdx);
        if (next >= 0 && next != fsIdx)
            VideoSettings::instance().setFullscreen(next == 1);
    }

    {
        int idx = VideoSettings::instance().resolutionIndex() - 1;
        if (idx < 0 || idx >= VideoSettings::NUM_RESOLUTIONS) idx = 0;
        int next = drawDropdown(win, layout.get("resolutionDropdown"),
            kResolutionLabels, VideoSettings::NUM_RESOLUTIONS, idx);
        if (next >= 0 && next != idx)
            VideoSettings::instance().setResolution(next + 1);
    }

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
    {
        static bool physicsDebug = true;
        const GuiElement* tb = layout.get("physicsDebugToggle");
        if (tb && uiButton(win, physicsDebug ? "ON" : "OFF",
            {tb->x, tb->y, tb->w, tb->h},
            physicsDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "physics_debug").clicked)
            physicsDebug = !physicsDebug;
    }

    {
        static bool renderDebug = true;
        const GuiElement* tb = layout.get("renderDebugToggle");
        if (tb && uiButton(win, renderDebug ? "ON" : "OFF",
            {tb->x, tb->y, tb->w, tb->h},
            renderDebug ? glm::vec4(0.25f,0.7f,0.35f,1) : glm::vec4(0.5f,0.2f,0.2f,1),
            "render_debug").clicked)
            renderDebug = !renderDebug;
    }

    // ===== CROSSHAIR =====
    {
        auto& config = CrosshairConfig::instance();
        auto& crosshair = config.edit();
        uiDrawText("CROSSHAIR", uiScaleX(1080.0f), uiScaleY(120.0f), 0.48f,
                   {0.65f, 0.85f, 1.0f, 1.0f});
        uiDrawRect({uiScaleX(1450.0f), uiScaleY(150.0f),
                    uiScaleX(300.0f), uiScaleY(220.0f)},
                   {0.08f, 0.09f, 0.12f, 1.0f}, "crosshair-preview");
        drawCrosshairPreview(uiScaleX(1600.0f), uiScaleY(260.0f),
                             std::min(uiScaleX(2.0f), uiScaleY(2.0f)));

        bool changed = false;
        auto slider = [&](const char* label, float y, float* value,
                          float minValue, float maxValue) {
            uiDrawText(label, uiScaleX(1080.0f), uiScaleY(y), 0.30f,
                       {0.8f, 0.85f, 0.95f, 1.0f});
            changed |= uiSlider(win, label, {1080.0f, y + 28.0f, 300.0f, 24.0f},
                                value, minValue, maxValue);
        };
        slider("Size", 176.0f, &crosshair.size, 0.0f, 32.0f);
        slider("Gap", 246.0f, &crosshair.gap, 0.0f, 32.0f);
        slider("Thickness", 316.0f, &crosshair.thickness, 1.0f, 10.0f);

        float red = (float)crosshair.red;
        float green = (float)crosshair.green;
        float blue = (float)crosshair.blue;
        slider("Red", 386.0f, &red, 0.0f, 255.0f);
        slider("Green", 456.0f, &green, 0.0f, 255.0f);
        slider("Blue", 526.0f, &blue, 0.0f, 255.0f);
        crosshair.red = (int)red;
        crosshair.green = (int)green;
        crosshair.blue = (int)blue;

        changed |= uiCheckbox(win, "Dot", {1080.0f, 610.0f, 72.0f, 32.0f}, &crosshair.dot);
        changed |= uiCheckbox(win, "Outline", {1080.0f, 658.0f, 72.0f, 32.0f}, &crosshair.outline);
        changed |= uiCheckbox(win, "Dynamic", {1080.0f, 706.0f, 72.0f, 32.0f}, &crosshair.dynamic);
        changed |= uiCheckbox(win, "Top", {1280.0f, 610.0f, 72.0f, 32.0f}, &crosshair.showTop);
        changed |= uiCheckbox(win, "Bottom", {1280.0f, 658.0f, 72.0f, 32.0f}, &crosshair.showBottom);
        changed |= uiCheckbox(win, "Left", {1450.0f, 610.0f, 72.0f, 32.0f}, &crosshair.showLeft);
        changed |= uiCheckbox(win, "Right", {1450.0f, 658.0f, 72.0f, 32.0f}, &crosshair.showRight);
        if (changed) config.save();
    }

    // ===== BACK BUTTON =====
    {
        const GuiElement* bb = layout.get("backButton");
        if (bb && drawGuiElement(win, *bb).clicked)
        {
            printf("[SETTINGS MENU] Back pressed\n");
            r.goBack = true;
        }
    }

    return r;
}
