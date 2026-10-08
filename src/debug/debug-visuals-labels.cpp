#include "debug/debug-visuals.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "camera.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "gui/font-stuff/font-loader.h"
#include "config.h"

extern Renderer* gRenderer;

struct DebugTextLabel
{
    glm::vec3 worldPos{0.0f};
    std::string text;
    glm::vec4 color{1.0f};
    float nearAlpha = 1.0f;
    float farDistance = 0.0f;
};

extern std::vector<DebugTextLabel> gTextLabels;
extern GLFWwindow* gWindow;

// =====================================================
// Screen-space label rendering
// =====================================================

bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y)
{
    if (!gRenderer)
        return false;

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f)
        return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f)
        return false;

    x = (ndc.x * 0.5f + 0.5f) * (float)gRenderer->width;
    y = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)gRenderer->height;
    return true;
}

void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color)
{
    if (!text || !*text)
        return;
    if (gTextLabels.size() >= 96)
        return;
    gTextLabels.push_back({worldPos, text, color});
}

void drawWorldLabelFaded(glm::vec3 worldPos, const char* text, glm::vec4 color,
                         float nearAlpha, float farDistance)
{
    if (!text || !*text)
        return;
    if (gTextLabels.size() >= 96)
        return;
    DebugTextLabel label{worldPos, text, color,
                         std::clamp(nearAlpha, 0.0f, 1.0f),
                         std::max(0.001f, farDistance)};
    gTextLabels.push_back(std::move(label));
}

void drawDebugLabels(const Camera& camera)
{
    if (gTextLabels.empty() || !gWindow)
        return;

    uiBeginFrame(gWindow, "player-architecture-labels");
    for (const DebugTextLabel& label : gTextLabels)
    {
        float x = 0.0f;
        float y = 0.0f;
        if (projectToScreen(camera, label.worldPos, x, y)) {
            glm::vec4 color = label.color;
            if (label.farDistance > 0.0f) {
                const float distance = glm::distance(camera.pos, label.worldPos);
                color.a *= std::clamp(label.nearAlpha * (1.0f - distance / label.farDistance), 0.0f, 1.0f);
            }
            if (color.a > 0.0f)
                uiDrawText(label.text.c_str(), x + 4.0f, y - 4.0f, 0.24f, color);
        }
    }
    uiEndFrame();
    gTextLabels.clear();
}

// =====================================================
// DebugVis namespace wrappers (label-related)
// =====================================================

namespace DebugVis {

void drawDiagnosticWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color) {
    ::drawWorldLabel(worldPos, text, color);
}

void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawWorldLabel(worldPos, text, color);
}

void drawWorldLabelFaded(glm::vec3 worldPos, const char* text, glm::vec4 color,
                         float nearAlpha, float farDistance) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawWorldLabelFaded(worldPos, text, color, nearAlpha, farDistance);
}

bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y) {
    return ::projectToScreen(camera, worldPos, x, y);
}

} // namespace DebugVis
