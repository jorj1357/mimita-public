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

// ── Camera mode keyframe ────────────────────────────────────
// Controls which camera mode is active at a given tick.

enum class ReplayEditorCamMode {
    ThirdPerson,
    Freecam,
    FirstPerson
};

inline const char* camModeName(ReplayEditorCamMode m) {
    switch (m) {
        case ReplayEditorCamMode::ThirdPerson: return "thirdperson";
        case ReplayEditorCamMode::Freecam: return "freecam";
        case ReplayEditorCamMode::FirstPerson: return "firstperson";
        default: return "thirdperson";
    }
}

inline ReplayEditorCamMode camModeFromString(const std::string& s) {
    if (s == "freecam") return ReplayEditorCamMode::Freecam;
    if (s == "firstperson" || s == "fp") return ReplayEditorCamMode::FirstPerson;
    return ReplayEditorCamMode::ThirdPerson;
}

struct ReplayEditorCamModeKeyframe {
    int tick = 0;
    ReplayEditorCamMode mode = ReplayEditorCamMode::ThirdPerson;
};

// ── Keyframe type for the K prompt ──────────────────────────

enum class KeyframeType {
    CameraPosition,
    CameraMode,
    PlaybackSpeed
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

// ── Autosave entry for Ctrl+Z undo ──────────────────────────

struct ReplayEditorAutosave {
    std::string path;
    double timestamp = 0.0;
};

// ── Audio track for replay editor timeline ──────────────────

struct ReplayEditorAudioTrack {
    std::string path;
    int startTick = 0;
    float startSeconds = 0.0f;
    float volume = 1.0f;
    bool enabled = true;
};

// ── Replay Editor ───────────────────────────────────────────
// Owns timelines: replay (immutable), camera, camera-mode, time.
// Persists to a .rple.json file beside the replay JSON.

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
    float movieTick = 0.0f;
    bool playing = false;
    float playbackSpeed = 1.0f;

    // ── Free camera state ────────────────────────────────
    bool freecam = false;
    std::string mPrevCameraMode;
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
    int findNearestCameraKeyframe(int tick) const;

    // ── Camera mode keyframes ────────────────────────────
    void addCameraModeKeyframe(int tick, ReplayEditorCamMode mode);
    bool deleteCameraModeKeyframe(int index);
    void clearCameraModeKeyframes();
    int cameraModeKeyframeCount() const { return (int)mCameraModeKeyframes.size(); }
    const ReplayEditorCamModeKeyframe& cameraModeKeyframe(int index) const;
    ReplayEditorCamMode cameraModeAtTick(int tick) const;

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

    // ── Audio tracks ────────────────────────────────────
    void setAudioTrack(const std::string& path, float volume = 1.0f);
    void clearAudioTracks();
    int audioTrackCount() const { return (int)mAudioTracks.size(); }
    const ReplayEditorAudioTrack& audioTrack(int index) const;
    ReplayEditorAudioTrack* mutableAudioTrack(int index);

    // ── Persistence ─────────────────────────────────────
    bool saveEdit();
    bool loadEdit();

    // ── Autosave / Undo ─────────────────────────────────
    void autosave();
    bool undoLastAutosave();
    void clearAutosaves();

    // ── Editor tick (call every frame during editing) ────
    void update(float dt);
    void seekToTick(int tick);

    // ── Keyframe prompt state (K dialog) ─────────────────
    int keyframePromptStage = 0;     // 0=none, 1=type, 2=camera-mode
    int keyframePromptTick = 0;      // tick when K was pressed

    // ── Autosave info ──────────────────────────────────
    int autosaveCount() const { return (int)mAutosaves.size(); }

    // ── Keyframe navigation ─────────────────────────────
    int nextKeyframeTick(int currentTick) const;
    int prevKeyframeTick(int currentTick) const;

    // ── Easing (public so engine-tick-camera.cpp can use it) ─
    static float applyEasing(float t, KeyframeInterp interp);

private:
    bool mLoaded = false;
    std::string mReplayPath;
    std::string mEditPath;
    int mTotalTicks = 0;
    int mTickRate = 60;

    std::vector<ReplayEditorCameraKeyframe> mCameraKeyframes;
    std::vector<ReplayEditorCamModeKeyframe> mCameraModeKeyframes;
    std::vector<ReplayEditorTimeKeyframe> mTimeKeyframes;
    std::vector<ReplayEditorBookmark> mBookmarks;
    std::vector<ReplayEditorAudioTrack> mAudioTracks;

    double mAutosaveTimer = 0.0;
    static constexpr double AUTOSAVE_INTERVAL = 30.0;
    static constexpr int MAX_AUTOSAVES = 50;
    std::vector<ReplayEditorAutosave> mAutosaves;

    static float lerpTime(float a, float b, float t, KeyframeInterp interp);
};

// ── Command registration ────────────────────────────────────
void registerReplayEditorCommands();

// ── Global access ───────────────────────────────────────────
extern ReplayEditor gReplayEditor;
