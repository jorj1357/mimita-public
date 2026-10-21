#pragma once

enum class FullscreenMode : int {
    Windowed = 0,
    Exclusive = 1,
    Borderless = 2,
    ExclusiveStretched = 3,
    BorderlessStretched = 4,
    ExclusiveFullFrameStretched = 5,
    BorderlessFullFrameStretched = 6
};

inline const char* fullscreenModeName(FullscreenMode mode)
{
    switch (mode) {
        case FullscreenMode::Windowed: return "windowed";
        case FullscreenMode::Exclusive: return "exclusive";
        case FullscreenMode::Borderless: return "borderless";
        case FullscreenMode::ExclusiveStretched: return "exclusive_stretched";
        case FullscreenMode::BorderlessStretched: return "borderless_stretched";
        case FullscreenMode::ExclusiveFullFrameStretched: return "exclusive_full_frame_stretched";
        case FullscreenMode::BorderlessFullFrameStretched: return "borderless_full_frame_stretched";
    }
    return "unknown";
}

inline bool fullscreenModeIsStretched(FullscreenMode mode)
{
    return mode == FullscreenMode::ExclusiveStretched ||
           mode == FullscreenMode::BorderlessStretched ||
           mode == FullscreenMode::ExclusiveFullFrameStretched ||
           mode == FullscreenMode::BorderlessFullFrameStretched;
}

inline bool fullscreenModeStretchesWholeFrame(FullscreenMode mode)
{
    return mode == FullscreenMode::ExclusiveFullFrameStretched ||
           mode == FullscreenMode::BorderlessFullFrameStretched;
}
