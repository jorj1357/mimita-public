#include "music-manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "miniaudio.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "gui/gui-coord.h"

using json = nlohmann::json;

MusicManager& MusicManager::instance()
{
    static MusicManager mgr;
    return mgr;
}

static bool hasMusicExt(const std::string& name)
{
    std::string ext;
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) {
        for (char c : name.substr(dot + 1))
            ext.push_back((char)std::tolower((unsigned char)c));
    }
    return ext == "mp3" || ext == "wav" || ext == "ogg";
}

void MusicManager::scanFolder(const std::string& dir, std::vector<TrackEntry>& out)
{
    out.clear();
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
        return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        if (!hasMusicExt(fname)) continue;
        out.push_back({entry.path().string(), fname});
    }
    std::sort(out.begin(), out.end(),
        [](const TrackEntry& a, const TrackEntry& b) { return a.filename < b.filename; });
}

void MusicManager::loadCredits(const std::string& path)
{
    mCredits.clear();
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::log(Debug::Category::Audio, "[MUSIC] credits file not found: %s\n", path.c_str());
        return;
    }
    try {
        json j;
        file >> j;
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string fname = it.key();
            std::string artist = it.value().value("artist", std::string());
            std::string title = it.value().value("title", std::string());
            if (!artist.empty() && !title.empty())
                mCredits[fname] = {artist, title};
        }
        Debug::log(Debug::Category::Audio, "[MUSIC] loaded %zu credits\n", mCredits.size());
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Audio, "[MUSIC] credits parse error: %s\n", e.what());
    }
}

std::string MusicManager::displayName(const std::string& filename) const
{
    auto it = mCredits.find(filename);
    if (it != mCredits.end())
        return it->second.first + " - " + it->second.second;
    return filename;
}

void MusicManager::uninitSound()
{
    if (mCurrentSound) {
        ma_sound_uninit(mCurrentSound);
        delete mCurrentSound;
        mCurrentSound = nullptr;
    }
}

void MusicManager::applyVolume()
{
    if (mCurrentSound) {
        float effective = mMuted ? 0.0f : std::clamp(mVolume, 0.0f, 1.0f);
        ma_sound_set_volume(mCurrentSound, effective);
    }
}

void MusicManager::applyPlaybackSpeed()
{
    if (mCurrentSound) {
        float clamped = std::clamp(mPlaybackSpeed, 0.25f, 2.0f);
        ma_sound_set_pitch(mCurrentSound, clamped);
    }
}

void MusicManager::startTrack(const std::string& path)
{
    uninitSound();

    if (!mEngine) return;
    if (!std::filesystem::exists(path)) return;

    mCurrentSound = new ma_sound();
    ma_result result = ma_sound_init_from_file(mEngine, path.c_str(),
        MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, mCurrentSound);
    if (result != MA_SUCCESS) {
        Debug::log(Debug::Category::Audio, "[MUSIC] failed to load: %s\n", path.c_str());
        delete mCurrentSound;
        mCurrentSound = nullptr;
        return;
    }

    mCurrentPath = path;
    mCurrentFilename = std::filesystem::path(path).filename().string();
    auto it = mCredits.find(mCurrentFilename);
    if (it != mCredits.end()) {
        mCurrentArtist = it->second.first;
        mCurrentTitle = it->second.second;
    } else {
        mCurrentArtist.clear();
        mCurrentTitle.clear();
    }

    applyVolume();
    applyPlaybackSpeed();
    ma_sound_start(mCurrentSound);

    mTrackJustChanged = true;
    mTimeSinceTrackChange = 0.0f;
    mPopupAlpha = 0.0f;
    mPopupSlide = 200.0f;

    Debug::log(Debug::Category::Audio, "[MUSIC] track=\"%s\"\n", currentTrackInfo().c_str());
}

void MusicManager::pickMenuTrack()
{
    if (mMenuTracks.empty()) return;
    std::uniform_int_distribution<size_t> dist(0, mMenuTracks.size() - 1);
    const auto& t = mMenuTracks[dist(mRng)];
    startTrack(t.path);
}

void MusicManager::playNextIngame()
{
    if (mPlaylist.empty()) return;

    mPlaylistIndex++;
    if (mPlaylistIndex >= mPlaylist.size()) {
        std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
        mPlaylistIndex = 0;
        Debug::log(Debug::Category::Audio, "[MUSIC] playlist reshuffled (%zu tracks)\n", mPlaylist.size());
    }

    const auto& t = mPlaylist[mPlaylistIndex];
    startTrack(t.path);
}

void MusicManager::init()
{
    if (mInitialized) return;

    mEngine = new ma_engine();
    ma_result result = ma_engine_init(NULL, mEngine);
    if (result != MA_SUCCESS) {
        Debug::log(Debug::Category::Audio, "[MUSIC] engine init failed\n");
        delete mEngine;
        mEngine = nullptr;
        return;
    }

    mRng.seed(std::random_device{}());

    scanFolder("assets/sound/music/mainmenu", mMenuTracks);
    scanFolder("assets/sound/music/ingame", mIngameTracks);
    loadCredits("assets/sound/music/credits.json");

    Debug::log(Debug::Category::Audio, "[MUSIC] loaded tracks=%zu menu=%zu ingame=%zu\n",
        mMenuTracks.size() + mIngameTracks.size(), mMenuTracks.size(), mIngameTracks.size());

    loadConfig();
    mInitialized = true;
    enterMenuMode();
}

void MusicManager::shutdown()
{
    uninitSound();
    if (mEngine) {
        ma_engine_uninit(mEngine);
        delete mEngine;
        mEngine = nullptr;
    }
    mInitialized = false;
}

void MusicManager::update(float dt)
{
    if (!mInitialized) return;
    // Hot reload: check if config file changed on disk
    {
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(mConfigPath, ec);
        if (!ec && wt != mConfigLastWrite) {
            Debug::log(Debug::Category::Audio, "[MUSIC] config changed on disk, reloading\n");
            loadConfig();
        }
    }
    mWidgetDt = dt;

    if (mCurrentSound && ma_sound_at_end(mCurrentSound)) {
        if (mMode == Mode::Ingame) {
            if (!mPlaylist.empty())
                playNextIngame();
        } else if (mMode == Mode::Menu) {
            if (!mMenuTracks.empty())
                pickMenuTrack();
        }
    }

    if (mTrackJustChanged) {
        mTimeSinceTrackChange += dt;
        float slideIn = 0.3f;
        float stay = 5.0f;
        float fadeOut = 0.5f;
        float total = slideIn + stay + fadeOut;

        if (mTimeSinceTrackChange < slideIn) {
            float t = mTimeSinceTrackChange / slideIn;
            mPopupSlide = (1.0f - t) * 200.0f;
            mPopupAlpha = t;
        } else if (mTimeSinceTrackChange < slideIn + stay) {
            mPopupSlide = 0.0f;
            mPopupAlpha = 1.0f;
        } else if (mTimeSinceTrackChange < total) {
            float t = (mTimeSinceTrackChange - slideIn - stay) / fadeOut;
            mPopupAlpha = 1.0f - t;
        } else {
            mTrackJustChanged = false;
        }
    }
}

void MusicManager::enterMenuMode()
{
    if (!mInitialized) return;
    if (mMode == Mode::Menu) return;

    mMode = Mode::Menu;

    if (!mMenuTracks.empty()) {
        pickMenuTrack();
    }

    Debug::log(Debug::Category::Audio, "[MUSIC] mode=menu\n");
}

void MusicManager::enterGameMode()
{
    if (!mInitialized) return;
    if (mMode == Mode::Ingame) return;

    mMode = Mode::Ingame;
    uninitSound();

    if (mIngameTracks.empty()) return;

    mPlaylist = mIngameTracks;
    std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
    mPlaylistIndex = 0;

    std::uniform_int_distribution<size_t> dist(0, mPlaylist.size() - 1);
    mPlaylistIndex = dist(mRng);

    const auto& t = mPlaylist[mPlaylistIndex];
    startTrack(t.path);

    Debug::log(Debug::Category::Audio, "[MUSIC] mode=ingame playlist=%zu\n", mPlaylist.size());
}

void MusicManager::pause()
{
    if (!mInitialized || !mCurrentSound) return;
    ma_sound_stop(mCurrentSound);
}

void MusicManager::resume()
{
    if (!mInitialized || !mCurrentSound) return;
    if (!ma_sound_is_playing(mCurrentSound))
        ma_sound_start(mCurrentSound);
}

void MusicManager::skip()
{
    if (!mInitialized) return;
    if (mMode == Mode::Ingame && !mPlaylist.empty())
        playNextIngame();
    else if (mMode == Mode::Menu && !mMenuTracks.empty())
        pickMenuTrack();
}

void MusicManager::previous()
{
    if (!mInitialized || mMode != Mode::Ingame || mPlaylist.empty()) return;

    if (mPlaylistIndex == 0)
        mPlaylistIndex = mPlaylist.size() - 1;
    else
        mPlaylistIndex--;

    const auto& t = mPlaylist[mPlaylistIndex];
    startTrack(t.path);
}

void MusicManager::stop()
{
    uninitSound();
    mMode = Mode::None;
}

void MusicManager::reload()
{
    scanFolder("assets/sound/music/mainmenu", mMenuTracks);
    scanFolder("assets/sound/music/ingame", mIngameTracks);
    loadCredits("assets/sound/music/credits.json");
    Debug::log(Debug::Category::Audio, "[MUSIC] reloaded tracks=%zu menu=%zu ingame=%zu\n",
        mMenuTracks.size() + mIngameTracks.size(), mMenuTracks.size(), mIngameTracks.size());
}

void MusicManager::setVolume(float vol)
{
    mVolume = std::clamp(vol, 0.0f, 1.0f);
    applyVolume();
    saveConfig();
    Debug::log(Debug::Category::Audio, "[MUSIC] volume=%.2f\n", mVolume);
}

float MusicManager::volume() const { return mVolume; }

void MusicManager::setMuted(bool m)
{
    mMuted = m;
    applyVolume();
    saveConfig();
}

bool MusicManager::muted() const { return mMuted; }

void MusicManager::setPlaybackSpeed(float speed)
{
    mPlaybackSpeed = std::clamp(speed, 0.25f, 2.0f);
    applyPlaybackSpeed();
    saveConfig();
    Debug::log(Debug::Category::Audio, "[MUSIC] playbackSpeed applied=%.2f\n", mPlaybackSpeed);
}

float MusicManager::playbackSpeed() const { return mPlaybackSpeed; }

bool MusicManager::isPlaying() const
{
    return mInitialized && mCurrentSound && ma_sound_is_playing(mCurrentSound);
}

bool MusicManager::isMenuMode() const { return mMode == Mode::Menu; }

bool MusicManager::trackJustChanged() const { return mTrackJustChanged; }

float MusicManager::timeSinceTrackChange() const { return mTimeSinceTrackChange; }

std::string MusicManager::currentTrackInfo() const
{
    if (mCurrentPath.empty()) return "(no track)";
    return displayName(mCurrentFilename);
}

void MusicManager::drawNowPlayingPopup()
{
    if (!mTrackJustChanged) return;

    float sw = uiScreenW();
    float sh = uiScreenH();

    float pw = 340.0f;
    float ph = 72.0f;
    float px = sw - pw - 20.0f + mPopupSlide;
    float py = sh - ph - 100.0f;

    glm::vec4 bg = {0.04f, 0.04f, 0.07f, mPopupAlpha * 0.92f};
    glm::vec4 border = {0.3f, 0.65f, 1.0f, mPopupAlpha};
    glm::vec4 heading = {0.5f, 0.8f, 1.0f, mPopupAlpha};
    glm::vec4 text = {0.9f, 0.9f, 1.0f, mPopupAlpha};

    UIRect r = {px, py, pw, ph};
    uiDrawRect(r, bg, "now-playing-bg");
    uiDrawRectOutline(r, border, "now-playing-border");
    uiDrawText("NOW PLAYING", px + 14.0f, py + 8.0f, 0.32f, heading);

    std::string info = currentTrackInfo();
    uiDrawText(info.c_str(), px + 14.0f, py + 36.0f, 0.38f, text);
}

void MusicManager::drawMusicWidget()
{
    GLFWwindow* win = glfwGetCurrentContext();
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

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
    const float panelX = iconX - (panelW - iconS);  // align right edges
    const float panelY = iconY - panelH - 6.0f;
    const UIRect panelRect = {panelX, panelY, panelW, panelH};

    // Hover detection
    bool hoverIcon = cdx >= iconRect.x && cdx <= iconRect.x + iconRect.w &&
                     cdy >= iconRect.y && cdy <= iconRect.y + iconRect.h;
    bool hoverPanel = cdx >= panelRect.x && cdx <= panelRect.x + panelRect.w &&
                      cdy >= panelRect.y && cdy <= panelRect.y + panelRect.h;
    bool hoverAny = hoverIcon || hoverPanel;

    // Update open/close state (called each frame from guiMain -> drawAllOverlay)
    // Reset close timer when hovering any part of the widget
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

    // Draw icon (always visible)
    uiDrawRect(iconRect, {0.12f, 0.12f, 0.16f, 0.85f}, "music-widget-icon");
    uiDrawRectOutline(iconRect, {0.4f, 0.6f, 0.9f, 0.8f}, "music-widget-border");
    uiDrawText("♪", uiScaleX(iconX + 7.0f), uiScaleY(iconY + 5.0f), 0.5f, {0.7f, 0.85f, 1.0f, 0.9f});

    if (!mWidgetPanelOpen) return;

    // Draw panel
    uiDrawRect(panelRect, {0.06f, 0.06f, 0.1f, 0.95f}, "music-widget-panel");
    uiDrawRectOutline(panelRect, {0.3f, 0.5f, 0.8f, 0.9f}, "music-widget-panel-border");

    float y = panelY + 10.0f;
    uiDrawText("MUSIC PLAYER", uiScaleX(panelX + 12.0f), uiScaleY(y), 0.36f,
               {0.6f, 0.8f, 1.0f, 1.0f});
    y += 26.0f;

    std::string info = currentTrackInfo();
    uiDrawText(info.c_str(), uiScaleX(panelX + 12.0f), uiScaleY(y), 0.30f,
               {0.85f, 0.88f, 1.0f, 1.0f});
    y += 26.0f;

    // Transport buttons
    const float btnW = 50.0f;
    const float btnH = 26.0f;
    const float gap = 8.0f;
    float btnY = y;

    if (uiButton(win, isPlaying() ? "||" : "|>",
        {panelX + 12.0f, btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-pause").clicked)
    {
        if (isPlaying()) pause(); else resume();
    }
    if (uiButton(win, ">>",
        {panelX + 12.0f + (btnW + gap), btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-skip").clicked)
    {
        skip();
    }
    if (uiButton(win, "<<",
        {panelX + 12.0f + (btnW + gap) * 2, btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-prev").clicked)
    {
        previous();
    }

    y += btnH + 14.0f;

    // Volume slider
    if (uiSlider(win, "VOLUME",
        {panelX + 12.0f, y, panelW - 24.0f, 20.0f}, &mVolume, 0.0f, 1.0f))
        applyVolume();
    y += 28.0f;

    // Mute toggle
    if (uiButton(win, mMuted ? "MUTED" : "MUTE ON",
        {panelX + 12.0f, y, 80.0f, 24.0f},
        mMuted ? glm::vec4(0.5f,0.2f,0.2f,1) : glm::vec4(0.25f,0.55f,0.3f,1),
        "music-mute").clicked)
    {
        setMuted(!mMuted);
    }
    y += 30.0f;

    // Speed controls
    {
        char speedLabel[64];
        snprintf(speedLabel, sizeof(speedLabel), "Speed: %.2fx", mPlaybackSpeed);
        uiDrawText(speedLabel, uiScaleX(panelX + 12.0f), uiScaleY(y), 0.30f,
                   {0.7f, 0.9f, 1.0f, 1.0f});
        y += 22.0f;

        const float spBtnW = 60.0f;
        const float spBtnH = 22.0f;
        if (uiButton(win, "Slow",
            {panelX + 12.0f, y, spBtnW, spBtnH}, {0.3f, 0.2f, 0.5f, 1.0f}, "music-speed-down").clicked)
        {
            setPlaybackSpeed(mPlaybackSpeed - 0.1f);
        }
        if (uiButton(win, "Reset",
            {panelX + 12.0f + (spBtnW + 6.0f), y, spBtnW, spBtnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-speed-reset").clicked)
        {
            setPlaybackSpeed(1.0f);
        }
        if (uiButton(win, "Fast",
            {panelX + 12.0f + (spBtnW + 6.0f) * 2, y, spBtnW, spBtnH}, {0.2f, 0.5f, 0.3f, 1.0f}, "music-speed-up").clicked)
        {
            setPlaybackSpeed(mPlaybackSpeed + 0.1f);
        }
    }

    // Debug overlay
    if (mWidgetDebug)
    {
        char buf[256];
        float dx = uiScaleX(panelX);
        float dy = uiScaleY(panelY + panelH + 4.0f);
        uiDrawRect({dx, dy, uiScaleX(280.0f), uiScaleY(100.0f)},
                   {0.0f, 0.0f, 0.0f, 0.8f}, "music-debug-bg");
        snprintf(buf, sizeof(buf), "Panel Open: %s", mWidgetPanelOpen ? "yes" : "no");
        uiDrawText(buf, dx + 4.0f, dy, 0.24f, {1,1,1,1}); dy += uiScaleY(16.0f);
        snprintf(buf, sizeof(buf), "Icon Hovered: %s", hoverIcon ? "yes" : "no");
        uiDrawText(buf, dx + 4.0f, dy, 0.24f, {1,1,1,1}); dy += uiScaleY(16.0f);
        snprintf(buf, sizeof(buf), "Panel Hovered: %s", hoverPanel ? "yes" : "no");
        uiDrawText(buf, dx + 4.0f, dy, 0.24f, {1,1,1,1}); dy += uiScaleY(16.0f);
        snprintf(buf, sizeof(buf), "Close Timer: %.2fs", mWidgetCloseTimer);
        uiDrawText(buf, dx + 4.0f, dy, 0.24f, {1,1,1,1}); dy += uiScaleY(16.0f);
        snprintf(buf, sizeof(buf), "Cursor design: %.0f, %.0f", cdx, cdy);
        uiDrawText(buf, dx + 4.0f, dy, 0.24f, {1,1,1,1});
    }
}

void MusicManager::drawAllOverlay()
{
    drawNowPlayingPopup();
    drawMusicWidget();
}

void MusicManager::loadConfig()
{
    std::ifstream file(mConfigPath);
    if (!file.is_open()) {
        // First launch: save defaults, then load is skipped
        saveConfig();
        return;
    }
    try {
        json j;
        file >> j;
        if (j.contains("musicEnabled")) {
            bool enabled = j["musicEnabled"];
            setMuted(!enabled);
        }
        if (j.contains("musicVolume")) {
            float vol = j["musicVolume"];
            setVolume(vol);
        }
        if (j.contains("playbackSpeed")) {
            float speed = j["playbackSpeed"];
            setPlaybackSpeed(speed);
        }
        std::error_code ec;
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        Debug::log(Debug::Category::Audio, "[MUSIC CONFIG] loaded path=%s musicMuted=%d volume=%.2f playbackSpeed=%.2f\n",
                   mConfigPath.c_str(), (int)mMuted, mVolume, mPlaybackSpeed);
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Audio, "[MUSIC] config parse error: %s\n", e.what());
    }
}

void MusicManager::saveConfig()
{
    try {
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(mConfigPath).parent_path(), ec);
        json j;
        j["musicEnabled"] = !mMuted;
        j["musicVolume"] = mVolume;
        j["playbackSpeed"] = mPlaybackSpeed;
        std::ofstream file(mConfigPath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        }
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Audio, "[MUSIC] config save error: %s\n", e.what());
    }
}
