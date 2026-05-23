// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-render\gui-rect.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawGuiRect(args)
 *
 * this file DOES:
 * - draw one gui rect in normalized device coordinates
 *
 * this file DOES NOT:
 * - know about buttons or menus
 */

#include "gui-rect.h"
#include "gui/ui-system.h"

void drawGuiRect(
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color
)
{
    uiDrawRect({x, y, w, h}, color, "legacy-rect");
}
