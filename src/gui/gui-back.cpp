// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-back.cpp
// mar 8 2026
/**
 * purpose
 * exposes A SINGLE FUNCTION
 * 
 * backButton(args)
 * and the args are like
 * text in the button
 * size of button
 * position of button etc
 */

#include "gui-back.h"
#include "gui-button.h"
#include <cstdio>

bool guiBackButton(GLFWwindow* win, UIRect r)
{
    bool clicked = guiButton(win, "Back", r.x, r.y, r.w, r.h, {0.6f,0.2f,0.2f,1.0f});
    if (clicked)
        printf("[GUI BACK] Back button CLICKED\n");
    return clicked;
}

bool guiBackButton(GLFWwindow* win)
{
    return guiBackButton(win, {40.0f, 40.0f, 120.0f, 50.0f});
}