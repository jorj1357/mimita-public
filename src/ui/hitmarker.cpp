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
constexpr float HITMARKER_DURATION = 0.20f;
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

    float gap = 10.0f;
    float len = 18.0f;
    float thick = 4.0f;

    glm::vec4 color =
    {
        1.0f,
        1.0f,
        1.0f,
        hmT
    };

    uiDrawRect(
        {
            cx - gap - len,
            cy - thick * 0.5f,
            len,
            thick
        },
        color,
        "hitmarker-left");

    uiDrawRect(
        {
            cx + gap,
            cy - thick * 0.5f,
            len,
            thick
        },
        color,
        "hitmarker-right");

    uiDrawRect(
        {
            cx - thick * 0.5f,
            cy - gap - len,
            thick,
            len
        },
        color,
        "hitmarker-top");

    uiDrawRect(
        {
            cx - thick * 0.5f,
            cy + gap,
            thick,
            len
        },
        color,
        "hitmarker-bottom");
}