#pragma once

#include "ui-system.h"
#include "gui-layout.h"

struct GLFWwindow;

// Render any GUI element based on its stored type and properties.
// Reads position, size, colors, text, and type from GuiElement.
// Handles design-to-screen coordinate conversion internally.
// For "button" and "checkbox": returns click/hover state.
// For "text", "label", "image", "panel": returns all-false state.
// In edit mode, all elements are tracked for editor selection.
UIButtonState drawGuiElement(GLFWwindow* win, const GuiElement& elem,
                             bool* checkboxValue = nullptr);
