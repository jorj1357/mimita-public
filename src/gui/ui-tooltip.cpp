#include "ui-tooltip.h"
#include "ui-system-internal.h"
#include "ui-system.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <GLFW/glfw3.h>

static std::unordered_map<std::string, std::string> gTooltipMap;
static std::string gLastHovered;
static double gHoverStart = 0.0;
static std::string gFadingText;
static double gFadeStart = 0.0;

void uiSetTooltip(const char* widgetId, const char* text)
{
    if (widgetId && text)
        gTooltipMap[widgetId] = text;
}

void uiDrawTooltip()
{
    const std::string& hovered = UISys::gHoverOwnerKey;

    if (!hovered.empty())
    {
        auto it = gTooltipMap.find(hovered);
        if (it != gTooltipMap.end())
        {
            if (hovered != gLastHovered)
            {
                gHoverStart = glfwGetTime();
                gLastHovered = hovered;
                gFadingText.clear();
            }

            double dt = glfwGetTime() - gHoverStart;
            float alpha = std::clamp((float)((dt - 0.4) / 1.0), 0.0f, 1.0f);
            if (alpha <= 0.0f) return;

            const char* text = it->second.c_str();
            float tw = uiMeasureText(text, 0.28f) + uiScaleX(32.0f);
            float th = uiScaleY(48.0f);
            float tx = (float)UISys::gFbW * 0.5f - tw * 0.5f;
            float ty = (float)UISys::gFbH - th - uiScaleY(60.0f);

            glm::vec4 bg{0.08f, 0.1f, 0.14f, alpha * 0.92f};
            uiDrawRect({tx, ty, tw, th}, bg, "tooltip-bg");
            glm::vec4 border{0.3f, 0.5f, 0.8f, alpha * 0.6f};
            uiDrawRectOutline({tx, ty, tw, th}, border, "tooltip-border");

            float tw2 = uiMeasureText(text, 0.28f);
            uiDrawText(text, tx + (tw - tw2) * 0.5f, ty + uiScaleY(12.0f), 0.28f,
                       {0.85f, 0.9f, 1.0f, alpha});
            return;
        }
    }

    if (!gFadingText.empty())
    {
        double dt = glfwGetTime() - gFadeStart;
        float alpha = 1.0f - std::clamp((float)(dt / 0.4), 0.0f, 1.0f);
        if (alpha <= 0.0f)
        {
            gFadingText.clear();
            return;
        }

        const char* text = gFadingText.c_str();
        float tw = uiMeasureText(text, 0.28f) + uiScaleX(32.0f);
        float th = uiScaleY(48.0f);
        float tx = (float)UISys::gFbW * 0.5f - tw * 0.5f;
        float ty = (float)UISys::gFbH - th - uiScaleY(60.0f);

        glm::vec4 bg{0.08f, 0.1f, 0.14f, alpha * 0.92f};
        uiDrawRect({tx, ty, tw, th}, bg, "tooltip-bg");
        glm::vec4 border{0.3f, 0.5f, 0.8f, alpha * 0.6f};
        uiDrawRectOutline({tx, ty, tw, th}, border, "tooltip-border");

        float tw2 = uiMeasureText(text, 0.28f);
        uiDrawText(text, tx + (tw - tw2) * 0.5f, ty + uiScaleY(12.0f), 0.28f,
                   {0.85f, 0.9f, 1.0f, alpha});
        return;
    }

    if (!gLastHovered.empty())
    {
        auto it = gTooltipMap.find(gLastHovered);
        if (it != gTooltipMap.end())
        {
            gFadingText = it->second;
            gFadeStart = glfwGetTime();
        }
        gLastHovered.clear();
    }
}
