// 2026-10-27

#include "interaction/interaction-system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "debug/debug-visuals.h"
#include "debug/structured-log.h"
#include "gui/ui-system.h"
#include "terminal/terminal-state.h"

namespace {

constexpr float kMaxPromptDistance = 30.0f;
constexpr float kMinInteractionRadius = 1.0f;
constexpr float kBaseFont = 0.62f;
constexpr float kMinFont = 0.50f;
constexpr float kMaxFont = 1.05f;

} // namespace

InteractionSystem& InteractionSystem::instance()
{
    static InteractionSystem system;
    return system;
}

uint32_t InteractionSystem::add(InteractionTarget target)
{
    target.id = mNextId++;
    target.radius = std::max(target.radius, kMinInteractionRadius);
    mTargets.push_back(std::move(target));
    return mTargets.back().id;
}

void InteractionSystem::remove(uint32_t id)
{
    mTargets.erase(
        std::remove_if(mTargets.begin(), mTargets.end(),
                       [id](const InteractionTarget& t) { return t.id == id; }),
        mTargets.end());
}

void InteractionSystem::clearOwner(const std::string& owner)
{
    mTargets.erase(
        std::remove_if(mTargets.begin(), mTargets.end(),
                       [&owner](const InteractionTarget& t) {
                           return t.owner == owner;
                       }),
        mTargets.end());
}

void InteractionSystem::clear()
{
    mTargets.clear();
    mNextId = 1;
    mPreviousKeyHeld = false;
    mLastPromptTarget = 0;
    mStickyTarget = 0;
    mMissingFrames = 0;
}

void InteractionSystem::updateTarget(uint32_t id,
                                     const glm::vec3& promptOrigin,
                                     const glm::vec3& interactOrigin,
                                     const std::string& label, float radius,
                                     bool enabled)
{
    for (InteractionTarget& target : mTargets)
    {
        if (target.id != id)
            continue;
        target.promptOrigin = promptOrigin;
        target.interactOrigin = interactOrigin;
        target.label = label;
        target.radius = std::max(radius, kMinInteractionRadius);
        target.enabled = enabled;
        return;
    }
}

const InteractionTarget* InteractionSystem::selected(
    const Camera& camera) const
{
    if (mStickyTarget != 0)
    {
        for (const InteractionTarget& target : mTargets)
            if (target.id == mStickyTarget && target.enabled && target.activate)
                return &target;
    }

    const glm::vec3 playerPos = THE_PLAYER.pos;
    const InteractionTarget* best = nullptr;
    float bestScore = -1.0e30f;

    for (const InteractionTarget& target : mTargets)
    {
        if (!target.enabled || !target.activate)
            continue;
        const float distance = glm::length(playerPos - target.interactOrigin);
        if (distance > std::max(target.radius, kMinInteractionRadius))
            continue;

        glm::vec3 toTarget = target.interactOrigin - camera.pos;
        const float aimLength = glm::length(toTarget);
        float aim = -1.0f;
        if (aimLength > 0.001f)
            aim = glm::dot(toTarget / aimLength, camera.front);

        const float score = static_cast<float>(target.priority) * 1000.0f +
                            aim * 25.0f - distance;
        if (score > bestScore)
        {
            bestScore = score;
            best = &target;
        }
    }
    return best;
}

bool InteractionSystem::activateSelected(const Camera& camera)
{
    const InteractionTarget* target = selected(camera);
    if (!target)
        return false;

    const InteractionContext context{&camera};
    const bool activated = target->activate(context);

    nlohmann::json fields;
    fields["targetId"] = target->id;
    fields["owner"] = target->owner;
    fields["label"] = target->label;
    fields["activated"] = activated;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "interaction.activated", target->owner,
        "interaction action executed", 0, fields, __FILE__, __LINE__,
        __func__);
    return activated;
}

bool InteractionSystem::handleKey(GLFWwindow* window, const Camera& camera)
{
    if (!window)
        return false;

    const bool held = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    const bool pressed = held && !mPreviousKeyHeld;
    mPreviousKeyHeld = held;
    if (!pressed)
        return false;

    return activateSelected(camera);
}

void InteractionSystem::draw(const Camera& camera)
{
    const InteractionTarget* target = selected(camera);
    if (!target)
    {
        ++mMissingFrames;
        if (mLastPromptTarget != 0 && mMissingFrames > 30)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "interaction.prompt.hidden",
                std::to_string(mLastPromptTarget),
                "no interaction target in range", 0,
                nlohmann::json::object(), __FILE__, __LINE__, __func__);
            mLastPromptTarget = 0;
        }
        return;
    }
    mMissingFrames = 0;

    if (mLastPromptTarget != target->id)
    {
        mLastPromptTarget = target->id;
        nlohmann::json fields;
        fields["targetId"] = target->id;
        fields["owner"] = target->owner;
        fields["label"] = target->label;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "interaction.prompt.shown", target->owner,
            "interaction prompt became visible", 0, fields, __FILE__,
            __LINE__, __func__);
    }

    float sx = 0.0f;
    float sy = 0.0f;
    if (!DebugVis::projectToScreen(camera, target->promptOrigin, sx, sy))
        return;

    const float distance = glm::length(camera.pos - target->promptOrigin);
    if (distance > kMaxPromptDistance)
        return;

    const float font = std::clamp(
        kBaseFont * (1.0f + (kMaxPromptDistance - distance) /
                                kMaxPromptDistance),
        kMinFont, kMaxFont);

    const std::string text = "Press F to " + target->label;
    const float textWidth = uiMeasureText(text.c_str(), font);
    const float textHeight = font * 16.0f;
    const float keySize = textHeight + font * 10.0f;
    const float gap = font * 14.0f;
    const float padX = font * 16.0f;
    const float padY = font * 10.0f;

    const float width = keySize + gap + textWidth + padX * 2.0f;
    const float height = std::max(keySize, textHeight) + padY * 2.0f;
    const float x = sx - width * 0.5f;
    const float y = sy - height - font * 20.0f;

    const UIRect background{x, y, width, height};
    uiDrawRect(background, {0.03f, 0.04f, 0.06f, 0.92f}, "interaction.bg");
    uiDrawRectOutline(background, {1.0f, 0.82f, 0.22f, 1.0f},
                      "interaction.border");

    const UIRect key{x + padX, y + (height - keySize) * 0.5f, keySize,
                     keySize};
    uiDrawRect(key, {0.95f, 0.78f, 0.18f, 1.0f}, "interaction.key");
    uiDrawRectOutline(key, {1.0f, 1.0f, 1.0f, 1.0f}, "interaction.key.border");
    const char* keyText = "F";
    const float keyTextWidth = uiMeasureText(keyText, font * 1.1f);
    uiDrawText(keyText, key.x + (key.w - keyTextWidth) * 0.5f,
               key.y + (key.h - font * 16.0f) * 0.5f, font * 1.1f,
               {0.05f, 0.05f, 0.05f, 1.0f});

    uiDrawText(text.c_str(), x + padX + keySize + gap,
               y + (height - textHeight) * 0.5f, font,
               {1.0f, 1.0f, 1.0f, 1.0f});
}
