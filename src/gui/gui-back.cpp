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

bool guiBackButton(GLFWwindow* win)
{
    printf("[GUI BACK] begin\n");

    bool clicked = guiButton(
        win,
        "Back",
        40,
        40,
        120,
        50,
        {0.6f,0.2f,0.2f,1.0f}
    );

    if (clicked)
    {
        printf("[GUI BACK] Back button CLICKED\n");
    }
    else
    {
        printf("[GUI BACK] Back button not clicked\n");
    }

    printf("[GUI BACK] end\n");

    return clicked;
}