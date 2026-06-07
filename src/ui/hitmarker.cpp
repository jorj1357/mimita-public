// C:\important\mimita-priv-v8\src\ui\hitmarker.cpp
// 6 7 2026
/** purpose 
 * HITMaererrkkrkrkrk 
 */

#include "hitmarker.h"

#include <algorithm>

#include <glm/glm.hpp>

#include "gui/ui-system.h"

namespace
{
constexpr float HITMARKER_DURATION = 0.5f;
}

float gHitmarkerTimer = 0.0f;

void hitmarker()
{
    gHitmarkerTimer = HITMARKER_DURATION;
}

void drawHitmarker(float dt)
{
    gHitmarkerTimer =
        std::max(
            0.0f,
            gHitmarkerTimer - dt);

    if (gHitmarkerTimer <= 0.0f)
        return;

    float hmT =
        std::clamp(
            gHitmarkerTimer / HITMARKER_DURATION,
            0.0f,
            1.0f);

    float cx = uiScreenW() * 0.5f;
    float cy = uiScreenH() * 0.5f;

    glm::vec4 color =
    {
        1.0f,
        1.0f,
        1.0f,
        hmT
    };

    // uiDrawImageRotated("assets/crosshair/crosshairhit.png", cx, cy, 28.0f, 45.0f, color);
    uiDrawImageRotated("assets/crosshair/crosshairhit.png", cx, cy, 28.0f, 0.0f, color);
}