#pragma once

void updateCrosshairDynamic(float dt, float speed, bool grounded,
                            bool dashing, bool shooting);
void drawCrosshair(float centerX, float centerY, float scale = 1.0f);
void drawCrosshairPreview(float centerX, float centerY, float scale = 2.0f);
