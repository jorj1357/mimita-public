// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-render\gui-rect.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawGuiRect(args)
 *
 * this file DOES:
 * - draw one gui rectangle in screen space
 *
 * this file DOES NOT:
 * - handle clicks
 * - decide menu flow
 */

#pragma once
#include <glm/glm.hpp>

void drawGuiRect(
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color
);