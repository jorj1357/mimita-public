#include "replay-editor.h"
#include "replay-io.h"
#include "replay.h"
#include "terminal/terminal-state.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

ReplayEditor gReplayEditor;

// ── Easing helpers ──────────────────────────────────────────

float ReplayEditor::applyEasing(float t, KeyframeInterp interp) {
    switch (interp) {
        case KeyframeInterp::Linear:    return t;
        case KeyframeInterp::EaseIn:    return t * t;
        case KeyframeInterp::EaseOut:   return t * (2.0f - t);
        case KeyframeInterp::EaseInOut: return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case KeyframeInterp::Smooth:    return t * t * (3.0f - 2.0f * t);
        case KeyframeInterp::Cut:       return 0.0f;  // hold at start until cut
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

    // Build .rpledit path: strip .json, add .rpledit
    std::string base = replayPath;
    if (base.size() > 5 && base.rfind(".json") == base.size() - 5)
        base = base.substr(0, base.size() - 5);
    else if (base.size() > 11 && base.rfind(".mclip.json") == base.size() - 11)
        base = base.substr(0, base.size() - 11);
    mEditPath = base + ".rpledit";

    // Load replay header to get tick count from the REPLAY_PLAYER if available
    if (REPLAY_PLAYER.totalTicks() > 0) {
        mTotalTicks = (int)REPLAY_PLAYER.totalTicks();
        mTickRate = 60;
    } else {
        // Fallback: quick scan from file
        mTotalTicks = 0;
        mTickRate = 60;
    }

    mLoaded = true;

    // Try loading existing .rpledit
    loadEdit();

    Debug::log(Debug::Category::Replay, "[RPLE] Loaded replay: %s (ticks=%d)\n",
               replayPath.c_str(), mTotalTicks);
    return true;
}

void ReplayEditor::unload() {
    mLoaded = false;
    mReplayPath.clear();
    mEditPath.clear();
    mTotalTicks = 0;
    mTickRate = 60;
    movieTick = 0.0f;
    playing = false;
    playbackSpeed = 1.0f;
    freecam = false;
    mCameraKeyframes.clear();
    mTimeKeyframes.clear();
    mBookmarks.clear();
}

double ReplayEditor::durationSec() const {
    return mTickRate > 0 ? (double)mTotalTicks / (double)mTickRate : 0.0;
}

// ── Camera keyframes ────────────────────────────────────────

void ReplayEditor::addCameraKeyframe(int tick, const glm::vec3& pos, const glm::quat& rot,
                                      float roll, float fov, KeyframeInterp interp) {
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
    Debug::log(Debug::Category::Replay, "[RPLE] Camera keyframe %d at tick %d\n",
               (int)mCameraKeyframes.size(), tick);
}

bool ReplayEditor::deleteCameraKeyframe(int index) {
    if (index < 0 || index >= (int)mCameraKeyframes.size()) return false;
    mCameraKeyframes.erase(mCameraKeyframes.begin() + index);
    Debug::log(Debug::Category::Replay, "[RPLE] Deleted camera keyframe %d\n", index);
    return true;
}

void ReplayEditor::clearCameraKeyframes() {
    mCameraKeyframes.clear();
    Debug::log(Debug::Category::Replay, "[RPLE] Cleared all camera keyframes\n");
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
}

bool ReplayEditor::deleteTimeKeyframe(int index) {
    if (index < 0 || index >= (int)mTimeKeyframes.size()) return false;
    mTimeKeyframes.erase(mTimeKeyframes.begin() + index);
    return true;
}

void ReplayEditor::clearTimeKeyframes() {
    mTimeKeyframes.clear();
}

const ReplayEditorTimeKeyframe& ReplayEditor::timeKeyframe(int index) const {
    static ReplayEditorTimeKeyframe sDummy;
    if (index < 0 || index >= (int)mTimeKeyframes.size()) return sDummy;
    return mTimeKeyframes[index];
}

float ReplayEditor::playbackSpeedAtTick(int tick) const {
    if (mTimeKeyframes.empty()) return 1.0f;
    // Find bracketing keyframes
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

// ── Persistence ─────────────────────────────────────────────

bool ReplayEditor::saveEdit() {
    nlohmann::json j;
    j["version"] = 1;
    j["replayPath"] = mReplayPath;
    j["totalTicks"] = mTotalTicks;
    j["tickRate"] = mTickRate;

    // Camera keyframes
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

    // Time keyframes
    auto& timeArr = j["timeKeyframes"] = nlohmann::json::array();
    for (const auto& kf : mTimeKeyframes) {
        nlohmann::json k;
        k["tick"] = kf.tick;
        k["speed"] = kf.speed;
        k["interp"] = interpName(kf.interp);
        timeArr.push_back(k);
    }

    // Bookmarks
    auto& bmArr = j["bookmarks"] = nlohmann::json::array();
    for (const auto& bm : mBookmarks) {
        nlohmann::json b;
        b["tick"] = bm.tick;
        b["label"] = bm.label;
        bmArr.push_back(b);
    }

    // Free camera state
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

    Debug::log(Debug::Category::Replay, "[RPLE] Loaded edit: %d cam kf, %d time kf, %d marks\n",
               (int)mCameraKeyframes.size(), (int)mTimeKeyframes.size(), (int)mBookmarks.size());
    return true;
}

// ── Update ──────────────────────────────────────────────────

void ReplayEditor::update(float dt) {
    if (!mLoaded || !playing) return;

    // Compute playback speed from time timeline
    float speed = playbackSpeedAtTick((int)movieTick);
    movieTick += dt * (float)mTickRate * speed;

    if (movieTick >= mTotalTicks && mTotalTicks > 0) {
        movieTick = (float)mTotalTicks;
        playing = false;
    }
}

void ReplayEditor::seekToTick(int tick) {
    movieTick = (float)std::clamp(tick, 0, mTotalTicks);
}
