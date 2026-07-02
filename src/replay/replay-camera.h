#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;

struct CameraKeyframe {
    int tick = 0;
    glm::vec3 position{};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    std::string mode;
};

class ReplayCameraMgr {
public:
    void addKeyframe(int tick, const Camera& camera);
    bool deleteKeyframe(int index);
    void clearKeyframes();
    int keyframeCount() const { return (int)mKeyframes.size(); }
    const CameraKeyframe& keyframe(int index) const { return mKeyframes[index]; }

    bool save(const std::string& path = "config/cameratimeline.json");
    bool load(const std::string& path = "config/cameratimeline.json");

    void setMode(const std::string& mode) { mMode = mode; }
    const std::string& mode() const { return mMode; }

    void update(int currentTick, Camera& camera, float dt);

private:
    std::vector<CameraKeyframe> mKeyframes;
    std::string mMode = "follow";

    static bool lerpKeyframes(const CameraKeyframe& a, const CameraKeyframe& b,
                              float t, Camera& camera);
    void applyModeChange(int tick);
};

void registerReplayCameraCommands();
