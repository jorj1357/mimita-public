#include "player-nameplates.h"
#include "healthbar-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "gui/ui-system.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"

namespace {

constexpr float MAX_HEALTHBAR_DISTANCE = 50.0f;
constexpr float FADE_DISTANCE = 60.0f;

// Per-player aim transition state
std::unordered_map<const Player*, HealthbarAimState> gAimStates;
static int gFrameCounter = 0;

static void cleanAimStates()
{
    if (gAimStates.size() > 256)
        gAimStates.clear();
}

} // namespace

glm::vec3 playerHealthbarAnchor(
    const Player& player,
    bool* usedHeadTransform)
{
    if (usedHeadTransform)
        *usedHeadTransform = false;

    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        if (part.name != "head")
            continue;

        const glm::vec3 localTop{
            (part.collider.localMin.x + part.collider.localMax.x) * 0.5f,
            (part.collider.localMin.y + part.collider.localMax.y) * 0.5f,
            part.collider.localMax.z
        };
        const glm::vec3 headTop =
            glm::vec3(part.worldTransform * glm::vec4(localTop, 1.0f));
        if (usedHeadTransform)
            *usedHeadTransform = true;
        return headTop + glm::vec3(0.0f, 0.0f, 0.3f);
    }

    return player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.72f);
}

// ── Aim cone detection ──────────────────────────────────────────

bool isActorInAimCone(const glm::vec3& camPos, const glm::vec3& camFront,
                      const glm::vec3& targetPos, float coneDegrees)
{
    glm::vec3 toTarget = targetPos - camPos;
    float dist = glm::length(toTarget);
    if (dist < 0.001f) return false;
    toTarget /= dist;
    float cosAngle = glm::clamp(glm::dot(camFront, toTarget), -1.0f, 1.0f);
    float angleDeg = std::acos(cosAngle) * 180.0f / 3.14159265f;
    return angleDeg <= coneDegrees;
}

// ── Transition state management ─────────────────────────────────

HealthbarAimState& getOrCreateAimState(const Player& player)
{
    cleanAimStates();
    return gAimStates[&player];
}

// ── HP color mapping ────────────────────────────────────────────

glm::vec4 healthColorForHp(int currentHp, int maxHp)
{
    const auto& cfg = HealthbarConfig::instance().data();
    float ratio = maxHp > 0 ? (float)currentHp / (float)maxHp : 0.0f;
    if (ratio > 0.75f) return cfg.greenColor;
    if (ratio > 0.50f) return cfg.yellowColor;
    if (ratio > 0.25f) return cfg.orangeColor;
    return cfg.redColor;
}

// ── Debug state ─────────────────────────────────────────────────

static bool gHealthbarDebug = false;

bool isHealthbarDebugEnabled() { return gHealthbarDebug; }
void setHealthbarDebugEnabled(bool enabled) { gHealthbarDebug = enabled; }

// ── Counters (export debug) ─────────────────────────────────────

static int gTotalHealthbarsRendered = 0;
static int gTotalLiveWorldHealthbars = 0;
static int gTotalInvalidHealthbars = 0;

void resetHealthbarCounters()
{
    gTotalHealthbarsRendered = 0;
    gTotalLiveWorldHealthbars = 0;
    gTotalInvalidHealthbars = 0;
}

int getHealthbarTotal() { return gTotalHealthbarsRendered; }
int getHealthbarLiveWorld() { return gTotalLiveWorldHealthbars; }
int getHealthbarInvalid() { return gTotalInvalidHealthbars; }

// ── Main healthbar draw ─────────────────────────────────────────

HealthbarRenderResult drawPlayerHealthbar(
    const Player& player,
    const Camera& camera,
    const char* debugPrefix,
    const char* sourceTag)
{
    HealthbarRenderResult result;
    result.anchor = playerHealthbarAnchor(
        player, &result.usedHeadTransform);

    bool isLiveWorld = (std::strcmp(sourceTag, "live_world") == 0);

    if (player.dead || player.currentHp <= 0)
    {
        RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=1 pos=(%.2f %.2f %.2f) rendered=NO reason=dead actor\n",
                  (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
                  player.currentHp, player.maxHp,
                  result.anchor.x, result.anchor.y, result.anchor.z);
        result.cullReason = HealthbarCullReason::Dead;
        if (isLiveWorld) gTotalLiveWorldHealthbars++;
        gTotalInvalidHealthbars++;
        return result;
    }

    result.distance = glm::length(camera.pos - result.anchor);
    if (result.distance > MAX_HEALTHBAR_DISTANCE)
    {
        result.cullReason = HealthbarCullReason::TooFar;
        return result;
    }

    if (!DebugVis::projectToScreen(
            camera, result.anchor, result.screen.x, result.screen.y))
    {
        result.cullReason = HealthbarCullReason::Offscreen;
        return result;
    }

    // ── Aim mode detection ──────────────────────────────────────
    const auto& cfg = HealthbarConfig::instance().data();
    HealthbarAimState& aimState = getOrCreateAimState(player);
    bool inAimCone = cfg.aimModeEnabled &&
        isActorInAimCone(camera.pos, camera.front, player.pos, cfg.aimConeDegrees);

    // Update transition
    float fadeStep = 1.0f / (float)std::max(1, cfg.triangleFadeTicks);
    if (inAimCone)
    {
        aimState.transitionAlpha = std::min(1.0f, aimState.transitionAlpha + fadeStep);
        aimState.inAimCount++;
    }
    else
    {
        aimState.transitionAlpha = std::max(0.0f, aimState.transitionAlpha - fadeStep);
        aimState.inAimCount = 0;
    }

    bool inAimMode = aimState.transitionAlpha > 0.01f;
    float barAlpha = 1.0f - aimState.transitionAlpha;
    float triAlpha = aimState.transitionAlpha * cfg.triangleAlpha;

    // ── Draw normal healthbar (possibly faded) ──────────────────
    const float fade = std::clamp(
        1.0f - result.distance / FADE_DISTANCE, 0.25f, 1.0f);
    const float ratio = player.maxHp > 0
        ? std::clamp(
            (float)player.currentHp / (float)player.maxHp, 0.0f, 1.0f)
        : 0.0f;
    const float x = result.screen.x;
    const float y = result.screen.y;

    if (!cfg.showBarInAimMode || barAlpha > 0.01f)
    {
        const glm::vec4 barColor{0.55f, 0.03f, 0.03f, 0.9f * fade * barAlpha};
        const glm::vec4 fillColor{0.05f, 0.8f, 0.15f, 0.9f * fade * barAlpha};
        const glm::vec4 textColor{1.0f, 1.0f, 1.0f, fade * barAlpha};

        if (barColor.a > 0.01f)
        {
            uiDrawRect(
                {x - 65.0f, y - 8.0f, 130.0f, 11.0f},
                barColor, "healthbar-bg");
            uiDrawRect(
                {x - 65.0f, y - 8.0f, 130.0f * ratio, 11.0f},
                fillColor, "healthbar-fg");
        }

        if ((!cfg.showNameInAimMode || textColor.a > 0.01f) && textColor.a > 0.01f)
        {
            const float nameWidth = uiMeasureText(player.username.c_str(), 0.28f);
            uiDrawText(
                player.username.c_str(), x - nameWidth * 0.5f, y - 31.0f,
                0.28f, textColor);
        }

        if ((!cfg.showHpTextInAimMode || textColor.a > 0.01f) && textColor.a > 0.01f)
        {
            char healthText[48];
            snprintf(
                healthText, sizeof(healthText), "%d/%d",
                player.currentHp, player.maxHp);
            const float healthWidth = uiMeasureText(healthText, 0.25f);
            uiDrawText(
                healthText, x - healthWidth * 0.5f, y + 8.0f,
                0.25f, textColor);
        }
    }

    // ── Draw aim mode triangle ──────────────────────────────────
    if (inAimMode && triAlpha > 0.01f)
    {
        glm::vec4 triColor = healthColorForHp(player.currentHp, player.maxHp);
        triColor.a *= triAlpha;

        // Blink if HP <= threshold
        if (player.currentHp <= cfg.blinkHpThreshold)
        {
            float blinkPhase = std::cos((float)gFrameCounter * 3.14159265f * 2.0f / (float)cfg.blinkTicks);
            float blinkFactor = blinkPhase * 0.5f + 0.5f;
            triColor = glm::mix(triColor, cfg.blackColor, blinkFactor);
        }

        float triSize = cfg.triangleSize;
        if (aimState.inAimCount < cfg.triangleFadeTicks)
        {
            triSize *= 1.0f + 0.12f * (1.0f - aimState.transitionAlpha);
        }
        uiDrawTriangle(x, y - cfg.triangleOffset, triSize, true, triColor, "aim-tri");
    }

    result.rendered = true;
    gTotalHealthbarsRendered++;
    if (isLiveWorld) gTotalLiveWorldHealthbars++;
    RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=0 pos=(%.2f %.2f %.2f) rendered=YES reason=visible\n",
              (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
              player.currentHp, player.maxHp,
              result.anchor.x, result.anchor.y, result.anchor.z);

    gFrameCounter++;
    return result;
}

const char* healthbarCullReasonName(HealthbarCullReason reason)
{
    switch (reason)
    {
    case HealthbarCullReason::None: return "none";
    case HealthbarCullReason::Dead: return "dead";
    case HealthbarCullReason::TooFar: return "too-far";
    case HealthbarCullReason::Offscreen: return "offscreen";
    }
    return "unknown";
}

// ── Debug overlay ───────────────────────────────────────────────

void drawHealthbarDebugOverlay(const Camera& camera)
{
    if (!gHealthbarDebug) return;
    const auto& cfg = HealthbarConfig::instance().data();

    float y = 80.0f;
    float lx = 16.0f;

    uiDrawText("HEALTHBAR DEBUG", lx, y, 0.32f, {0.0f, 1.0f, 1.0f, 1.0f}); y += 20.0f;
    uiDrawText(("Aim cone: " + std::to_string(cfg.aimConeDegrees) + " deg").c_str(), lx, y, 0.26f, {0.8f, 0.8f, 1.0f, 1.0f}); y += 18.0f;

    for (const auto& kv : gAimStates)
    {
        const Player* p = kv.first;
        const HealthbarAimState& state = kv.second;
        if (!p) continue;

        glm::vec3 anchor = playerHealthbarAnchor(*p, nullptr);
        float sx = 0.0f, sy = 0.0f;
        if (!DebugVis::projectToScreen(camera, anchor, sx, sy))
            continue;

        float angleDeg = 0.0f;
        glm::vec3 toTarget = p->pos - camera.pos;
        float dist = glm::length(toTarget);
        if (dist > 0.001f)
        {
            toTarget /= dist;
            float cosA = glm::clamp(glm::dot(camera.front, toTarget), -1.0f, 1.0f);
            angleDeg = std::acos(cosA) * 180.0f / 3.14159265f;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "%s: angle=%.1fdeg mode=%s triAlpha=%.2f hp=%d",
                 p->username.c_str(), angleDeg,
                 state.transitionAlpha > 0.5f ? "AIM_MODE" : "NORMAL",
                 state.transitionAlpha,
                 p->currentHp);
        uiDrawText(buf, lx, y, 0.26f, {1.0f, 0.9f, 0.5f, 1.0f}); y += 18.0f;

        // Draw cone hint line from anchor to screen center
        uiDrawRect({sx - 2.0f, sy - 2.0f, 4.0f, 4.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, "aim-entity-marker");
    }
}
