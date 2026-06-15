#include "crosshair-render.h"

#include <algorithm>

#include <glm/glm.hpp>

#include "crosshair-config.h"
#include "gui/ui-system.h"

static float gDynamicSpread = 0.0f;

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

static void drawConfigured(float centerX, float centerY, float scale, float spread)
{
    const auto& d = CrosshairConfig::instance().data();
    if (!d.enabled) return;

    const float size = d.size * scale;
    const float thickness = d.thickness * scale;
    const float gap = (d.gap + spread) * scale;
    const float outlineThickness = d.outlineThickness * scale;
    const glm::vec4 color{
        d.red / 255.0f, d.green / 255.0f, d.blue / 255.0f, d.alpha / 255.0f
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

void drawCrosshair(float centerX, float centerY, float scale)
{
    drawConfigured(centerX, centerY, scale, gDynamicSpread);
}

void drawCrosshairPreview(float centerX, float centerY, float scale)
{
    drawConfigured(centerX, centerY, scale, 0.0f);
}
