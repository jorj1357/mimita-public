// C:\important\mimita-priv-v8\src\ui\hitmarker.cpp
// 6 7 2026
/** purpose 
 * HITMaererrkkrkrkrk 
 */

#include "hitmarker.h"

#include <algorithm>

#include <glm/glm.hpp>

#include "gui/ui-system.h"
#include "audio/hitmarker-audio.h"
#include "effects/hit-effects.h"

float gHitmarkerTimer = 0.0f;

static float hitmarkerDuration()
{
    return std::max(0.01f, HitEffects::config().hitmarkerVisual.duration);
}

static float hitmarkerSize()
{
    return std::max(1.0f, HitEffects::config().hitmarkerVisual.size);
}

void hitmarker(int damage)
{
    hitmarkerVisualOnly(damage);
    playHitmarkerSound(damage);
}

void hitmarkerVisualOnly(int damage)
{
    (void)damage;
    if (!HitEffects::config().hitmarkerVisual.enabled)
        return;
    gHitmarkerTimer = hitmarkerDuration();
}

void drawHitmarker(float dt)
{
    const float duration = hitmarkerDuration();

    gHitmarkerTimer =
        std::max(
            0.0f,
            gHitmarkerTimer - dt);

    if (gHitmarkerTimer <= 0.0f)
        return;

    float hmT =
        std::clamp(
            gHitmarkerTimer / duration,
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

    uiDrawImageRotated("assets/crosshair/crosshairhit.png", cx, cy, hitmarkerSize(), 0.0f, color);
}
