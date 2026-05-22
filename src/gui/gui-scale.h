// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-scale.h
// mar 8 2026
/**
 * purpose
 * makes it so WE CAN SCALE
 * TE GUII
 * WITH ALL WINDOW SIZES
 * THIS IS AWESOME
 * AND GOOD
 */

#pragma once
#include <GLFW/glfw3.h>

struct GuiScale
{
    float scaleX;
    float scaleY;
};

GuiScale guiGetScale(GLFWwindow* win);