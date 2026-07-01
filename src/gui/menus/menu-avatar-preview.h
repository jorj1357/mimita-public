#pragma once

#include <string>
#include <glm/glm.hpp>

struct Player;
class Camera;

struct MenuCharacterPreviewConfig
{
    // Viewport
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
    float orbitDistance = 3.0f;
    float orbitHeight = 1.2f;
    float orbitYaw = 0.0f;
    float orbitPitch = 10.0f;

    // Character transform
    glm::vec3 characterPosition{0.0f, 0.0f, 0.0f};
    glm::vec3 characterRotationDeg{0.0f, 0.0f, 0.0f};
    glm::vec3 characterScale{1.0f, 1.0f, 1.0f};
    float initialYaw = 0.0f;
    float initialPitch = 0.0f;
    float initialRoll = 0.0f;

    // Auto-rotation
    bool rotationEnabled = true;
    float rotationDegreesPerSecond = 120.0f;
    bool rotationClockwise = true;
    float rotationStartAngle = 0.0f;
    bool rotationPauseWhenHovered = false;
    bool rotationReverse = false;
    float rotationAcceleration = 0.0f;

    // Lighting
    glm::vec3 ambientColor{0.04f, 0.045f, 0.06f};
    float exposure = 1.0f;
    float brightness = 1.0f;
    float gamma = 2.2f;

    // Render toggles
    bool drawWeapon = false;
    bool drawNameplate = true;
    bool drawHealthbar = true;
    bool drawShadow = false;
    bool drawOutline = false;

    // Animation
    std::string idleAnimation = "idle";
    float idleSpeed = 1.0f;
    float idleStartTime = 0.0f;
    float animationSpeed = 1.0f;

    // Background
    bool backgroundEnabled = true;
    float backgroundBlur = 0.0f;
    float backgroundOpacity = 1.0f;

    // Model
    glm::vec3 modelOffset{0.0f, 0.0f, 0.0f};
    bool lookAtCamera = false;
    float floorOffset = 0.0f;

    // Camera controls
    float zoom = 1.0f;
    float minZoom = 0.5f;
    float maxZoom = 3.0f;
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

    const MenuCharacterPreviewConfig& config() const { return mConfig; }
    float rotationAngle() const { return mRotationAngle; }

public:
    static void printMenuPreviewConfig(const MenuCharacterPreviewConfig& c, const MenuCharacterPreviewConfig& p, bool first);

private:
    MenuAvatarPreview() = default;

    void computeViewport(int fbW, int fbH, int& vpX, int& vpY, int& vpW, int& vpH) const;
    void setupCamera(Camera& cam, const glm::vec3& target, int vpW, int vpH);
    void printChangedConfig(const MenuCharacterPreviewConfig& prev);

    Player* mPlayer = nullptr;
    MenuCharacterPreviewConfig mConfig;
    std::string mConfigPath = "config/gui/menu-avatar-preview.json";
    int64_t mLastModified = 0;
    int64_t mLastCheckTime = 0;
    float mRotationAngle = 0.0f;
    int mHotReloadCount = 0;
    bool mFirstLoad = true;
};
