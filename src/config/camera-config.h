#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>

struct CameraConfigData {
    glm::vec3 offset{2.0f, -3.5f, 1.0f};
    float fov = 100.0f;
    float positionStiffness = 1.0f;
    float rotationStiffness = 1.0f;
    bool stiffnessEnabled = true;
    bool collisionEnabled = true;
    float lookAheadDistance = 0.0f;
};

class CamConfig {
public:
    static CamConfig& instance();

    bool load(const std::string& path = "config/camconfig.json");
    bool pollReload();

    const CameraConfigData& data() const { return mData; }

private:
    CamConfig() = default;

    CameraConfigData mData;
    std::string mPath = "config/camconfig.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
