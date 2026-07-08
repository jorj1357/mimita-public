#pragma once

#include <string>
#include <filesystem>
#include <glm/glm.hpp>

class Camera;

// ── Face order: 0=+X(right), 1=-X(left), 2=+Y(top), 3=-Y(bottom), 4=+Z(front), 5=-Z(back)

struct SkyboxFaceConfig {
    std::string path;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float alpha = 1.0f;
    float rotationSpeed = 0.0f;
    float hueSpeed = 0.0f;
    float stretchXSpeed = 0.0f;
    float stretchYSpeed = 0.0f;
    float uvScrollX = 0.0f;
    float uvScrollY = 0.0f;
};

struct SkyboxFaceAnim {
    float rotation = 0.0f;
    float hue = 0.0f;
    float stretchX = 1.0f;
    float stretchY = 1.0f;
    float uvOffX = 0.0f;
    float uvOffY = 0.0f;
};

static constexpr const char* SKYBOX_FACE_NAMES[6] = {
    "right", "left", "top", "bottom", "front", "back"
};

class Skybox {
public:
    Skybox();
    ~Skybox();

    void init();
    bool loadConfig(const std::string& path = "config/skybox.json");
    void pollReload();
    void update(float dt);
    void render(const Camera& camera);

    bool isEnabled();

private:
    void loadCubemap();
    void destroyCubemap();
    void compileShader();
    void createMesh();
    void destroyMesh();
    void setUniforms() const;

    bool mEnabled = true;
    bool mInitialized = false;
    std::string mFolder;
    float mGlobalRotationSpeed = 0.0f;
    float mGlobalHueSpeed = 0.0f;
    float mGlobalScaleX = 1.0f;
    float mGlobalScaleY = 1.0f;
    float mGlobalRotation = 0.0f;
    float mGlobalHue = 0.0f;

    SkyboxFaceConfig mFaces[6];
    SkyboxFaceAnim mFaceAnim[6];

    GLuint mCubemapTex = 0;
    GLuint mVAO = 0;
    GLuint mVBO = 0;
    GLuint mShader = 0;

    std::string mConfigPath;
    std::filesystem::file_time_type mConfigLastWrite;
};

extern Skybox gSkybox;
