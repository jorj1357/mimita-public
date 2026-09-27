// C:\important\mimita-priv-v8\src\ui\hitmarker.h
// 6 7 2026
/** purpos
 * hitmarkerrrrr 
 */

#pragma once

extern float gHitmarkerTimer;

void hitmarker(int damage = 0);
void hitmarkerVisualOnly(int damage = 0);
// Draw at the supplied screen-space aim point. Negative coordinates retain the
// legacy center-screen fallback for callers that do not have a world aim point.
void drawHitmarker(float dt, float screenX = -1.0f, float screenY = -1.0f);
