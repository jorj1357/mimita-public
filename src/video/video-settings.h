#pragma once

#include <string>
#include "video/video-mode.h"

// Single owner of resolution and fullscreen settings.
// Every system (terminal commands, settings menu) routes through this API.
// No duplicate logic. No hardcoded GLFW calls scattered across files.
class VideoSettings {
public:
    static VideoSettings& instance();

    // Supported resolutions (1-indexed to match terminal command)
    static constexpr int RES_800x600    = 1;
    static constexpr int RES_1024x768   = 2;
    static constexpr int RES_1280x720   = 3;
    static constexpr int RES_1920x1080  = 4;
    static constexpr int NUM_RESOLUTIONS = 4;

    struct Resolution {
        int w;
        int h;
    };
    static const Resolution kResolutions[NUM_RESOLUTIONS];

    // Set resolution by index (1–4). Calls apply() internally.
    void setResolution(int index);

    // Set fullscreen mode (0-6). Calls apply() internally.
    bool setFullscreenMode(int mode);

    // Compatibility entry point for existing on/off controls.
    void setFullscreen(bool enabled);

    // Apply current settings to the GLFW window.
    // Handles windowed↔fullscreen transitions and resolution changes.
    void apply();

    // Query
    int resolutionIndex() const { return mIndex; }
    bool fullscreen() const { return mFullscreenMode != FullscreenMode::Windowed; }
    FullscreenMode fullscreenMode() const { return mFullscreenMode; }
    int width() const;
    int height() const;

    // Frame pacing
    void setMaxFrames(int fps);
    int maxFrames() const { return mMaxFrames; }
    void setVSync(bool on);
    bool vsync() const { return mVSync; }

    // Persistence
    void load();
    void save();

private:
    VideoSettings() = default;

    int mIndex = RES_800x600;
    FullscreenMode mFullscreenMode = FullscreenMode::Windowed;
    int mMaxFrames = 999;
    bool mVSync = false;
};
