// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-label.cpp
// mar 8 2026
/**
 * purpose
 * exposes A SINGLE FUNCTION
 * 
 * textLabel(args)
 * and the args are like
 * text in the button
 * size of button
 * position of button etc
 */

/**
 * mar 14 2026
 * gui-label.cpp should only render text, not load fonts.
 */

#include "gui-label.h"
#include "gui/ui-system.h"

void guiLabel(const char* text, float x, float y)
{
    uiDrawText(text, x, y, 2.0f, {0.92f, 0.94f, 1.0f, 1.0f});
}
