// 08 03 2026, 17 20
/* purpose
* Owns the animated avatar preview shown in game menus.
* Loads preview layout config and renders the preview player camera pass.
* Applies authenticated VIP appearance to the preview nameplate.
* DOES NOT own account authentication, entitlement verification, or website routes.
* DOES NOT mutate live gameplay players outside the preview instance.
* DOES NOT replace the shared healthbar or player rendering systems.
*/

#include "menu-avatar-preview.h"
#include "entities/player.h"
#include "camera.h"
#include "render/render-player.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "auth/auth-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "vip/vip-name-render.h"

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

// Section loaders (defined in menu-avatar-preview-debug.cpp)
bool loadViewportSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadCameraSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadCharacterSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadRotationSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadLightingSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadRenderSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadAnimationSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadBackgroundSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadModelSection(const json& j, MenuCharacterPreviewConfig& cfg);
bool loadCameraControlsSection(const json& j, MenuCharacterPreviewConfig& cfg);

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

} // namespace
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
        return;
    }

    try
    {
        json j;
        file >> j;
        if (!j.is_object())
        {
            printf("[MENU PREVIEW] Config root is not an object: %s\n", path.c_str());
            return;
        }

        MenuCharacterPreviewConfig prev = mConfig;
        MenuCharacterPreviewConfig cfg = mConfig;

        loadViewportSection(j, cfg);
        loadCameraSection(j, cfg);
        loadCharacterSection(j, cfg);
        loadRotationSection(j, cfg);
        loadLightingSection(j, cfg);
        loadRenderSection(j, cfg);
        loadAnimationSection(j, cfg);
        loadBackgroundSection(j, cfg);
        loadModelSection(j, cfg);
        loadCameraControlsSection(j, cfg);

        mConfig = cfg;
        mLastModified = getFileModifiedTimeMs(path);
        mHotReloadCount++;
        printf("[MENU PREVIEW] Loaded config: %s\n", path.c_str());
        printMenuPreviewConfig(mConfig, prev, mFirstLoad);
        mFirstLoad = false;
    }
    catch (const std::exception& e) {
        printf("[MENU PREVIEW] Error loading %s: %s\n", path.c_str(), e.what());
        printf("[MENU PREVIEW] Previous config preserved.\n");
    }
}
void MenuAvatarPreview::pollHotReload()
{
    if (mConfigPath.empty()) {
        loadConfig("config/gui/menu-avatar-preview.json");
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
        mPlayer = new Player(false);
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

    float yawRad = glm::radians(mRotationAngle + mConfig.orbitYaw);
    float pitchRad = glm::radians(mConfig.orbitPitch);
    float dist = mConfig.orbitDistance * mConfig.zoom;
    glm::vec3 orbitOffset(
        std::sin(yawRad) * std::cos(pitchRad) * dist,
        std::cos(yawRad) * std::cos(pitchRad) * dist,
        std::sin(pitchRad) * dist + mConfig.orbitHeight
    );

    glm::vec3 offset = mConfig.cameraPosition + orbitOffset;
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

    // Check if async model + atlas build finished
    p->finalizeModelIfReady();
    AvatarSystem::instance().finalizeAtlasIfReady(*p);

    float speedDt = dt * mConfig.animationSpeed;
    p->proceduralTime += speedDt * mConfig.idleSpeed;

    if (mConfig.rotationEnabled)
    {
        float dir = mConfig.rotationClockwise ? 1.0f : -1.0f;
        float speed = mConfig.rotationDegreesPerSecond;
        if (mConfig.rotationAcceleration > 0.0f)
            speed += mConfig.rotationAcceleration * (float)mHotReloadCount;
        mRotationAngle += speed * dir * speedDt;
        if (mRotationAngle > 360.0f) mRotationAngle -= 360.0f;
        if (mRotationAngle < 0.0f) mRotationAngle += 360.0f;
    }

    p->pos.z = mConfig.floorOffset;
    p->ground.onGround = true;
    p->updateProceduralAnimation(speedDt, camForward, glm::vec3(0.0f), false);

    p->username = AuthSystem::instance().displayName();
    p->vipAppearance = AuthSystem::instance().user().vipAppearance;
    Debug::logThrottled(Debug::Category::Gui, "avatar-username", 3.0f,
        "username=%s source=AuthSystem.displayName()\n", p->username.c_str());
    if (DebugConfig::DEBUG_MENU_PREVIEW)
    {
        static float logTimer = 0.0f;
        logTimer -= dt;
        if (logTimer <= 0.0f)
        {
            logTimer = 1.0f;
            printf("[MENU PREVIEW] rot=%.1f fov=%.0f cam=(%.1f,%.1f,%.1f) "
                   "target=(%.1f,%.1f,%.1f) speed=%.0f reloads=%d zoom=%.2f\n",
                   mRotationAngle, mConfig.cameraFOV,
                   mConfig.cameraPosition.x, mConfig.cameraPosition.y, mConfig.cameraPosition.z,
                   mConfig.cameraTarget.x, mConfig.cameraTarget.y, mConfig.cameraTarget.z,
                   mConfig.rotationDegreesPerSecond, mHotReloadCount, mConfig.zoom);
        }
    }
}

static void drawNameplateUI(const Player& p, const Camera& cam, int vpX, int vpY, int vpW, int vpH)
{
    glm::vec3 headTop = playerHealthbarAnchor(p);
    glm::vec2 screen = projectToScreen(cam, headTop, vpW, vpH);
    if (screen.x < -5000.0f) return;
    float sx = screen.x + (float)vpX;
    float sy = screen.y + (float)vpY;
    VipNameDrawOptions nameOptions;
    nameOptions.scale = 0.28f;
    nameOptions.alpha = 1.0f;
    nameOptions.phase = 0.0f;
    vipDrawStyledNameCentered(p.username, p.vipAppearance, sx, sy - 28.0f,
                              nameOptions);
}

static void drawHealthbarUI(const Player& p, const Camera& cam, int vpX, int vpY, int vpW, int vpH)
{
    glm::vec3 headTop = playerHealthbarAnchor(p);
    glm::vec2 screen = projectToScreen(cam, headTop, vpW, vpH);
    if (screen.x < -5000.0f) return;
    float sx = screen.x + (float)vpX;
    float sy = screen.y + (float)vpY;
    float ratio = p.maxHp > 0
        ? std::clamp((float)p.currentHp / (float)p.maxHp, 0.0f, 1.0f) : 1.0f;
    float barW = 130.0f, barH = 11.0f;
    uiDrawRect({sx - barW * 0.5f, sy - 5.0f, barW, barH},
               {0.55f, 0.03f, 0.03f, 0.9f}, "menu-avatar-hp-bg");
    uiDrawRect({sx - barW * 0.5f, sy - 5.0f, barW * ratio, barH},
               {0.05f, 0.8f, 0.15f, 0.9f}, "menu-avatar-hp-fg");
    char hpText[48];
    snprintf(hpText, sizeof(hpText), "%d/%d", p.currentHp, p.maxHp);
    float hpTextW = uiMeasureText(hpText, 0.25f);
    uiDrawText(hpText, sx - hpTextW * 0.5f, sy + barH + 2.0f,
               0.25f, {1.0f, 1.0f, 1.0f, 0.85f});
}

void MenuAvatarPreview::draw(int fbW, int fbH)
{
    Player* p = ensurePlayer();
    if (!p) return;

    glm::vec3 basePos = mConfig.characterPosition + mConfig.modelOffset;
    float yaw = mConfig.characterRotationDeg.z + mRotationAngle;
    if (mConfig.lookAtCamera) yaw = 180.0f + mRotationAngle;

    p->pos = basePos;
    p->yaw = yaw;
    p->meshScale = mConfig.characterScale;

    int vpX, vpY, vpW, vpH;
    computeViewport(fbW, fbH, vpX, vpY, vpW, vpH);
    if (vpW <= 0 || vpH <= 0) return;

    Camera previewCam;
    setupCamera(previewCam, p->pos, vpW, vpH);

    GLint prevVp[4]; glGetIntegerv(GL_VIEWPORT, prevVp);
    GLboolean depthEn = glIsEnabled(GL_DEPTH_TEST);
    GLboolean scissorEn = glIsEnabled(GL_SCISSOR_TEST);
    GLint prevScissor[4]; glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
    glViewport(vpX, vpY, vpW, vpH);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vpX, vpY, vpW, vpH);
    glEnable(GL_DEPTH_TEST);

    int prevRW = gRenderer->width, prevRH = gRenderer->height;
    gRenderer->width = vpW; gRenderer->height = vpH;

    if (mConfig.backgroundEnabled)
        glClearColor(mConfig.ambientColor.r, mConfig.ambientColor.g, mConfig.ambientColor.b, mConfig.backgroundOpacity);
    else
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderPlayer(*p, previewCam);

    gRenderer->width = prevRW; gRenderer->height = prevRH;
    if (!depthEn) glDisable(GL_DEPTH_TEST);
    if (!scissorEn) glDisable(GL_SCISSOR_TEST); else glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);

    if (p->dead || p->currentHp <= 0) return;
    if (mConfig.drawNameplate) drawNameplateUI(*p, previewCam, vpX, vpY, vpW, vpH);
    if (mConfig.drawHealthbar) drawHealthbarUI(*p, previewCam, vpX, vpY, vpW, vpH);
}
