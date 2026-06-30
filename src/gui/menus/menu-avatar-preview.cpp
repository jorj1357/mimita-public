#include "menu-avatar-preview.h"
#include "entities/player.h"
#include "camera.h"
#include "render/render-player.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "auth/auth-system.h"
#include "config.h"

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
        printf("[MENU PREVIEW] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = getFileModifiedTimeMs(path);
        return;
    }

    try
    {
        json j;
        file >> j;

        MenuCharacterPreviewConfig cfg;

        auto readV3 = [&](const json& o, const char* k, glm::vec3& v) {
            if (o.contains(k) && o[k].is_array() && o[k].size() >= 3)
                v = glm::vec3(o[k][0].get<float>(), o[k][1].get<float>(), o[k][2].get<float>());
        };

        cfg.anchor = j.value("anchor", cfg.anchor);
        cfg.offsetX = j.value("offsetX", cfg.offsetX);
        cfg.offsetY = j.value("offsetY", cfg.offsetY);
        cfg.width = j.value("width", cfg.width);
        cfg.height = j.value("height", cfg.height);

        if (j.contains("camera")) {
            auto& c = j["camera"];
            readV3(c, "position", cfg.cameraPosition);
            readV3(c, "target", cfg.cameraTarget);
            cfg.cameraFOV = c.value("fov", cfg.cameraFOV);
            cfg.cameraNear = c.value("near", cfg.cameraNear);
            cfg.cameraFar = c.value("far", cfg.cameraFar);
        }

        if (j.contains("character")) {
            auto& ch = j["character"];
            readV3(ch, "position", cfg.characterPosition);
            readV3(ch, "rotation_degrees", cfg.characterRotationDeg);
            readV3(ch, "scale", cfg.characterScale);
        }

        if (j.contains("rotation")) {
            auto& rot = j["rotation"];
            cfg.rotationEnabled = rot.value("enabled", cfg.rotationEnabled);
            cfg.rotationDegreesPerSecond = rot.value("degrees_per_second", cfg.rotationDegreesPerSecond);
            cfg.rotationClockwise = rot.value("clockwise", cfg.rotationClockwise);
        }

        mConfig = cfg;
        mLastModified = getFileModifiedTimeMs(path);
        mHotReloadCount++;
        printf("[MENU PREVIEW] Loaded config from %s\n", path.c_str());
    }
    catch (const std::exception& e)
    {
        printf("[MENU PREVIEW] Error loading %s: %s\n", path.c_str(), e.what());
    }
}

void MenuAvatarPreview::pollHotReload()
{
    if (mConfigPath.empty())
    {
        loadConfig("config/main_menu_character_preview.json");
        return;
    }

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - mLastCheckTime < 500) return;
    mLastCheckTime = now;

    int64_t current = getFileModifiedTimeMs(mConfigPath);
    if (current != mLastModified && current != 0)
    {
        printf("[MENU PREVIEW] Config changed, reloading...\n");
        loadConfig(mConfigPath);
    }
}

Player* MenuAvatarPreview::ensurePlayer()
{
    if (!mPlayer)
    {
        printf("[MENU PREVIEW] Creating preview player...\n");
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

    vpX = mConfig.anchor == "RightCenter" ? fbW - vpW : (fbW - vpW) / 2;
    vpX += (int)(mConfig.offsetX * scaleX);
    vpY = (fbH - vpH) / 2 + (int)(mConfig.offsetY * scaleY);
}

void MenuAvatarPreview::setupCamera(Camera& cam, const glm::vec3& target, int vpW, int vpH)
{
    (void)target; (void)vpW; (void)vpH;
    cam.fov = mConfig.cameraFOV;

    // Orbital camera around target with yaw from rotation angle
    float yawRad = glm::radians(mRotationAngle);
    glm::vec3 offset = mConfig.cameraPosition;
    float cosA = std::cos(yawRad);
    float sinA = std::sin(yawRad);
    glm::vec3 rotated(
        offset.x * cosA - offset.y * sinA,
        offset.x * sinA + offset.y * cosA,
        offset.z
    );

    cam.pos = mConfig.cameraTarget + rotated;

    glm::vec3 lookTarget = mConfig.cameraTarget;
    cam.front = glm::normalize(lookTarget - cam.pos);
    cam.right = glm::normalize(glm::cross(cam.front, glm::vec3(0.0f, 0.0f, 1.0f)));
    cam.up = glm::normalize(glm::cross(cam.right, cam.front));
}

void MenuAvatarPreview::update(float dt, const glm::vec3& camForward)
{
    Player* p = ensurePlayer();
    if (!p) return;

    if (mConfig.rotationEnabled)
    {
        float dir = mConfig.rotationClockwise ? 1.0f : -1.0f;
        mRotationAngle += mConfig.rotationDegreesPerSecond * dir * dt;
        if (mRotationAngle > 360.0f) mRotationAngle -= 360.0f;
        if (mRotationAngle < 0.0f) mRotationAngle += 360.0f;
    }

    p->ground.onGround = true;
    p->updateProceduralAnimation(dt, camForward, glm::vec3(0.0f), false);

    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() == AuthState::Authenticated)
        p->username = auth.user().username;
    else
        p->username = "DefaultGuy";

    // Debug logging
    if (DebugConfig::DEBUG_MENU_PREVIEW) {
        static float logTimer = 0.0f;
        logTimer -= dt;
        if (logTimer <= 0.0f) {
            logTimer = 1.0f;
            printf("[MENU PREVIEW] rot=%.1f fov=%.0f cam=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) speed=%.0f reloads=%d\n",
                   mRotationAngle, mConfig.cameraFOV,
                   mConfig.cameraPosition.x, mConfig.cameraPosition.y, mConfig.cameraPosition.z,
                   mConfig.cameraTarget.x, mConfig.cameraTarget.y, mConfig.cameraTarget.z,
                   mConfig.rotationDegreesPerSecond, mHotReloadCount);
        }
    }
}

void MenuAvatarPreview::draw(int fbW, int fbH)
{
    Player* p = ensurePlayer();
    if (!p) return;

    p->pos = mConfig.characterPosition;
    p->yaw = mConfig.characterRotationDeg.z + mRotationAngle;

    int vpX, vpY, vpW, vpH;
    computeViewport(fbW, fbH, vpX, vpY, vpW, vpH);
    if (vpW <= 0 || vpH <= 0) return;

    Camera previewCam;
    setupCamera(previewCam, p->pos, vpW, vpH);

    GLint prevVp[4]; glGetIntegerv(GL_VIEWPORT, prevVp);
    GLboolean depthEn = glIsEnabled(GL_DEPTH_TEST);
    glViewport(vpX, vpY, vpW, vpH);
    glEnable(GL_DEPTH_TEST);

    int prevRW = gRenderer->width, prevRH = gRenderer->height;
    gRenderer->width = vpW; gRenderer->height = vpH;
    glClearColor(0.04f, 0.045f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayer(*p, previewCam);
    gRenderer->width = prevRW; gRenderer->height = prevRH;
    if (!depthEn) glDisable(GL_DEPTH_TEST);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);

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
