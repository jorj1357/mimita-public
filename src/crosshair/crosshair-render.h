#pragma once

#include <cstdint>

void updateCrosshairDynamic(float dt, float speed, bool grounded,
                            bool dashing, bool shooting);
void drawCrosshair(float centerX, float centerY, float scale = 1.0f,
                   bool dryFireTint = false);
void drawCrosshairPreview(float centerX, float centerY, float scale = 2.0f);
void triggerDryFireFeedback(uint64_t uiTick, const char* reason, const char* weaponId);
float dryFireFeedbackFade(uint64_t uiTick, int fadeTicks);
float crosshairRightExtent(float scale = 1.0f);
