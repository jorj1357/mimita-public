// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-button.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * guiButton(args)
 *
 * this file DOES:
 * - draw one clickable gui button
 * - return true on click
 *
 * this file DOES NOT:
 * - change game state directly
 * - decide menu flow
 */

#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

bool guiButton(
    GLFWwindow* win,
    const char* text,
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color
);