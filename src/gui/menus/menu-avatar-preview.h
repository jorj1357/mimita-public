#pragma once

#include <string>
#include <glm/glm.hpp>

struct Player;
class Camera;

struct MenuAvatarPreviewConfig
{
    std::string anchor = "RightCenter";
    float offsetX = -180.0f;
    float offsetY = 0.0f;
    float width = 500.0f;
    float height = 700.0f;

    float cameraDistance = 3.0f;
    float cameraYaw = -20.0f;
    float cameraPitch = 8.0f;
    float cameraFOV = 35.0f;

    bool slowRotationEnabled = true;
    float rotationSpeed = 0.15f;

    float playerFootOffsetZ = 0.0f;
};

class MenuAvatarPreview
{
public:
    static MenuAvatarPreview& instance();

    void loadConfig(const std::string& path);
    void pollHotReload();

    void update(float dt, const glm::vec3& camForward);
    void draw(int fbW, int fbH);

    Player* ensurePlayer();
    Player* player() const { return mPlayer; }

private:
    MenuAvatarPreview() = default;

    void computeViewport(int fbW, int fbH, int& vpX, int& vpY, int& vpW, int& vpH) const;
    void setupCamera(Camera& cam, const glm::vec3& target, int vpW, int vpH);

    Player* mPlayer = nullptr;
    MenuAvatarPreviewConfig mConfig;
    std::string mConfigPath;
    int64_t mLastModified = 0;
    int64_t mLastCheckTime = 0;
    float mRotationAngle = 0.0f;
};
