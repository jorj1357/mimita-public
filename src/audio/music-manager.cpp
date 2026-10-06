#include "music-manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <functional>

#include <nlohmann/json.hpp>

#include "miniaudio.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"

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
    return ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "opus";
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

std::string MusicManager::resolvePlaybackPath(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    for (char& c : ext)
        c = (char)std::tolower((unsigned char)c);
    if (ext != ".opus")
        return path;

    auto cached = mOpusCache.find(path);
    if (cached != mOpusCache.end() && std::filesystem::exists(cached->second))
        return cached->second;

    const std::string ffmpeg = defaultFfmpegPath();
    if (ffmpeg.empty()) {
        Debug::log(Debug::Category::Audio,
            "[MUSIC] cannot play Opus; FFmpeg was not found: %s\n", path.c_str());
        return {};
    }

    std::error_code ec;
    const auto cacheDir = std::filesystem::temp_directory_path(ec) / "mimita-music-opus";
    if (ec) {
        Debug::log(Debug::Category::Audio,
            "[MUSIC] cannot resolve Opus cache directory: %s\n", path.c_str());
        return {};
    }
    std::filesystem::create_directories(cacheDir, ec);
    if (ec) {
        Debug::log(Debug::Category::Audio,
            "[MUSIC] cannot create Opus cache directory: %s\n", path.c_str());
        return {};
    }

    const auto sourceTime = std::filesystem::last_write_time(path, ec);
    const auto sourceSize = std::filesystem::file_size(path, ec);
    const auto cacheKey = std::to_string(std::hash<std::string>{}(path)) + "_" +
        std::to_string(sourceSize) + "_" + std::to_string(sourceTime.time_since_epoch().count());
    const auto output = cacheDir / (cacheKey + ".wav");
    if (!std::filesystem::exists(output)) {
        const std::string command = "\"" + ffmpeg +
            "\" -hide_banner -loglevel error -y -i \"" + path +
            "\" -vn -ac 2 -ar 48000 -c:a pcm_s16le \"" + output.string() + "\"";
        if (std::system(command.c_str()) != 0 || !std::filesystem::exists(output)) {
            Debug::log(Debug::Category::Audio,
                "[MUSIC] failed to decode Opus with FFmpeg: %s\n", path.c_str());
            return {};
        }
    }

    mOpusCache[path] = output.string();
    Debug::log(Debug::Category::Audio, "[MUSIC] decoded Opus cache: %s\n", path.c_str());
    return output.string();
}

bool MusicManager::startTrack(const std::string& path)
{
    uninitSound();

    if (!mEngine) return false;
    if (!std::filesystem::exists(path)) return false;

    const std::string playbackPath = resolvePlaybackPath(path);
    if (playbackPath.empty()) return false;

    mCurrentSound = new ma_sound();
    ma_result result = ma_sound_init_from_file(mEngine, playbackPath.c_str(),
        MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, mCurrentSound);
    if (result != MA_SUCCESS) {
        Debug::log(Debug::Category::Audio, "[MUSIC] failed to load: %s\n", path.c_str());
        delete mCurrentSound;
        mCurrentSound = nullptr;
        return false;
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
        mPlaylistIndex++;
        if (mPlaylistIndex >= mPlaylist.size()) {
            std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
            mPlaylistIndex = 0;
            Debug::log(Debug::Category::Audio, "[MUSIC] playlist reshuffled (%zu tracks)\n", mPlaylist.size());
        }

        if (startTrack(mPlaylist[mPlaylistIndex].path))
            return;
        Debug::log(Debug::Category::Audio, "[MUSIC] skipping unusable track: %s\n",
            mPlaylist[mPlaylistIndex].path.c_str());
    }

    Debug::log(Debug::Category::Audio, "[MUSIC] no playable tracks remain\n");
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
    if (!mInitialized) {
        init();
    }
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

    if (!startTrack(mPlaylist[mPlaylistIndex].path))
        playNextIngame();

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
    scanFolder("assets/music/mainmenu", mMenuTracks);
    scanFolder("assets/music/ingame", mIngameTracks);
    loadCredits("assets/music/credits.json");

    if (mMode == Mode::Ingame) {
        mPlaylist = mIngameTracks;
        if (mPlaylist.empty()) {
            uninitSound();
        } else {
            std::shuffle(mPlaylist.begin(), mPlaylist.end(), mRng);
            mPlaylistIndex = mPlaylist.size() - 1;
            playNextIngame();
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
        if (j.contains("firstBootComplete"))
            mFirstBootComplete = j["firstBootComplete"];
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
