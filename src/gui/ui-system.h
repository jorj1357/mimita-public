#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct UIRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct UIButtonState {
    bool hovered = false;
    bool pressed = false;
    bool clicked = false;
};

struct UITrackedWidget {
    std::string id;
    UIRect rect;
    bool hovered = false;
    bool pressed = false;
};

void uiInit(GLFWwindow* win);
void uiBeginFrame(GLFWwindow* win, const char* passName);
void uiEndFrame();

void uiSetDebug(bool enabled);
void uiSetEditMode(bool enabled);
bool uiEditModeEnabled();
bool uiDebugEnabled();

void uiSetOverlapDebug(bool enabled);
bool uiOverlapDebugEnabled();

void uiDrawRect(UIRect r, glm::vec4 color, const char* debugName);
void uiDrawRectOutline(UIRect r, glm::vec4 color, const char* debugName);
void uiDrawText(const char* text, float x, float y, float scale, glm::vec4 color);
void uiDrawImage(const char* path, UIRect r, glm::vec4 color = glm::vec4(1.0f));
void uiDrawMedia(const char* path, UIRect r, glm::vec4 color = glm::vec4(1.0f));
void uiUpdateMedia(float dt);
void uiDrawImageRotated(const char* path, float cx, float cy, float halfSize, float angleDeg, glm::vec4 color = glm::vec4(1.0f));
void uiDrawWarning(const char* text, float x, float y);
float uiMeasureText(const char* text, float scale);

UIButtonState uiButton(GLFWwindow* win, const char* text, UIRect r, glm::vec4 color, const char* id = nullptr);
bool uiCheckbox(GLFWwindow* win, const char* label, UIRect r, bool* value);
bool uiSlider(GLFWwindow* win, const char* label, UIRect r, float* value, float minValue, float maxValue);
void uiPlaceholderImageButton(GLFWwindow* win, const char* label, UIRect r);

float uiScaleX(float px);
float uiScaleY(float px);

float uiScreenW();
float uiScreenH();

UIRect uiCentered(float w, float h, float y);

UIRect uiRow(
    float x,
    float& y,
    float w,
    float h,
    float gap
);

void uiRenderFrameDebugOverlay(GLFWwindow* win, const char* activeScene, bool worldPassRan);

// Widget tracking for GUI editor
const std::vector<UITrackedWidget>& uiGetTrackedWidgets();
void uiTrackWidget(const char* id, UIRect designRect);

// Coordinate debug overlay
void uiSetCoordDebug(bool enabled);
bool uiCoordDebugEnabled();
