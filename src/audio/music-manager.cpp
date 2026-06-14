#include "music-manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "miniaudio.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"

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
    Debug::log(Debug::Category::Audio, "[MUSIC] volume=%.2f\n", mVolume);
}

float MusicManager::volume() const { return mVolume; }

void MusicManager::setMuted(bool m)
{
    mMuted = m;
    applyVolume();
}

bool MusicManager::muted() const { return mMuted; }

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
    float sw = uiScreenW();
    float sh = uiScreenH();

    float iconSize = 36.0f;
    float ix = sw - iconSize - 10.0f;
    float iy = sh - iconSize - 10.0f;

    UIRect iconRect = {ix, iy, iconSize, iconSize};
    uiDrawRect(iconRect, {0.12f, 0.12f, 0.16f, 0.85f}, "music-widget-icon");
    uiDrawRectOutline(iconRect, {0.4f, 0.6f, 0.9f, 0.8f}, "music-widget-border");
    uiDrawText("♪", ix + 7.0f, iy + 5.0f, 0.5f, {0.7f, 0.85f, 1.0f, 0.9f});

    bool hovering = false;
    double mx, my;
    glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    bool mouseInIcon = mx >= ix && mx <= ix + iconSize && my >= iy && my <= iy + iconSize;

    if (!mouseInIcon) return;

    float panelW = 300.0f;
    float panelH = 200.0f;
    float px = sw - panelW - 10.0f;
    float py = sh - panelH - iconSize - 16.0f;

    UIRect panel = {px, py, panelW, panelH};
    bool mouseInPanel = mx >= px && mx <= px + panelW && my >= py && my <= py + panelH;

    if (!mouseInPanel && !mouseInIcon) return;

    uiDrawRect(panel, {0.06f, 0.06f, 0.1f, 0.95f}, "music-widget-panel");
    uiDrawRectOutline(panel, {0.3f, 0.5f, 0.8f, 0.9f}, "music-widget-panel-border");

    float y = py + 12.0f;
    uiDrawText("MUSIC PLAYER", px + 12.0f, y, 0.36f, {0.6f, 0.8f, 1.0f, 1.0f});
    y += 28.0f;

    std::string info = currentTrackInfo();
    uiDrawText(info.c_str(), px + 12.0f, y, 0.32f, {0.85f, 0.88f, 1.0f, 1.0f});
    y += 26.0f;

    GLFWwindow* win = glfwGetCurrentContext();
    float btnW = 50.0f;
    float btnH = 26.0f;
    float gap = 8.0f;
    float btnY = y;

    if (uiButton(win, isPlaying() ? "||" : "|>",
        {px + 12.0f, btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-pause").clicked)
    {
        if (isPlaying()) pause(); else resume();
    }
    if (uiButton(win, ">>",
        {px + 12.0f + (btnW + gap), btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-skip").clicked)
    {
        skip();
    }
    if (uiButton(win, "<<",
        {px + 12.0f + (btnW + gap) * 2, btnY, btnW, btnH}, {0.2f, 0.3f, 0.5f, 1.0f}, "music-prev").clicked)
    {
        previous();
    }

    y += btnH + 16.0f;

    float sliderW = panelW - 24.0f;
    UIRect volRect = {px + 12.0f, y, sliderW, 20.0f};
    if (uiSlider(win, "VOLUME", volRect, &mVolume, 0.0f, 1.0f))
        applyVolume();

    y += 36.0f;

    bool muteVal = mMuted;
    uiCheckbox(win, "MUTE",
        {px + 12.0f, y, 50.0f, 22.0f}, &muteVal);
    if (muteVal != mMuted) {
        mMuted = muteVal;
        applyVolume();
    }
}

void MusicManager::drawAllOverlay()
{
    drawNowPlayingPopup();
    drawMusicWidget();
}
