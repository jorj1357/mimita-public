#include "music-manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <functional>
#include <iterator>

#include <nlohmann/json.hpp>

#include "miniaudio.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "replay/replay-export.h"
#include "perf/perf-spike.h"

using json = nlohmann::json;

namespace {

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

} // namespace

MusicManager& MusicManager::instance()
{
    static MusicManager mgr;
    return mgr;
}

MusicManager::~MusicManager()
{
    stopMusicPredecode();
}

static bool hasMusicExt(const std::string& name)
{
    std::string ext;
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) {
        for (char c : name.substr(dot + 1))
            ext.push_back((char)std::tolower((unsigned char)c));
    }
    return ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "oga" ||
        ext == "opus" || ext == "flac" || ext == "aac" || ext == "m4a" ||
        ext == "aif" || ext == "aiff" || ext == "caf" || ext == "wma" ||
        ext == "mp2" || ext == "ac3" || ext == "eac3" || ext == "mka" ||
        ext == "mp4" || ext == "ape";
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
        [](const TrackEntry& a, const TrackEntry& b) {
            return lowercase(a.filename) < lowercase(b.filename);
        });
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
        j = nlohmann::json::parse(file, nullptr, true, true);
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

namespace {

std::string lowerExtension(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    for (char& c : ext)
        c = (char)std::tolower((unsigned char)c);
    return ext;
}

bool requiresFfmpegDecode(const std::string& path)
{
    const std::string ext = lowerExtension(path);
    // miniaudio owns these common in-process formats. Other accepted formats
    // are normalized to WAV by the FFmpeg worker before the mixer opens them.
    return ext != ".mp3" && ext != ".wav" && ext != ".flac";
}

std::string musicCachePath(const std::string& path)
{
    std::error_code ec;
    const auto sourceTime = std::filesystem::last_write_time(path, ec);
    if (ec)
        return {};
    const auto sourceSize = std::filesystem::file_size(path, ec);
    if (ec)
        return {};
    const auto cacheDir = std::filesystem::temp_directory_path(ec) / "mimita-music-cache";
    if (ec)
        return {};
    std::filesystem::create_directories(cacheDir, ec);
    if (ec)
        return {};
    const auto cacheKey = std::to_string(std::hash<std::string>{}(path)) + "_" +
        std::to_string(sourceSize) + "_" +
        std::to_string(sourceTime.time_since_epoch().count());
    return (cacheDir / (cacheKey + ".wav")).string();
}

bool decodeMusicToCache(const std::string& path, std::string& error)
{
    const std::string output = musicCachePath(path);
    if (output.empty()) {
        error = "cannot create a music cache path";
        return false;
    }
    if (std::filesystem::exists(output))
        return true;
    const std::string ffmpeg = defaultFfmpegPath();
    if (ffmpeg.empty()) {
        error = "FFmpeg was not found";
        return false;
    }
    const std::string temporary = output + "." +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        ".partial.wav";
    std::error_code ec;
    const std::string command = "\"" + ffmpeg +
        "\" -hide_banner -loglevel error -y -i \"" + path +
        "\" -vn -ac 2 -ar 48000 -c:a pcm_s16le -f wav \"" + temporary + "\"";
    const int result = std::system(command.c_str());
    if (result != 0 || !std::filesystem::exists(temporary) ||
        std::filesystem::file_size(temporary, ec) <= 44 || ec) {
        std::filesystem::remove(temporary, ec);
        error = "FFmpeg decode failed (exit=" + std::to_string(result) + ")";
        return false;
    }
    std::filesystem::rename(temporary, output, ec);
    if (ec) {
        if (std::filesystem::exists(output)) {
            std::filesystem::remove(temporary, ec);
            return true;
        }
        std::filesystem::remove(temporary, ec);
        error = "could not publish the decoded WAV cache";
        return false;
    }
    return true;
}

} // namespace

std::string MusicManager::resolvePlaybackPath(const std::string& path)
{
    if (!requiresFfmpegDecode(path))
        return path;

    // Never spawn FFmpeg or block on a missing decoded cache from the gameplay
    // thread. The predecode worker installs the WAV cache in the background.
    const std::string output = musicCachePath(path);
    if (output.empty() || !std::filesystem::exists(output))
        return {};
    return output;
}

void MusicManager::startMusicPredecode()
{
    if (mMusicPredecodeThread.joinable())
        return;

    std::vector<std::string> paths;
    auto collect = [&](const std::vector<TrackEntry>& entries) {
        for (const TrackEntry& entry : entries) {
            if (requiresFfmpegDecode(entry.path))
                paths.push_back(entry.path);
        }
    };
    collect(mMenuTracks);
    collect(mIngameTracks);
    if (paths.empty())
        return;

    mMusicPredecodeStop.store(false, std::memory_order_release);
    mMusicPredecodeThread = std::thread([this, paths = std::move(paths)]() {
        for (const std::string& path : paths) {
            if (mMusicPredecodeStop.load(std::memory_order_acquire))
                return;
            std::string error;
            if (!decodeMusicToCache(path, error)) {
                logMusicEvent("music.decode.failed", path, error);
                Debug::log(Debug::Category::Audio,
                    "[MUSIC] decode failed: %s (%s)\n", path.c_str(), error.c_str());
            }
        }
    });
}

void MusicManager::stopMusicPredecode()
{
    mMusicPredecodeStop.store(true, std::memory_order_release);
    if (mMusicPredecodeThread.joinable())
        mMusicPredecodeThread.join();
}

void MusicManager::logMusicEvent(const std::string& eventName,
                                 const std::string& path,
                                 const std::string& reason) const
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Audio, StructuredLevel::Important,
        eventName, path, reason, 0,
        {{"track_path", path}},
        __FILE__, __LINE__, __func__);
}

void MusicManager::rememberTrack(const std::string& path)
{
    if (mHasTrackHistoryIndex && mTrackHistoryIndex + 1 < mTrackHistory.size())
        mTrackHistory.erase(mTrackHistory.begin() + (mTrackHistoryIndex + 1), mTrackHistory.end());

    mTrackHistory.push_back(path);
    constexpr size_t MAX_TRACK_HISTORY = 256;
    if (mTrackHistory.size() > MAX_TRACK_HISTORY) {
        mTrackHistory.erase(mTrackHistory.begin());
        if (mHasTrackHistoryIndex && mTrackHistoryIndex > 0)
            --mTrackHistoryIndex;
    }
    mTrackHistoryIndex = mTrackHistory.size() - 1;
    mHasTrackHistoryIndex = true;
}

bool MusicManager::playHistoryTrack(size_t index)
{
    if (index >= mTrackHistory.size() || !startTrack(mTrackHistory[index], false))
        return false;
    if (mModeTrackMode && !mPersonalMusicOverride) {
        mPersonalMusicOverride = true;
        saveConfig();
    }
    mTrackHistoryIndex = index;
    mHasTrackHistoryIndex = true;
    return true;
}

bool MusicManager::startTrack(const std::string& path, bool addToHistory)
{
    MIMITA_PERF_SCOPE("Audio::MusicStartTrack");
    uninitSound();

    if (!mEngine) return false;
    if (!std::filesystem::exists(path)) return false;

    const std::string playbackPath = resolvePlaybackPath(path);
    if (playbackPath.empty()) return false;

    mCurrentSound = new ma_sound();
    ma_result result = ma_sound_init_from_file(mEngine, playbackPath.c_str(),
        MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, mCurrentSound);
    if (result != MA_SUCCESS) {
        if (mLoggedLoadFailures.insert(path).second) {
            Debug::log(Debug::Category::Audio, "[MUSIC] failed to load: %s\n", path.c_str());
            logMusicEvent("music.track.load_failed", path,
                          "miniaudio rejected the source or decoded WAV");
        }
        delete mCurrentSound;
        mCurrentSound = nullptr;
        return false;
    }

    mCurrentPath = path;
    mNoPlayableTracksReported = false;
    if (addToHistory)
        rememberTrack(path);
    auto playlistEntry = std::find_if(mPlaylist.begin(), mPlaylist.end(),
        [&](const TrackEntry& entry) { return entry.path == path; });
    if (playlistEntry != mPlaylist.end())
        mPlaylistIndex = (size_t)std::distance(mPlaylist.begin(), playlistEntry);
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
    logMusicEvent("music.track.started", path,
                  requiresFfmpegDecode(path) ? "decoded cached WAV accepted by miniaudio"
                                             : "source accepted by miniaudio");
    return true;
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

    for (size_t attempt = 0; attempt < mPlaylist.size(); ++attempt) {
        if (mPlaylistIndex + 1 >= mPlaylist.size()) {
            if (mPlaylistMode == PlaylistMode::Shuffle && mPlaylist.size() > 1) {
                const std::string lastTrack = mCurrentPath;
                std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
                if (mPlaylist.front().path == lastTrack) {
                    auto different = std::find_if(mPlaylist.begin() + 1, mPlaylist.end(),
                        [&](const TrackEntry& entry) { return entry.path != lastTrack; });
                    if (different != mPlaylist.end())
                        std::iter_swap(mPlaylist.begin(), different);
                }
                Debug::log(Debug::Category::Audio,
                    "[MUSIC] shuffled playlist bag (%zu tracks)\n", mPlaylist.size());
            }
            mPlaylistIndex = 0;
        } else {
            ++mPlaylistIndex;
        }

        if (startTrack(mPlaylist[mPlaylistIndex].path))
            return;
        Debug::log(Debug::Category::Audio, "[MUSIC] skipping unusable track: %s\n",
            mPlaylist[mPlaylistIndex].path.c_str());
    }

    if (!mNoPlayableTracksReported) {
        mNoPlayableTracksReported = true;
        Debug::log(Debug::Category::Audio, "[MUSIC] no playable tracks remain\n");
        logMusicEvent("music.playlist.empty", {}, "all tracks in the active playlist failed to load");
    }
}

void MusicManager::rebuildIngamePlaylist(bool preserveCurrent)
{
    mPlaylist = mIngameTracks;
    if (mPlaylistMode == PlaylistMode::Shuffle)
        std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
    mPlaylistIndex = 0;

    if (preserveCurrent) {
        auto current = std::find_if(mPlaylist.begin(), mPlaylist.end(),
            [&](const TrackEntry& entry) { return entry.path == mCurrentPath; });
        if (current != mPlaylist.end())
            mPlaylistIndex = (size_t)std::distance(mPlaylist.begin(), current);
    }
}

void MusicManager::startIngamePlaylist()
{
    rebuildIngamePlaylist(false);
    if (mPlaylist.empty()) {
        uninitSound();
        mNoPlayableTracksReported = true;
        return;
    }
    if (!startTrack(mPlaylist[mPlaylistIndex].path))
        playNextIngame();
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

    scanFolder("assets/music/mainmenu", mMenuTracks);
    scanFolder("assets/music/ingame", mIngameTracks);
    loadCredits("assets/music/credits.json");

    Debug::log(Debug::Category::Audio, "[MUSIC] loaded tracks=%zu menu=%zu ingame=%zu\n",
        mMenuTracks.size() + mIngameTracks.size(), mMenuTracks.size(), mIngameTracks.size());

    loadConfig();
    startMusicPredecode();

    // Force-unmute on first boot so theme music plays immediately.
    // After this, the user's manual mute/unmute is always respected.
    if (!mMenuTracks.empty() && mMuted && !mFirstBootComplete) {
        mFirstBootComplete = true;
        setMuted(false);
    }

    mInitialized = true;
    enterMenuMode();
}

void MusicManager::shutdown()
{
    stopMusicPredecode();
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
    MIMITA_PERF_SCOPE("Audio::MusicUpdate");
    if (!mInitialized) {
        init();
    }
    // Hot reload: check if config file changed on disk
    {
        MIMITA_PERF_SCOPE("Audio::MusicConfigPoll");
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(mConfigPath, ec);
        if (!ec && wt != mConfigLastWrite) {
            Debug::log(Debug::Category::Audio, "[MUSIC] config changed on disk, reloading\n");
            loadConfig();
        }
    }
    mWidgetDt = dt;

    // If a track needs background FFmpeg decoding and its cache is not ready, retry
    // later instead of running FFmpeg on the gameplay thread or staying silent.
    if (!mCurrentSound) {
        mTrackRetryTimer -= dt;
        if (mTrackRetryTimer <= 0.0f) {
            mTrackRetryTimer = 1.0f;
            if (mModeTrackMode && !mPersonalMusicOverride && !mModeTrackPath.empty())
                startTrack(mModeTrackPath);
            else if (mMode == Mode::Ingame && (!mModeTrackMode || mPersonalMusicOverride) && !mPlaylist.empty())
                playNextIngame();
            else if (mMode == Mode::Menu && !mMenuTracks.empty())
                pickMenuTrack();
        }
    } else {
        mTrackRetryTimer = 1.0f;
    }

    if (mCurrentSound && ma_sound_at_end(mCurrentSound)) {
        if (mModeTrackMode && !mPersonalMusicOverride) {
            mTrackEnded = true;
        } else if (mMode == Mode::Ingame) {
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
    mModeTrackMode = false;
    mModeTrackPath.clear();
    mTrackEnded = false;
    mTrackHistory.clear();
    mHasTrackHistoryIndex = false;

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
    mModeTrackMode = false;
    mModeTrackPath.clear();
    mTrackEnded = false;
    mTrackHistory.clear();
    mHasTrackHistoryIndex = false;
    uninitSound();

    startIngamePlaylist();

    Debug::log(Debug::Category::Audio, "[MUSIC] mode=ingame playlist=%zu\n", mPlaylist.size());
}

bool MusicManager::playModeTrack(const std::string& path)
{
    if (!mInitialized) return false;
    if (path.empty()) {
        stopModeTrack();
        return false;
    }
    mMode = Mode::Ingame;
    mModeTrackMode = true;
    mModeTrackPath = path;
    mTrackEnded = false;
    if (mPersonalMusicOverride)
        return true;
    if (!startTrack(path)) {
        Debug::log(Debug::Category::Audio,
            "[MUSIC] mode track unavailable: %s\n", path.c_str());
        logMusicEvent("music.mode_track.unavailable", path,
                      "mode-selected soundtrack could not be loaded");
        return false;
    }
    return true;
}

void MusicManager::stopModeTrack()
{
    mTrackEnded = false;
    mModeTrackMode = false;
    mModeTrackPath.clear();
    if (!mPersonalMusicOverride) {
        uninitSound();
        mMode = Mode::None;
    }
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
    if (mHasTrackHistoryIndex && mTrackHistoryIndex + 1 < mTrackHistory.size()) {
        playHistoryTrack(mTrackHistoryIndex + 1);
    } else if (mModeTrackMode && !mPersonalMusicOverride) {
        // The mode owner selects the next shared song after this client reports
        // the current one ended; do not replace it with the local folder list.
        mTrackEnded = true;
    } else if (mMode == Mode::Ingame && !mPlaylist.empty()) {
        playNextIngame();
    } else if (mMode == Mode::Menu && !mMenuTracks.empty()) {
        pickMenuTrack();
    }
}

void MusicManager::previous()
{
    if (!mInitialized || !mHasTrackHistoryIndex || mTrackHistoryIndex == 0)
        return;
    if (mModeTrackMode)
        mPersonalMusicOverride = true;
    playHistoryTrack(mTrackHistoryIndex - 1);
    saveConfig();
}

void MusicManager::setPlaylistMode(PlaylistMode mode)
{
    if (mPlaylistMode == mode)
        return;
    mPlaylistMode = mode;
    if (mMode == Mode::Ingame && (!mModeTrackMode || mPersonalMusicOverride))
        rebuildIngamePlaylist(true);
    saveConfig();
    const char* value = mode == PlaylistMode::Shuffle ? "shuffle" : "linear";
    logMusicEvent("music.playlist.mode_changed", mCurrentPath, value);
}

void MusicManager::setPersonalMusicOverride(bool enabled)
{
    if (mPersonalMusicOverride == enabled)
        return;
    if (enabled && mModeTrackMode && mIngameTracks.empty()) {
        logMusicEvent("music.personal_override.unavailable", {},
                      "the local in-game playlist contains no tracks");
        return;
    }
    mPersonalMusicOverride = enabled;
    saveConfig();

    if (enabled) {
        if (mMode == Mode::Ingame && !mIngameTracks.empty())
            startIngamePlaylist();
    } else if (mModeTrackMode && !mModeTrackPath.empty()) {
        startTrack(mModeTrackPath);
    } else if (mMode != Mode::Ingame) {
        uninitSound();
    }
    logMusicEvent("music.personal_override.changed", mCurrentPath,
                  enabled ? "personal playlist selected locally" : "mode/default music selected");
}

void MusicManager::stop()
{
    uninitSound();
    mMode = Mode::None;
    mModeTrackMode = false;
    mModeTrackPath.clear();
    mTrackEnded = false;
}

void MusicManager::reload()
{
    const std::string previousTrack = mCurrentPath;
    stopMusicPredecode();
    scanFolder("assets/music/mainmenu", mMenuTracks);
    scanFolder("assets/music/ingame", mIngameTracks);
    loadCredits("assets/music/credits.json");
    mLoggedLoadFailures.clear();
    startMusicPredecode();

    if (mMode == Mode::Ingame && (!mModeTrackMode || mPersonalMusicOverride)) {
        rebuildIngamePlaylist(false);
        if (mPlaylist.empty()) {
            uninitSound();
        } else {
            auto current = std::find_if(mPlaylist.begin(), mPlaylist.end(),
                [&](const TrackEntry& entry) { return entry.path == previousTrack; });
            if (current != mPlaylist.end()) {
                mPlaylistIndex = (size_t)std::distance(mPlaylist.begin(), current);
            } else {
                startIngamePlaylist();
            }
        }
    }

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

void MusicManager::loadConfig()
{
    const PlaylistMode previousPlaylistMode = mPlaylistMode;
    const bool previousPersonalMusic = mPersonalMusicOverride;
    std::ifstream file(mConfigPath);
    if (!file.is_open()) {
        // First launch: save defaults, then load is skipped
        saveConfig();
        return;
    }
    try {
        json j;
        j = nlohmann::json::parse(file, nullptr, true, true);
        if (j.contains("musicEnabled")) {
            bool enabled = j["musicEnabled"];
            mMuted = !enabled;
        }
        if (j.contains("musicVolume")) {
            float vol = j["musicVolume"];
            mVolume = std::clamp(vol, 0.0f, 1.0f);
        }
        if (j.contains("playbackSpeed")) {
            float speed = j["playbackSpeed"];
            mPlaybackSpeed = std::clamp(speed, 0.25f, 2.0f);
        }
        if (j.contains("playlistMode")) {
            const std::string mode = lowercase(j["playlistMode"].get<std::string>());
            if (mode == "linear")
                mPlaylistMode = PlaylistMode::Linear;
            else if (mode == "shuffle")
                mPlaylistMode = PlaylistMode::Shuffle;
            else
                Debug::log(Debug::Category::Audio,
                    "[MUSIC] invalid playlistMode=%s; retaining current value\n", mode.c_str());
        }
        if (j.contains("personalMusic"))
            mPersonalMusicOverride = j["personalMusic"].get<bool>();
        if (j.contains("firstBootComplete"))
            mFirstBootComplete = j["firstBootComplete"];
        if (mMode == Mode::Ingame && previousPlaylistMode != mPlaylistMode &&
            (!mModeTrackMode || mPersonalMusicOverride))
            rebuildIngamePlaylist(true);
        if (mMode == Mode::Ingame && previousPersonalMusic != mPersonalMusicOverride) {
            if (mPersonalMusicOverride && mModeTrackMode && !mIngameTracks.empty())
                startIngamePlaylist();
            else if (!mPersonalMusicOverride && mModeTrackMode && !mModeTrackPath.empty())
                startTrack(mModeTrackPath);
        }
        applyVolume();
        applyPlaybackSpeed();
        std::error_code ec;
        mConfigLastWrite = std::filesystem::last_write_time(mConfigPath, ec);
        Debug::log(Debug::Category::Audio, "[MUSIC CONFIG] loaded path=%s musicMuted=%d volume=%.2f playbackSpeed=%.2f firstBoot=%d\n",
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
        j["playlistMode"] = mPlaylistMode == PlaylistMode::Shuffle ? "shuffle" : "linear";
        j["personalMusic"] = mPersonalMusicOverride;
        j["firstBootComplete"] = mFirstBootComplete;
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
