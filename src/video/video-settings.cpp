#include "video-settings.h"
#include "renderer/renderer.h"
#include "render/post-fx.h"

#include <cstdio>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern Renderer* gRenderer;

const VideoSettings::Resolution VideoSettings::kResolutions[NUM_RESOLUTIONS] = {
    {800, 600},
    {1024, 768},
    {1280, 720},
    {1920, 1080}
};

VideoSettings& VideoSettings::instance()
{
    static VideoSettings vs;
    return vs;
}

int VideoSettings::width() const
{
    if (mIndex < 1 || mIndex > NUM_RESOLUTIONS)
        return kResolutions[RES_1280x720 - 1].w;
    return kResolutions[mIndex - 1].w;
}

int VideoSettings::height() const
{
    if (mIndex < 1 || mIndex > NUM_RESOLUTIONS)
        return kResolutions[RES_1280x720 - 1].h;
    return kResolutions[mIndex - 1].h;
}

void VideoSettings::setResolution(int index)
{
    if (index < 1 || index > NUM_RESOLUTIONS)
    {
        printf("[VIDEO] Invalid resolution index: %d (must be 1-%d)\n", index, NUM_RESOLUTIONS);
        return;
    }
    mIndex = index;
    printf("[VIDEO] Resolution set to %dx%d (index %d)\n", width(), height(), mIndex);
    apply();
    save();
}

void VideoSettings::setFullscreen(bool enabled)
{
    mFullscreen = enabled;
    printf("[VIDEO] Fullscreen: %s\n", enabled ? "ON" : "OFF");
    apply();
    save();
}

void VideoSettings::setMaxFrames(int fps)
{
    mMaxFrames = std::clamp(fps, 10, 999);
    printf("[VIDEO] maxFrames=%d\n", mMaxFrames);
    save();
}

void VideoSettings::setVSync(bool on)
{
    mVSync = on;
    printf("[VIDEO] vsync=%s\n", on ? "ON" : "OFF");
    if (gRenderer)
        gRenderer->setVSync(on);
    save();
}

void VideoSettings::apply()
{
    if (!gRenderer || !gRenderer->window)
    {
        printf("[VIDEO] Cannot apply: no window\n");
        return;
    }
    gRenderer->applyVideoMode(width(), height(), mFullscreen);
    // applyVideoMode may reset the driver swap interval; restore the config.
    gRenderer->setVSync(mVSync);
    PostFX::instance().initFBO(gRenderer->width, gRenderer->height);
}

void VideoSettings::load()
{
    const std::string path = "config/video-settings.json";
    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("[VIDEO] No config file at %s, using defaults\n", path.c_str());
        return;
    }

    try
    {
        json j;
        file >> j;

        if (j.contains("resolution_index"))
            mIndex = std::clamp(j["resolution_index"].get<int>(), 1, NUM_RESOLUTIONS);
        if (j.contains("fullscreen"))
            mFullscreen = j["fullscreen"].get<bool>();
        if (j.contains("maxFrames"))
            mMaxFrames = std::clamp(j["maxFrames"].get<int>(), 10, 999);
        if (j.contains("vsync"))
            mVSync = j["vsync"].get<bool>();

        printf("[VIDEO] Loaded: resolution=%d (%dx%d) fullscreen=%d maxFrames=%d vsync=%d\n",
               mIndex, width(), height(), (int)mFullscreen, mMaxFrames, (int)mVSync);
    }
    catch (const std::exception& e)
    {
        printf("[VIDEO] Error loading %s: %s\n", path.c_str(), e.what());
    }
}

void VideoSettings::save()
{
    const std::string path = "config/video-settings.json";

    std::error_code ec;
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, ec);

    json j;
    j["resolution_index"] = mIndex;
    j["fullscreen"] = mFullscreen;
    j["maxFrames"] = mMaxFrames;
    j["vsync"] = mVSync;
    j["width"] = width();
    j["height"] = height();

    std::ofstream file(path);
    if (!file.is_open())
    {
        printf("[VIDEO] Failed to write %s\n", path.c_str());
        return;
    }
    file << j.dump(2);
    printf("[VIDEO] Saved: resolution=%d (%dx%d) fullscreen=%d\n",
           mIndex, width(), height(), (int)mFullscreen);
}
