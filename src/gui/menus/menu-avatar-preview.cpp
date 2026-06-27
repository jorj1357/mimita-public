#include "menu-avatar-preview.h"
#include "entities/player.h"
#include "camera.h"
#include "render/render-player.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "auth/auth-system.h"
#include "renderer/renderer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using json = nlohmann::json;

extern Renderer* gRenderer;

namespace {

static int64_t getFileModifiedTimeMs(const std::string& path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        ft.time_since_epoch()).count();
}

static glm::vec2 projectToScreen(const Camera& camera, glm::vec3 worldPos,
                                  int vpW, int vpH)
{
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)vpW, (float)vpH);
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f)
        return glm::vec2(-9999.0f, -9999.0f);

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float x = (ndc.x * 0.5f + 0.5f) * (float)vpW;
    float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)vpH;
    return glm::vec2(x, y);
}

}

MenuAvatarPreview& MenuAvatarPreview::instance()
{
    static MenuAvatarPreview preview;
    return preview;
}

void MenuAvatarPreview::loadConfig(const std::string& path)
{
    mConfigPath = path;

    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("[MENU AVATAR] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = getFileModifiedTimeMs(path);
        return;
    }

    try
    {
        json j;
        file >> j;

        if (!j.contains("MainMenuAvatar"))
        {
            printf("[MENU AVATAR] Config missing MainMenuAvatar section\n");
            return;
        }

        auto& c = j["MainMenuAvatar"];
        MenuAvatarPreviewConfig cfg;

        cfg.anchor = c.value("anchor", cfg.anchor);
        cfg.offsetX = c.value("offsetX", cfg.offsetX);
        cfg.offsetY = c.value("offsetY", cfg.offsetY);
        cfg.width = c.value("width", cfg.width);
        cfg.height = c.value("height", cfg.height);

        cfg.cameraDistance = c.value("cameraDistance", cfg.cameraDistance);
        cfg.cameraYaw = c.value("cameraYaw", cfg.cameraYaw);
        cfg.cameraPitch = c.value("cameraPitch", cfg.cameraPitch);
        cfg.cameraFOV = c.value("cameraFOV", cfg.cameraFOV);

        cfg.slowRotationEnabled = c.value("slowRotationEnabled", cfg.slowRotationEnabled);
        cfg.rotationSpeed = c.value("rotationSpeed", cfg.rotationSpeed);
        cfg.playerFootOffsetZ = c.value("playerFootOffsetZ", cfg.playerFootOffsetZ);

        mConfig = cfg;
        mLastModified = getFileModifiedTimeMs(path);
        printf("[MENU AVATAR] Loaded config from %s\n", path.c_str());
    }
    catch (const std::exception& e)
    {
        printf("[MENU AVATAR] Error loading %s: %s\n", path.c_str(), e.what());
    }
}

void MenuAvatarPreview::pollHotReload()
{
    if (mConfigPath.empty()) return;

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - mLastCheckTime < 500) return;
    mLastCheckTime = now;

    int64_t current = getFileModifiedTimeMs(mConfigPath);
    if (current != mLastModified && current != 0)
    {
        printf("[MENU AVATAR] Config changed, reloading...\n");
        loadConfig(mConfigPath);
    }
}

Player* MenuAvatarPreview::ensurePlayer()
{
    if (!mPlayer)
    {
        printf("[MENU AVATAR] Creating preview player...\n");
        mPlayer = new Player();
    }
    return mPlayer;
}

void MenuAvatarPreview::computeViewport(int fbW, int fbH, int& vpX, int& vpY, int& vpW, int& vpH) const
{
    float scaleX = (float)fbW / 1920.0f;
    float scaleY = (float)fbH / 1080.0f;

    float designW = mConfig.width;
    float designH = mConfig.height;
    vpW = (int)(designW * scaleX);
    vpH = (int)(designH * scaleY);

    if (mConfig.anchor == "RightCenter")
    {
        vpX = fbW - vpW + (int)(mConfig.offsetX * scaleX);
        vpY = (fbH - vpH) / 2 + (int)(mConfig.offsetY * scaleY);
    }
    else if (mConfig.anchor == "LeftCenter")
    {
        vpX = (int)(mConfig.offsetX * scaleX);
        vpY = (fbH - vpH) / 2 + (int)(mConfig.offsetY * scaleY);
    }
    else
    {
        vpX = (fbW - vpW) / 2 + (int)(mConfig.offsetX * scaleX);
        vpY = (fbH - vpH) / 2 + (int)(mConfig.offsetY * scaleY);
    }
}

void MenuAvatarPreview::setupCamera(Camera& cam, const glm::vec3& target, int vpW, int vpH)
{
    cam.fov = mConfig.cameraFOV;

    float yawRad = glm::radians(mConfig.cameraYaw + mRotationAngle);
    float pitchRad = glm::radians(mConfig.cameraPitch);

    cam.pos = target + glm::vec3(
        std::sin(yawRad) * std::cos(pitchRad) * mConfig.cameraDistance,
        std::cos(yawRad) * std::cos(pitchRad) * mConfig.cameraDistance,
        std::sin(pitchRad) * mConfig.cameraDistance
    );

    glm::vec3 lookTarget = target + glm::vec3(0.0f, 0.0f, 1.2f);
    cam.front = glm::normalize(lookTarget - cam.pos);
    cam.right = glm::normalize(glm::cross(cam.front, glm::vec3(0.0f, 0.0f, 1.0f)));
    cam.up = glm::normalize(glm::cross(cam.right, cam.front));
}

void MenuAvatarPreview::update(float dt, const glm::vec3& camForward)
{
    Player* p = ensurePlayer();
    if (!p) return;

    if (mConfig.slowRotationEnabled)
    {
        mRotationAngle += mConfig.rotationSpeed * dt;
        if (mRotationAngle > 360.0f) mRotationAngle -= 360.0f;
    }

    p->ground.onGround = true;
    p->updateProceduralAnimation(dt, camForward, glm::vec3(0.0f), false);

    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() == AuthState::Authenticated)
        p->username = auth.user().username;
    else
        p->username = "DefaultGuy";
}

void MenuAvatarPreview::draw(int fbW, int fbH)
{
    Player* p = ensurePlayer();
    if (!p) return;

    p->pos = glm::vec3(0.0f, 0.0f, mConfig.playerFootOffsetZ);
    p->yaw = 180.0f + mConfig.cameraYaw + mRotationAngle;

    int vpX, vpY, vpW, vpH;
    computeViewport(fbW, fbH, vpX, vpY, vpW, vpH);
    if (vpW <= 0 || vpH <= 0) return;

    Camera previewCam;
    setupCamera(previewCam, p->pos, vpW, vpH);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

    glViewport(vpX, vpY, vpW, vpH);
    glEnable(GL_DEPTH_TEST);

    int prevRW = gRenderer->width;
    int prevRH = gRenderer->height;
    gRenderer->width = vpW;
    gRenderer->height = vpH;

    glClearColor(0.04f, 0.045f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderPlayer(*p, previewCam);

    gRenderer->width = prevRW;
    gRenderer->height = prevRH;

    if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    if (p->dead || p->currentHp <= 0) return;

    glm::vec3 headTop = playerHealthbarAnchor(*p);
    glm::vec2 hpScreen = projectToScreen(previewCam, headTop, vpW, vpH);

    if (hpScreen.x < -5000.0f) return;

    float sx = hpScreen.x + (float)vpX;
    float sy = hpScreen.y + (float)vpY;

    float ratio = p->maxHp > 0
        ? std::clamp((float)p->currentHp / (float)p->maxHp, 0.0f, 1.0f)
        : 1.0f;

    float barW = 130.0f;
    float barH = 11.0f;

    float nameWidth = uiMeasureText(p->username.c_str(), 0.28f);
    uiDrawText(p->username.c_str(), sx - nameWidth * 0.5f, sy - 28.0f,
               0.28f, {1.0f, 1.0f, 1.0f, 1.0f});

    uiDrawRect({sx - barW * 0.5f, sy - 5.0f, barW, barH},
               {0.55f, 0.03f, 0.03f, 0.9f}, "menu-avatar-hp-bg");
    uiDrawRect({sx - barW * 0.5f, sy - 5.0f, barW * ratio, barH},
               {0.05f, 0.8f, 0.15f, 0.9f}, "menu-avatar-hp-fg");

    char hpText[48];
    snprintf(hpText, sizeof(hpText), "%d/%d", p->currentHp, p->maxHp);
    float hpTextW = uiMeasureText(hpText, 0.25f);
    uiDrawText(hpText, sx - hpTextW * 0.5f, sy + barH + 2.0f,
               0.25f, {1.0f, 1.0f, 1.0f, 0.85f});
}
