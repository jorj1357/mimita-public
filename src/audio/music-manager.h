#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <random>

#include "miniaudio.h"

class MusicManager {
public:
    static MusicManager& instance();

    void init();
    void shutdown();
    void update(float dt);

    void enterMenuMode();
    void enterGameMode();

    void pause();
    void resume();
    void skip();
    void previous();
    void stop();
    void reload();

    void setVolume(float vol);
    float volume() const;

    void setMuted(bool m);
    bool muted() const;

    bool isPlaying() const;
    bool isMenuMode() const;

    bool trackJustChanged() const;
    float timeSinceTrackChange() const;
    std::string currentTrackInfo() const;

    void drawNowPlayingPopup();
    void drawMusicWidget();
    void drawAllOverlay();

private:
    MusicManager() = default;
    ~MusicManager() = default;
    MusicManager(const MusicManager&) = delete;
    MusicManager& operator=(const MusicManager&) = delete;

    struct TrackEntry {
        std::string path;
        std::string filename;
    };

    void scanFolder(const std::string& dir, std::vector<TrackEntry>& out);
    void loadCredits(const std::string& path);
    std::string displayName(const std::string& filename) const;
    void startTrack(const std::string& path);
    void playNextIngame();
    void pickMenuTrack();
    void applyVolume();
    void uninitSound();

    std::vector<TrackEntry> mMenuTracks;
    std::vector<TrackEntry> mIngameTracks;
    std::vector<TrackEntry> mPlaylist;
    size_t mPlaylistIndex = 0;

    std::unordered_map<std::string, std::pair<std::string, std::string>> mCredits;

    ma_engine* mEngine = nullptr;
    ma_sound* mCurrentSound = nullptr;

    enum class Mode { None, Menu, Ingame };
    Mode mMode = Mode::None;

    float mVolume = 1.0f;
    bool mMuted = false;

    std::string mCurrentPath;
    std::string mCurrentFilename;
    std::string mCurrentArtist;
    std::string mCurrentTitle;

    bool mTrackJustChanged = false;
    float mTimeSinceTrackChange = 0.0f;
    float mPopupAlpha = 0.0f;
    float mPopupSlide = 0.0f;

    std::mt19937 mRng;

    bool mInitialized = false;
};
