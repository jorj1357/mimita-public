#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera;

// ── Interpolation mode for camera and time keyframes ────────

enum class KeyframeInterp {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Smooth,
    Cut
};

inline const char* interpName(KeyframeInterp m) {
    switch (m) {
        case KeyframeInterp::Linear: return "linear";
        case KeyframeInterp::EaseIn: return "easein";
        case KeyframeInterp::EaseOut: return "easeout";
        case KeyframeInterp::EaseInOut: return "easeinout";
        case KeyframeInterp::Smooth: return "smooth";
        case KeyframeInterp::Cut: return "cut";
        default: return "linear";
    }
}

inline KeyframeInterp interpFromString(const std::string& s) {
    if (s == "easein") return KeyframeInterp::EaseIn;
    if (s == "easeout") return KeyframeInterp::EaseOut;
    if (s == "easeinout") return KeyframeInterp::EaseInOut;
    if (s == "smooth") return KeyframeInterp::Smooth;
    if (s == "cut") return KeyframeInterp::Cut;
    return KeyframeInterp::Linear;
}

// ── Editor camera keyframe ──────────────────────────────────
// Extends the game replay camera with interpolation mode.

struct ReplayEditorCameraKeyframe {
    int tick = 0;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float roll = 0.0f;
    float fov = 70.0f;
    KeyframeInterp interp = KeyframeInterp::Linear;
};

// ── Time (playback speed) keyframe ──────────────────────────

struct ReplayEditorTimeKeyframe {
    int tick = 0;
    float speed = 1.0f;
    KeyframeInterp interp = KeyframeInterp::Linear;
};

// ── Bookmark ────────────────────────────────────────────────

struct ReplayEditorBookmark {
    int tick = 0;
    std::string label;
};

// ── Replay Editor ───────────────────────────────────────────
// Owns three timelines: replay (immutable), camera, time.
// Persists edits to a .rpledit file beside the replay JSON.

class ReplayEditor {
public:
    // ── Loading ──────────────────────────────────────────
    bool load(const std::string& replayPath);
    void unload();
    bool isLoaded() const { return mLoaded; }

    // ── Replay info ──────────────────────────────────────
    const std::string& replayPath() const { return mReplayPath; }
    const std::string& editPath() const { return mEditPath; }
    int totalTicks() const { return mTotalTicks; }
    int tickRate() const { return mTickRate; }
    double durationSec() const;

    // ── Playback state ───────────────────────────────────
    float movieTick = 0.0f;        // fractional tick (movie time)
    bool playing = false;
    float playbackSpeed = 1.0f;

    // ── Free camera state ────────────────────────────────
    bool freecam = false;
    glm::vec3 freecamPos{0.0f, 0.0f, 0.0f};
    glm::quat freecamRot{1.0f, 0.0f, 0.0f, 0.0f};
    float freecamRoll = 0.0f;
    float freecamFov = 70.0f;
    KeyframeInterp defaultInterp = KeyframeInterp::Linear;

    // ── Camera keyframes ─────────────────────────────────
    void addCameraKeyframe(int tick, const glm::vec3& pos, const glm::quat& rot,
                           float roll, float fov, KeyframeInterp interp);
    bool deleteCameraKeyframe(int index);
    void clearCameraKeyframes();
    int cameraKeyframeCount() const { return (int)mCameraKeyframes.size(); }
    const ReplayEditorCameraKeyframe& cameraKeyframe(int index) const;
    void setCameraKeyframeInterp(int index, KeyframeInterp interp);

    // ── Time keyframes ───────────────────────────────────
    void addTimeKeyframe(int tick, float speed, KeyframeInterp interp);
    bool deleteTimeKeyframe(int index);
    void clearTimeKeyframes();
    int timeKeyframeCount() const { return (int)mTimeKeyframes.size(); }
    const ReplayEditorTimeKeyframe& timeKeyframe(int index) const;
    float playbackSpeedAtTick(int tick) const;

    // ── Bookmarks ───────────────────────────────────────
    void addBookmark(int tick, const std::string& label);
    bool deleteBookmark(int index);
    void clearBookmarks();
    int bookmarkCount() const { return (int)mBookmarks.size(); }
    const ReplayEditorBookmark& bookmark(int index) const;

    // ── Persistence ─────────────────────────────────────
    bool saveEdit();
    bool loadEdit();

    // ── Editor tick (call every frame during editing) ────
    void update(float dt);
    void seekToTick(int tick);

private:
    bool mLoaded = false;
    std::string mReplayPath;    // path to the .json replay
    std::string mEditPath;      // path to the .rpledit file
    int mTotalTicks = 0;
    int mTickRate = 60;

    std::vector<ReplayEditorCameraKeyframe> mCameraKeyframes;
    std::vector<ReplayEditorTimeKeyframe> mTimeKeyframes;
    std::vector<ReplayEditorBookmark> mBookmarks;

    static float applyEasing(float t, KeyframeInterp interp);
    static float lerpTime(float a, float b, float t, KeyframeInterp interp);
};

// ── Command registration ────────────────────────────────────
void registerReplayEditorCommands();

// ── Global access ───────────────────────────────────────────
extern ReplayEditor gReplayEditor;
