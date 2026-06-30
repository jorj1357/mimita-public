#pragma once

#include <string>
#include <glm/glm.hpp>

struct Player;
class Camera;

struct MenuCharacterPreviewConfig
{
    // Viewport (used by MenuAvatarPreview card, not the hero background)
    std::string anchor = "RightCenter";
    float offsetX = -180.0f;
    float offsetY = 0.0f;
    float width = 500.0f;
    float height = 700.0f;

    // Camera
    glm::vec3 cameraPosition{0.0f, -3.0f, 1.2f};
    glm::vec3 cameraTarget{0.0f, 0.0f, 1.0f};
    float cameraFOV = 35.0f;
    float cameraNear = 0.01f;
    float cameraFar = 1000.0f;

    // Character transform
    glm::vec3 characterPosition{0.0f, 0.0f, 0.0f};
    glm::vec3 characterRotationDeg{0.0f, 0.0f, 0.0f};
    glm::vec3 characterScale{1.0f, 1.0f, 1.0f};

    // Auto-rotation
    bool rotationEnabled = true;
    float rotationDegreesPerSecond = 120.0f;
    bool rotationClockwise = true;
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

    // Shared config access for gui-main.cpp hero/avatar previews
    const MenuCharacterPreviewConfig& config() const { return mConfig; }
    float rotationAngle() const { return mRotationAngle; }

private:
    MenuAvatarPreview() = default;

    void computeViewport(int fbW, int fbH, int& vpX, int& vpY, int& vpW, int& vpH) const;
    void setupCamera(Camera& cam, const glm::vec3& target, int vpW, int vpH);

    Player* mPlayer = nullptr;
    MenuCharacterPreviewConfig mConfig;
    std::string mConfigPath = "config/main_menu_character_preview.json";
    int64_t mLastModified = 0;
    int64_t mLastCheckTime = 0;
    float mRotationAngle = 0.0f;
    int mHotReloadCount = 0;
};
