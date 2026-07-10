#include "replay-editor.h"
#include "replay-io.h"
#include "replay.h"
#include "terminal/terminal-state.h"
#include "audio/audio.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

ReplayEditor gReplayEditor;

// ── Easing helpers ──────────────────────────────────────────

float ReplayEditor::applyEasing(float t, KeyframeInterp interp) {
    switch (interp) {
        case KeyframeInterp::Linear:      return t;
        case KeyframeInterp::EaseIn:      return t * t;
        case KeyframeInterp::EaseOut:     return t * (2.0f - t);
        case KeyframeInterp::EaseInOut:   return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case KeyframeInterp::Smooth:      return t * t * (3.0f - 2.0f * t);
        case KeyframeInterp::Cut:         return 0.0f;
        case KeyframeInterp::Bezier:      return t * t * (3.0f - 2.0f * t);
        case KeyframeInterp::Exponential: return t * t * t * t;
        default: return t;
    }
}

float ReplayEditor::lerpTime(float a, float b, float t, KeyframeInterp interp) {
    if (interp == KeyframeInterp::Cut) return a;
    return a + (b - a) * applyEasing(t, interp);
}

// ── Loading ─────────────────────────────────────────────────

bool ReplayEditor::load(const std::string& replayPath) {
    unload();
    mReplayPath = replayPath;

    std::string base = replayPath;
    if (base.size() > 5 && base.rfind(".json") == base.size() - 5)
        base = base.substr(0, base.size() - 5);
    else if (base.size() > 11 && base.rfind(".mclip.json") == base.size() - 11)
        base = base.substr(0, base.size() - 11);
    mEditPath = base + ".rple.json";

    if (REPLAY_PLAYER.totalTicks() > 0) {
        mTotalTicks = (int)REPLAY_PLAYER.totalTicks();
        mTickRate = 60;
    } else {
        mTotalTicks = 0;
        mTickRate = 60;
    }

    mLoaded = true;
    mAutosaveTimer = 0.0;

    loadEdit();

    // Start music preview if audio track exists
    if (!mAudioTracks.empty() && mAudioTracks[0].enabled && !mAudioTracks[0].path.empty()) {
        Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Auto-starting preview: %s\n",
                   mAudioTracks[0].path.c_str());
        if (playReplayMusicPreview(mAudioTracks[0].path, mAudioTracks[0].volume)) {
            seekReplayMusicPreview(0.0f);
            // Pause preview until user starts playback
            pauseReplayMusicPreview();
        }
    }

    Debug::log(Debug::Category::Replay, "[RPLE] Loaded replay: %s (ticks=%d)\n",
               replayPath.c_str(), mTotalTicks);
    saveSession();
    return true;
}

void ReplayEditor::unload() {
    stopReplayMusicPreview();
    saveSession();
    mLoaded = false;
    mReplayPath.clear();
    mEditPath.clear();
    mTotalTicks = 0;
    mTickRate = 60;
    movieTick = 0.0f;
    playing = false;
    playbackSpeed = 1.0f;
    freecam = false;
    keyframePromptStage = 0;
    keyframePromptTick = 0;
    mCameraKeyframes.clear();
    mCameraModeKeyframes.clear();
    mTimeKeyframes.clear();
    mBookmarks.clear();
    mAudioTracks.clear();
    mAutosaves.clear();
    mAutosaveTimer = 0.0;
    mSelectedCamPos.clear();
    mSelectedCamMode.clear();
    mSelectedTime.clear();
}

double ReplayEditor::durationSec() const {
    return mTickRate > 0 ? (double)mTotalTicks / (double)mTickRate : 0.0;
}

// ── Camera keyframes ────────────────────────────────────────

void ReplayEditor::addCameraKeyframe(int tick, const glm::vec3& pos, const glm::quat& rot,
                                      float roll, float fov, KeyframeInterp interp) {
    for (auto& kf : mCameraKeyframes) {
        if (kf.tick == tick) {
            kf.position = pos;
            kf.rotation = rot;
            kf.roll = roll;
            kf.fov = fov;
            kf.interp = interp;
            Debug::log(Debug::Category::Replay, "[RPLE] Replaced camera keyframe at tick %d\n", tick);
            autosave();
            return;
        }
    }
    ReplayEditorCameraKeyframe kf;
    kf.tick = tick;
    kf.position = pos;
    kf.rotation = rot;
    kf.roll = roll;
    kf.fov = fov;
    kf.interp = interp;
    mCameraKeyframes.push_back(kf);
    std::sort(mCameraKeyframes.begin(), mCameraKeyframes.end(),
        [](const auto& a, const auto& b) { return a.tick < b.tick; });
    Debug::log(Debug::Category::Replay, "[RPLE] Camera keyframe at tick %d pos=(%.1f %.1f %.1f) roll=%.1f fov=%.0f\n",
               tick, pos.x, pos.y, pos.z, roll, fov);
    autosave();
}

bool ReplayEditor::deleteCameraKeyframe(int index) {
    if (index < 0 || index >= (int)mCameraKeyframes.size()) return false;
    mCameraKeyframes.erase(mCameraKeyframes.begin() + index);
    Debug::log(Debug::Category::Replay, "[RPLE] Deleted camera keyframe %d\n", index);
    autosave();
    return true;
}

void ReplayEditor::clearCameraKeyframes() {
    mCameraKeyframes.clear();
    Debug::log(Debug::Category::Replay, "[RPLE] Cleared all camera keyframes\n");
    autosave();
}

const ReplayEditorCameraKeyframe& ReplayEditor::cameraKeyframe(int index) const {
    static ReplayEditorCameraKeyframe sDummy;
    if (index < 0 || index >= (int)mCameraKeyframes.size()) return sDummy;
    return mCameraKeyframes[index];
}

void ReplayEditor::setCameraKeyframeInterp(int index, KeyframeInterp interp) {
    if (index >= 0 && index < (int)mCameraKeyframes.size())
        mCameraKeyframes[index].interp = interp;
}

int ReplayEditor::findNearestCameraKeyframe(int tick) const {
    if (mCameraKeyframes.empty()) return -1;
    int best = 0;
    int bestDist = std::abs(mCameraKeyframes[0].tick - tick);
    for (int i = 1; i < (int)mCameraKeyframes.size(); ++i) {
        int dist = std::abs(mCameraKeyframes[i].tick - tick);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

// ── Camera mode keyframes ───────────────────────────────────

void ReplayEditor::addCameraModeKeyframe(int tick, ReplayEditorCamMode mode) {
    for (auto& kf : mCameraModeKeyframes) {
        if (kf.tick == tick) {
            kf.mode = mode;
            Debug::log(Debug::Category::Replay, "[RPLE] Replaced camera mode keyframe at tick %d\n", tick);
            autosave();
            return;
        }
    }
    ReplayEditorCamModeKeyframe kf;
    kf.tick = tick;
    kf.mode = mode;
    mCameraModeKeyframes.push_back(kf);
    std::sort(mCameraModeKeyframes.begin(), mCameraModeKeyframes.end(),
        [](const auto& a, const auto& b) { return a.tick < b.tick; });
    Debug::log(Debug::Category::Replay, "[RPLE] Camera mode keyframe at tick %d mode=%s\n",
               tick, camModeName(mode));
    autosave();
}

bool ReplayEditor::deleteCameraModeKeyframe(int index) {
    if (index < 0 || index >= (int)mCameraModeKeyframes.size()) return false;
    mCameraModeKeyframes.erase(mCameraModeKeyframes.begin() + index);
    Debug::log(Debug::Category::Replay, "[RPLE] Deleted camera mode keyframe %d\n", index);
    autosave();
    return true;
}

void ReplayEditor::clearCameraModeKeyframes() {
    mCameraModeKeyframes.clear();
    Debug::log(Debug::Category::Replay, "[RPLE] Cleared all camera mode keyframes\n");
    autosave();
}

const ReplayEditorCamModeKeyframe& ReplayEditor::cameraModeKeyframe(int index) const {
    static ReplayEditorCamModeKeyframe sDummy;
    if (index < 0 || index >= (int)mCameraModeKeyframes.size()) return sDummy;
    return mCameraModeKeyframes[index];
}

ReplayEditorCamMode ReplayEditor::cameraModeAtTick(int tick) const {
    if (mCameraModeKeyframes.empty()) return ReplayEditorCamMode::ThirdPerson;
    int idx = -1;
    for (int i = 0; i < (int)mCameraModeKeyframes.size(); ++i) {
        if (mCameraModeKeyframes[i].tick <= tick) idx = i;
    }
    if (idx < 0) return ReplayEditorCamMode::ThirdPerson;
    return mCameraModeKeyframes[idx].mode;
}

// ── Time keyframes ──────────────────────────────────────────

void ReplayEditor::addTimeKeyframe(int tick, float speed, KeyframeInterp interp) {
    ReplayEditorTimeKeyframe kf;
    kf.tick = tick;
    kf.speed = std::max(0.01f, speed);
    kf.interp = interp;
    mTimeKeyframes.push_back(kf);
    std::sort(mTimeKeyframes.begin(), mTimeKeyframes.end(),
        [](const auto& a, const auto& b) { return a.tick < b.tick; });
    Debug::log(Debug::Category::Replay, "[RPLE] Time keyframe at tick %d = %.2fx\n", tick, speed);
    autosave();
}

bool ReplayEditor::deleteTimeKeyframe(int index) {
    if (index < 0 || index >= (int)mTimeKeyframes.size()) return false;
    mTimeKeyframes.erase(mTimeKeyframes.begin() + index);
    autosave();
    return true;
}

void ReplayEditor::clearTimeKeyframes() {
    mTimeKeyframes.clear();
    autosave();
}

const ReplayEditorTimeKeyframe& ReplayEditor::timeKeyframe(int index) const {
    static ReplayEditorTimeKeyframe sDummy;
    if (index < 0 || index >= (int)mTimeKeyframes.size()) return sDummy;
    return mTimeKeyframes[index];
}

float ReplayEditor::playbackSpeedAtTick(int tick) const {
    if (mTimeKeyframes.empty()) return 1.0f;
    int idx = -1;
    for (int i = 0; i < (int)mTimeKeyframes.size(); ++i) {
        if (mTimeKeyframes[i].tick <= tick) idx = i;
    }
    if (idx < 0) return 1.0f;
    if (idx == (int)mTimeKeyframes.size() - 1) return mTimeKeyframes[idx].speed;
    const auto& a = mTimeKeyframes[idx];
    const auto& b = mTimeKeyframes[idx + 1];
    if (b.tick <= a.tick) return a.speed;
    float t = (float)(tick - a.tick) / (float)(b.tick - a.tick);
    return lerpTime(a.speed, b.speed, t, a.interp);
}

// ── Bookmarks ───────────────────────────────────────────────

void ReplayEditor::addBookmark(int tick, const std::string& label) {
    ReplayEditorBookmark bm;
    bm.tick = tick;
    bm.label = label.empty() ? "Marker" : label;
    mBookmarks.push_back(bm);
    std::sort(mBookmarks.begin(), mBookmarks.end(),
        [](const auto& a, const auto& b) { return a.tick < b.tick; });
}

bool ReplayEditor::deleteBookmark(int index) {
    if (index < 0 || index >= (int)mBookmarks.size()) return false;
    mBookmarks.erase(mBookmarks.begin() + index);
    return true;
}

void ReplayEditor::clearBookmarks() {
    mBookmarks.clear();
}

const ReplayEditorBookmark& ReplayEditor::bookmark(int index) const {
    static ReplayEditorBookmark sDummy;
    if (index < 0 || index >= (int)mBookmarks.size()) return sDummy;
    return mBookmarks[index];
}

// ── Audio tracks ────────────────────────────────────────────

void ReplayEditor::setAudioTrack(const std::string& path, float volume)
{
    mAudioTracks.clear();
    ReplayEditorAudioTrack track;
    track.path = path;
    track.startTick = 0;
    track.startSeconds = 0.0f;
    track.volume = volume;
    track.enabled = true;
    mAudioTracks.push_back(track);
    Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Set audio track: %s (vol=%.2f)\n", path.c_str(), volume);
    autosave();
}

void ReplayEditor::clearAudioTracks()
{
    mAudioTracks.clear();
    Debug::log(Debug::Category::Replay, "[RPLE AUDIO] Cleared audio tracks\n");
    autosave();
}

const ReplayEditorAudioTrack& ReplayEditor::audioTrack(int index) const
{
    static ReplayEditorAudioTrack sDummy;
    if (index < 0 || index >= (int)mAudioTracks.size()) return sDummy;
    return mAudioTracks[index];
}

ReplayEditorAudioTrack* ReplayEditor::mutableAudioTrack(int index)
{
    if (index < 0 || index >= (int)mAudioTracks.size()) return nullptr;
    return &mAudioTracks[index];
}

// ── Keyframe navigation ─────────────────────────────────────

int ReplayEditor::nextKeyframeTick(int currentTick) const {
    int best = -1;
    for (const auto& kf : mCameraKeyframes) {
        if (kf.tick > currentTick) { best = kf.tick; break; }
    }
    if (best >= 0) return best;
    if (!mCameraKeyframes.empty()) return mCameraKeyframes.front().tick;
    return -1;
}

int ReplayEditor::prevKeyframeTick(int currentTick) const {
    int best = -1;
    for (const auto& kf : mCameraKeyframes) {
        if (kf.tick < currentTick) best = kf.tick;
        else break;
    }
    if (best >= 0) return best;
    if (!mCameraKeyframes.empty()) return mCameraKeyframes.back().tick;
    return -1;
}

// ── Persistence ─────────────────────────────────────────────

bool ReplayEditor::saveEdit() {
    nlohmann::json j;
    j["version"] = 2;
    j["replayPath"] = mReplayPath;
    j["totalTicks"] = mTotalTicks;
    j["tickRate"] = mTickRate;

    auto& camArr = j["cameraKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mCameraKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["position"] = vec3Json(kf.position);
        k["rotation"] = {kf.rotation.x, kf.rotation.y, kf.rotation.z, kf.rotation.w};
        k["roll"] = kf.roll;
        k["fov"] = kf.fov;
        k["interp"] = interpName(kf.interp);
        camArr.push_back(k);
    }

    auto& camModeArr = j["cameraModeKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mCameraModeKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["mode"] = camModeName(kf.mode);
        camModeArr.push_back(k);
    }

    auto& timeArr = j["timeKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mTimeKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["speed"] = kf.speed;
        k["interp"] = interpName(kf.interp);
        timeArr.push_back(k);
    }

    auto& bmArr = j["bookmarks"] = nlohmann::json::array();
    for (const auto& bm : mBookmarks) {
        nlohmann::json b;
        b["tick"] = bm.tick;
        b["label"] = bm.label;
        bmArr.push_back(b);
    }

    // Audio tracks
    auto& audioArr = j["audio_tracks"] = nlohmann::json::array();
    for (const auto& at : mAudioTracks) {
        nlohmann::json a;
        a["path"] = at.path;
        a["start_tick"] = at.startTick;
        a["start_seconds"] = at.startSeconds;
        a["volume"] = at.volume;
        a["enabled"] = at.enabled;
        audioArr.push_back(a);
    }

    j["freecam"] = freecam;
    j["freecamPos"] = vec3Json(freecamPos);
    j["freecamRot"] = {freecamRot.x, freecamRot.y, freecamRot.z, freecamRot.w};
    j["freecamRoll"] = freecamRoll;
    j["freecamFov"] = freecamFov;
    j["defaultInterp"] = interpName(defaultInterp);

    std::ofstream f(mEditPath);
    if (!f.is_open()) {
        Debug::log(Debug::Category::Replay, "[RPLE] Cannot save: %s\n", mEditPath.c_str());
        return false;
    }
    f << j.dump(2);
    f.close();
    Debug::log(Debug::Category::Replay, "[RPLE] Saved: %s\n", mEditPath.c_str());
    return true;
}

bool ReplayEditor::loadEdit() {
    if (!std::filesystem::exists(mEditPath)) return false;
    std::ifstream f(mEditPath);
    if (!f.is_open()) return false;
    nlohmann::json j;
    try { f >> j; } catch (...) { f.close(); return false; }
    f.close();

    mCameraKeyframes.clear();
    mCameraModeKeyframes.clear();
    mTimeKeyframes.clear();
    mBookmarks.clear();

    if (j.contains("totalTicks")) mTotalTicks = j["totalTicks"];
    if (j.contains("tickRate")) mTickRate = j["tickRate"];

    if (j.contains("cameraKeyframes")) {
        for (const auto& k : j["cameraKeyframes"]) {
            ReplayEditorCameraKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.position = jsonVec3(k["position"]);
            if (k.contains("rotation") && k["rotation"].is_array() && k["rotation"].size() == 4)
                kf.rotation = {k["rotation"][0], k["rotation"][1], k["rotation"][2], k["rotation"][3]};
            kf.roll = k.value("roll", 0.0f);
            kf.fov = k.value("fov", 70.0f);
            kf.interp = interpFromString(k.value("interp", "linear"));
            mCameraKeyframes.push_back(kf);
        }
    }

    if (j.contains("cameraModeKeyframes")) {
        for (const auto& k : j["cameraModeKeyframes"]) {
            ReplayEditorCamModeKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.mode = camModeFromString(k.value("mode", "thirdperson"));
            mCameraModeKeyframes.push_back(kf);
        }
    }

    if (j.contains("timeKeyframes")) {
        for (const auto& k : j["timeKeyframes"]) {
            ReplayEditorTimeKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.speed = k.value("speed", 1.0f);
            kf.interp = interpFromString(k.value("interp", "linear"));
            mTimeKeyframes.push_back(kf);
        }
    }

    if (j.contains("bookmarks")) {
        for (const auto& b : j["bookmarks"]) {
            ReplayEditorBookmark bm;
            bm.tick = b.value("tick", 0);
            bm.label = b.value("label", "Marker");
            mBookmarks.push_back(bm);
        }
    }

    // Audio tracks
    mAudioTracks.clear();
    if (j.contains("audio_tracks")) {
        for (const auto& a : j["audio_tracks"]) {
            ReplayEditorAudioTrack at;
            at.path = a.value("path", "");
            at.startTick = a.value("start_tick", 0);
            at.startSeconds = a.value("start_seconds", 0.0f);
            at.volume = a.value("volume", 1.0f);
            at.enabled = a.value("enabled", true);
            mAudioTracks.push_back(at);
        }
    }

    freecam = j.value("freecam", false);
    if (j.contains("freecamPos")) freecamPos = jsonVec3(j["freecamPos"]);
    if (j.contains("freecamRot") && j["freecamRot"].is_array() && j["freecamRot"].size() == 4)
        freecamRot = {j["freecamRot"][0], j["freecamRot"][1], j["freecamRot"][2], j["freecamRot"][3]};
    freecamRoll = j.value("freecamRoll", 0.0f);
    freecamFov = j.value("freecamFov", 70.0f);
    defaultInterp = interpFromString(j.value("defaultInterp", "linear"));

    Debug::log(Debug::Category::Replay, "[RPLE] Loaded edit: %d cam kf, %d cam-mode kf, %d time kf, %d marks, %d audio tracks\n",
               (int)mCameraKeyframes.size(), (int)mCameraModeKeyframes.size(),
               (int)mTimeKeyframes.size(), (int)mBookmarks.size(), (int)mAudioTracks.size());
    return true;
}

// ── Session persistence ─────────────────────────────────────

std::string ReplayEditor::sessionPath() {
    return "replays/replay_editor_session.json";
}

bool ReplayEditor::hasSession() {
    return std::filesystem::exists(sessionPath());
}

bool ReplayEditor::saveSession() {
    nlohmann::json j;
    j["lastReplay"] = mReplayPath;
    j["lastProject"] = mEditPath;
    j["tick"] = (int)movieTick;
    j["paused"] = !playing;
    j["freecam"] = freecam;
    if (freecam) {
        j["freecamPos"] = vec3Json(freecamPos);
        j["freecamRot"] = {freecamRot.x, freecamRot.y, freecamRot.z, freecamRot.w};
        j["freecamRoll"] = freecamRoll;
        j["freecamFov"] = freecamFov;
    }

    std::error_code ec;
    std::filesystem::create_directories("replays", ec);
    std::ofstream f(sessionPath());
    if (!f.is_open()) return false;
    f << j.dump(2);
    f.close();
    return true;
}

bool ReplayEditor::loadSession() {
    std::ifstream f(sessionPath());
    if (!f.is_open()) return false;
    nlohmann::json j;
    try { f >> j; } catch (...) { f.close(); return false; }
    f.close();

    if (j.contains("lastReplay"))
        mReplayPath = j["lastReplay"].get<std::string>();
    if (j.contains("lastProject"))
        mEditPath = j["lastProject"].get<std::string>();
    movieTick = (float)j.value("tick", 0);
    playing = !j.value("paused", true);
    freecam = j.value("freecam", false);
    if (freecam) {
        if (j.contains("freecamPos")) freecamPos = jsonVec3(j["freecamPos"]);
        if (j.contains("freecamRot") && j["freecamRot"].is_array() && j["freecamRot"].size() == 4)
            freecamRot = {j["freecamRot"][0], j["freecamRot"][1], j["freecamRot"][2], j["freecamRot"][3]};
        freecamRoll = j.value("freecamRoll", 0.0f);
        freecamFov = j.value("freecamFov", 70.0f);
    }
    return true;
}

// ── Autosave / Undo ─────────────────────────────────────────

void ReplayEditor::autosave() {
    if (!mLoaded || mEditPath.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories("replays/editor-autosaves", ec);

    char timeBuf[32];
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &local);

    std::string base = std::filesystem::path(mEditPath).stem().string();
    std::string asPath = "replays/editor-autosaves/" + base + "_" + timeBuf + ".autosave.json";

    nlohmann::json j;
    j["version"] = 2;
    j["replayPath"] = mReplayPath;
    j["totalTicks"] = mTotalTicks;
    j["tickRate"] = mTickRate;

    auto& camArr = j["cameraKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mCameraKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["position"] = vec3Json(kf.position);
        k["rotation"] = {kf.rotation.x, kf.rotation.y, kf.rotation.z, kf.rotation.w};
        k["roll"] = kf.roll;
        k["fov"] = kf.fov;
        k["interp"] = interpName(kf.interp);
        camArr.push_back(k);
    }

    auto& camModeArr = j["cameraModeKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mCameraModeKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["mode"] = camModeName(kf.mode);
        camModeArr.push_back(k);
    }

    auto& timeArr = j["timeKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mTimeKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["speed"] = kf.speed;
        k["interp"] = interpName(kf.interp);
        timeArr.push_back(k);
    }

    auto& bmArr = j["bookmarks"] = nlohmann::json::array();
    for (const auto& bm : mBookmarks) {
        nlohmann::json b;
        b["tick"] = bm.tick;
        b["label"] = bm.label;
        bmArr.push_back(b);
    }

    // Audio tracks
    auto& audioArr = j["audio_tracks"] = nlohmann::json::array();
    for (const auto& at : mAudioTracks) {
        nlohmann::json a;
        a["path"] = at.path;
        a["start_tick"] = at.startTick;
        a["start_seconds"] = at.startSeconds;
        a["volume"] = at.volume;
        a["enabled"] = at.enabled;
        audioArr.push_back(a);
    }

    j["freecam"] = freecam;
    j["freecamPos"] = vec3Json(freecamPos);
    j["freecamRot"] = {freecamRot.x, freecamRot.y, freecamRot.z, freecamRot.w};
    j["freecamRoll"] = freecamRoll;
    j["freecamFov"] = freecamFov;
    j["defaultInterp"] = interpName(defaultInterp);

    std::ofstream f(asPath);
    if (!f.is_open()) {
        Debug::log(Debug::Category::Replay, "[RPLE] Autosave failed: %s\n", asPath.c_str());
        return;
    }
    f << j.dump(2);
    f.close();

    ReplayEditorAutosave entry;
    entry.path = asPath;
    entry.timestamp = (double)now;
    mAutosaves.push_back(entry);

    while ((int)mAutosaves.size() > MAX_AUTOSAVES) {
        std::filesystem::remove(mAutosaves.front().path, ec);
        mAutosaves.erase(mAutosaves.begin());
    }

    Debug::log(Debug::Category::Replay, "[RPLE] Autosave: %s (total=%zu)\n",
               asPath.c_str(), mAutosaves.size());
    saveSession();
}

bool ReplayEditor::undoLastAutosave() {
    if (mAutosaves.empty()) {
        Debug::log(Debug::Category::Replay, "[RPLE] Undo: no autosaves available\n");
        return false;
    }

    const ReplayEditorAutosave& entry = mAutosaves.back();
    if (!std::filesystem::exists(entry.path)) {
        mAutosaves.pop_back();
        return undoLastAutosave();
    }

    std::ifstream f(entry.path);
    if (!f.is_open()) {
        mAutosaves.pop_back();
        return undoLastAutosave();
    }
    nlohmann::json j;
    try { f >> j; } catch (...) { f.close(); mAutosaves.pop_back(); return undoLastAutosave(); }
    f.close();

    mCameraKeyframes.clear();
    mCameraModeKeyframes.clear();
    mTimeKeyframes.clear();
    mBookmarks.clear();

    if (j.contains("totalTicks")) mTotalTicks = j["totalTicks"];
    if (j.contains("tickRate")) mTickRate = j["tickRate"];

    if (j.contains("cameraKeyframes")) {
        for (const auto& k : j["cameraKeyframes"]) {
            ReplayEditorCameraKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.position = jsonVec3(k["position"]);
            if (k.contains("rotation") && k["rotation"].is_array() && k["rotation"].size() == 4)
                kf.rotation = {k["rotation"][0], k["rotation"][1], k["rotation"][2], k["rotation"][3]};
            kf.roll = k.value("roll", 0.0f);
            kf.fov = k.value("fov", 70.0f);
            kf.interp = interpFromString(k.value("interp", "linear"));
            mCameraKeyframes.push_back(kf);
        }
    }

    if (j.contains("cameraModeKeyframes")) {
        for (const auto& k : j["cameraModeKeyframes"]) {
            ReplayEditorCamModeKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.mode = camModeFromString(k.value("mode", "thirdperson"));
            mCameraModeKeyframes.push_back(kf);
        }
    }

    if (j.contains("timeKeyframes")) {
        for (const auto& k : j["timeKeyframes"]) {
            ReplayEditorTimeKeyframe kf;
            kf.tick = k.value("tick", 0);
            kf.speed = k.value("speed", 1.0f);
            kf.interp = interpFromString(k.value("interp", "linear"));
            mTimeKeyframes.push_back(kf);
        }
    }

    if (j.contains("bookmarks")) {
        for (const auto& b : j["bookmarks"]) {
            ReplayEditorBookmark bm;
            bm.tick = b.value("tick", 0);
            bm.label = b.value("label", "Marker");
            mBookmarks.push_back(bm);
        }
    }

    freecam = j.value("freecam", false);
    if (j.contains("freecamPos")) freecamPos = jsonVec3(j["freecamPos"]);
    if (j.contains("freecamRot") && j["freecamRot"].is_array() && j["freecamRot"].size() == 4)
        freecamRot = {j["freecamRot"][0], j["freecamRot"][1], j["freecamRot"][2], j["freecamRot"][3]};
    freecamRoll = j.value("freecamRoll", 0.0f);
    freecamFov = j.value("freecamFov", 70.0f);
    defaultInterp = interpFromString(j.value("defaultInterp", "linear"));

    std::error_code ec;
    std::filesystem::remove(entry.path, ec);
    mAutosaves.pop_back();

    Debug::log(Debug::Category::Replay, "[RPLE] Undo: restored from autosave (%zu remaining)\n",
               mAutosaves.size());
    return true;
}

void ReplayEditor::clearAutosaves() {
    std::error_code ec;
    for (const auto& as : mAutosaves)
        std::filesystem::remove(as.path, ec);
    mAutosaves.clear();
    Debug::log(Debug::Category::Replay, "[RPLE] Cleared %zu autosaves\n", mAutosaves.size());
}

// ── Keyframe selection ──────────────────────────────────────

void ReplayEditor::selectAll(KeyframeFilter filter) {
    mSelectedCamPos.clear();
    mSelectedCamMode.clear();
    mSelectedTime.clear();
    switch (filter) {
        case KeyframeFilter::Campos:
            for (int i = 0; i < (int)mCameraKeyframes.size(); ++i)
                mSelectedCamPos.push_back(i);
            break;
        case KeyframeFilter::Cammode:
            for (int i = 0; i < (int)mCameraModeKeyframes.size(); ++i)
                mSelectedCamMode.push_back(i);
            break;
        case KeyframeFilter::Pbspeed:
            for (int i = 0; i < (int)mTimeKeyframes.size(); ++i)
                mSelectedTime.push_back(i);
            break;
        case KeyframeFilter::All:
            for (int i = 0; i < (int)mCameraKeyframes.size(); ++i)
                mSelectedCamPos.push_back(i);
            for (int i = 0; i < (int)mCameraModeKeyframes.size(); ++i)
                mSelectedCamMode.push_back(i);
            for (int i = 0; i < (int)mTimeKeyframes.size(); ++i)
                mSelectedTime.push_back(i);
            break;
    }
}

void ReplayEditor::clearSelection() {
    mSelectedCamPos.clear();
    mSelectedCamMode.clear();
    mSelectedTime.clear();
}

bool ReplayEditor::setInterpOnSelected(KeyframeInterp interp) {
    int count = 0;
    for (int idx : mSelectedCamPos) {
        if (idx >= 0 && idx < (int)mCameraKeyframes.size()) {
            mCameraKeyframes[idx].interp = interp;
            count++;
        }
    }
    for (int idx : mSelectedCamMode) {
        (void)idx;
    }
    for (int idx : mSelectedTime) {
        if (idx >= 0 && idx < (int)mTimeKeyframes.size()) {
            mTimeKeyframes[idx].interp = interp;
            count++;
        }
    }
    return count > 0;
}

// ── Update ──────────────────────────────────────────────────

void ReplayEditor::update(float dt) {
    if (!mLoaded) return;

    if (playing) {
        float speed = playbackSpeedAtTick((int)movieTick);
        movieTick += dt * (float)mTickRate * speed;
        if (movieTick >= mTotalTicks && mTotalTicks > 0) {
            movieTick = (float)mTotalTicks;
            // Don't set playing=false — keep editor active so Space toggle,
            // timeline, keyframes, and camera remain interactive.
            // User scrubs backward to resume playback.
        }
    }

    // Sync music preview with play/pause state
    if (!mAudioTracks.empty() && mAudioTracks[0].enabled) {
        if (playing) {
            if (!isReplayMusicPreviewPlaying())
                resumeReplayMusicPreview();
        } else {
            if (isReplayMusicPreviewPlaying())
                pauseReplayMusicPreview();
        }
    }

    mAutosaveTimer += dt;
    if (mAutosaveTimer >= AUTOSAVE_INTERVAL) {
        mAutosaveTimer = 0.0;
        if (mLoaded) autosave();
    }
}

void ReplayEditor::seekToTick(int tick) {
    movieTick = (float)std::clamp(tick, 0, mTotalTicks);
    // Sync music preview
    if (!mAudioTracks.empty() && mAudioTracks[0].enabled) {
        float sec = (float)tick / (float)mTickRate;
        seekReplayMusicPreview(sec);
        if (!playing && isReplayMusicPreviewPlaying())
            pauseReplayMusicPreview();
        Debug::log(Debug::Category::Replay, "[RPLE AUDIO] seek sync: tick=%d sec=%.2f\n", tick, sec);
    }
    saveSession();
}
