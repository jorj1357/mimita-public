#include "crosshair-render.h"

#include <algorithm>

#include <glm/glm.hpp>

#include "crosshair-config.h"
#include "debug/structured-log.h"
#include "gui/ui-system.h"

static float gDynamicSpread = 0.0f;
static bool gDryFireTriggered = false;
static uint64_t gDryFireStartTick = 0;
static uint64_t gDryFireLastLogTick = 0;
static bool gDryFireLogged = false;

void triggerDryFireFeedback(uint64_t uiTick, const char* reason, const char* weaponId)
{
    gDryFireTriggered = true;
    gDryFireStartTick = uiTick;
    if (!gDryFireLogged || uiTick >= gDryFireLastLogTick + 60) {
        gDryFireLogged = true;
        gDryFireLastLogTick = uiTick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "weapon.dry_fire_feedback", weaponId,
            "client weapon-readiness rejection refreshed HUD feedback", uiTick,
            {{"weapon", weaponId}, {"reason", reason}, {"ui_tick", uiTick},
             {"start_alpha", 1.0}, {"end_alpha", 0.0}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

float dryFireFeedbackFade(uint64_t uiTick, int fadeTicks)
{
    if (!gDryFireTriggered || fadeTicks <= 0) return 0.0f;
    const uint64_t elapsed = uiTick >= gDryFireStartTick
        ? uiTick - gDryFireStartTick : 0;
    if (elapsed >= static_cast<uint64_t>(fadeTicks)) return 0.0f;
    return 1.0f - static_cast<float>(elapsed) / static_cast<float>(fadeTicks);
}

float crosshairRightExtent(float scale)
{
    const auto& d = CrosshairConfig::instance().data();
    return (d.gap + gDynamicSpread + d.size + (d.outline ? d.outlineThickness : 0.0f)) * scale;
}

static void drawPart(UIRect rect, glm::vec4 color,
                     bool outline, float outlineThickness)
{
    if (outline && outlineThickness > 0.0f) {
        uiDrawRect({
            rect.x - outlineThickness, rect.y - outlineThickness,
            rect.w + outlineThickness * 2.0f,
            rect.h + outlineThickness * 2.0f
        }, {0.0f, 0.0f, 0.0f, color.a}, "crosshair-outline");
    }
    uiDrawRect(rect, color, "crosshair");
}

void updateCrosshairDynamic(float dt, float speed, bool grounded,
                            bool dashing, bool shooting)
{
    const auto& d = CrosshairConfig::instance().data();
    float target = 0.0f;
    if (d.dynamic) {
        target += std::min(speed * 0.18f, 8.0f);
        if (!grounded) target += 6.0f;
        if (dashing) target += 8.0f;
        if (shooting) target += 5.0f;
    }
    const float response = target > gDynamicSpread ? 18.0f : 10.0f;
    gDynamicSpread += (target - gDynamicSpread) *
        std::min(dt * response, 1.0f);
}

static void drawConfigured(float centerX, float centerY, float scale, float spread,
                           bool dryFireTint)
{
    const auto& d = CrosshairConfig::instance().data();
    if (!d.enabled) return;

    const float size = d.size * scale;
    const float thickness = d.thickness * scale;
    const float gap = (d.gap + spread) * scale;
    const float outlineThickness = d.outlineThickness * scale;
    const glm::vec4 color{
        (dryFireTint ? d.dryFireRed : d.red) / 255.0f,
        (dryFireTint ? d.dryFireGreen : d.green) / 255.0f,
        (dryFireTint ? d.dryFireBlue : d.blue) / 255.0f,
        d.alpha / 255.0f
    };

    if (d.showLeft)
        drawPart({centerX - gap - size, centerY - thickness * 0.5f, size, thickness},
                 color, d.outline, outlineThickness);
    if (d.showRight)
        drawPart({centerX + gap, centerY - thickness * 0.5f, size, thickness},
                 color, d.outline, outlineThickness);
    if (d.showTop)
        drawPart({centerX - thickness * 0.5f, centerY - gap - size, thickness, size},
                 color, d.outline, outlineThickness);
    if (d.showBottom)
        drawPart({centerX - thickness * 0.5f, centerY + gap, thickness, size},
                 color, d.outline, outlineThickness);
    if (d.dot)
        drawPart({centerX - thickness * 0.5f, centerY - thickness * 0.5f,
                  thickness, thickness}, color, d.outline, outlineThickness);
}

void drawCrosshair(float centerX, float centerY, float scale, bool dryFireTint)
{
    drawConfigured(centerX, centerY, scale, gDynamicSpread, dryFireTint);
}

void drawCrosshairPreview(float centerX, float centerY, float scale)
{
    drawConfigured(centerX, centerY, scale, 0.0f, false);
}
