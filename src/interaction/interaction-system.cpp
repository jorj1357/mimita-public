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

InteractionActionRegistry& InteractionActionRegistry::instance()
{
    static InteractionActionRegistry registry;
    return registry;
}

bool InteractionActionRegistry::registerAction(const std::string& id,
                                               WorldInteractionHandler handler)
{
    if (id.empty() || !handler) return false;
    mActions[id] = std::move(handler);
    return true;
}

void InteractionActionRegistry::unregisterAction(const std::string& id)
{
    mActions.erase(id);
}

bool InteractionActionRegistry::has(const std::string& id) const
{
    return mActions.find(id) != mActions.end();
}

bool InteractionActionRegistry::invoke(const std::string& id,
                                       const InteractionContext& context,
                                       const std::string& targetId) const
{
    const auto it = mActions.find(id);
    return it != mActions.end() && it->second && it->second(context, targetId);
}

std::vector<std::string> InteractionActionRegistry::actionIds() const
{
    std::vector<std::string> ids;
    ids.reserve(mActions.size());
    for (const auto& entry : mActions)
        ids.push_back(entry.first);
    return ids;
}

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
    if (mHoldActive && mHoldTargetId == id)
        cancelHold("target-removed");
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
    mHoldActive = false;
    mHoldTargetId = 0;
    mHoldElapsedTicks = 0;
    mHoldKeyDown = false;
    mHoldFromCommand = false;
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
    mHoldKeyDown = held;

    const InteractionTarget* target = selected(camera);
    if (mHoldActive)
    {
        // A runtime/scenario hold is advanced by tickHold; the physical F key
        // state must not cancel it on the next input poll.
        if (!mHoldFromCommand && !held)
        {
            cancelHold("released");
            return false;
        }
        if (!target || target->id != mHoldTargetId)
        {
            cancelHold("target-changed");
            return false;
        }
        return true;
    }

    if (!pressed)
        return false;

    if (target && target->activate &&
        target->inputMode == InteractionInputMode::Hold && target->holdTicks > 0)
    {
        mHoldActive = true;
        mHoldFromCommand = false;
        mHoldTargetId = target->id;
        mHoldElapsedTicks = 0;
        for (InteractionTarget& item : mTargets)
        {
            if (item.id == target->id)
            {
                item.holdProgress = 0.0f;
                break;
            }
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "interaction.hold.started", target->owner,
            "F hold interaction started", 0,
            {{"targetId", target->id},
             {"owner", target->owner},
             {"label", target->label},
             {"holdTicks", target->holdTicks}},
            __FILE__, __LINE__, __func__);
        return true;
    }

    const bool activated = target && target->activate
                               ? target->activate(InteractionContext{&camera})
                               : false;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "interaction.key", target ? target->owner : "interaction-key",
        "F interaction key edge processed", 0,
        {{"held", held},
         {"selected", target != nullptr},
         {"target_id", target ? target->id : 0},
         {"target_owner", target ? target->owner : std::string()},
         {"target_label", target ? target->label : std::string()},
         {"activated", activated},
         {"player_pos", {THE_PLAYER.pos.x, THE_PLAYER.pos.y,
                         THE_PLAYER.pos.z}},
         {"camera_pos", {camera.pos.x, camera.pos.y, camera.pos.z}},
         {"sticky_target", mStickyTarget}},
        __FILE__, __LINE__, __func__);
    if (target && activated)
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "interaction.activated", target->owner,
            "interaction action executed", 0,
            {{"targetId", target->id},
             {"owner", target->owner},
             {"label", target->label},
             {"activated", true}},
            __FILE__, __LINE__, __func__);
    return activated;
}

bool InteractionSystem::beginHold(uint32_t targetId, const Camera& camera)
{
    const InteractionTarget* target = selected(camera);
    if (!target || target->id != targetId || !target->activate ||
        target->inputMode != InteractionInputMode::Hold || target->holdTicks <= 0)
        return false;
    mHoldActive = true;
    mHoldFromCommand = true;
    mHoldTargetId = targetId;
    mHoldElapsedTicks = 0;
    for (InteractionTarget& item : mTargets)
        if (item.id == targetId) item.holdProgress = 0.0f;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "interaction.hold.started", target->owner,
        "F hold interaction started", 0,
        {{"targetId", target->id}, {"owner", target->owner},
         {"label", target->label}, {"holdTicks", target->holdTicks},
         {"source", "runtime_command"}},
        __FILE__, __LINE__, __func__);
    return true;
}

void InteractionSystem::releaseHold()
{
    cancelHold("released");
}

void InteractionSystem::cancelHold(const char* reason)
{
    if (!mHoldActive)
        return;
    const uint32_t targetId = mHoldTargetId;
    for (InteractionTarget& item : mTargets)
    {
        if (item.id == mHoldTargetId)
        {
            item.holdProgress = 0.0f;
            break;
        }
    }
    mHoldActive = false;
    mHoldFromCommand = false;
    mHoldTargetId = 0;
    mHoldElapsedTicks = 0;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "interaction.hold.cancelled", std::to_string(targetId),
        "F hold interaction cancelled", 0,
        {{"targetId", targetId}, {"reason", reason}},
        __FILE__, __LINE__, __func__);
}

void InteractionSystem::tickHold(const Camera& camera)
{
    if (!mHoldActive)
        return;

    const InteractionTarget* target = selected(camera);
    if (!target || target->id != mHoldTargetId || !target->enabled)
    {
        cancelHold("range-lost");
        return;
    }

    ++mHoldElapsedTicks;
    const int holdTicks = std::max(1, target->holdTicks);
    const float progress = std::min(
        1.0f, static_cast<float>(mHoldElapsedTicks) /
                  static_cast<float>(holdTicks));
    for (InteractionTarget& item : mTargets)
    {
        if (item.id == mHoldTargetId)
        {
            item.holdProgress = progress;
            break;
        }
    }

    if (mHoldElapsedTicks < holdTicks)
        return;

    const uint32_t targetId = mHoldTargetId;
    const std::string owner = target->owner;
    const std::string label = target->label;
    const bool activated = target->activate(InteractionContext{&camera});
    for (InteractionTarget& item : mTargets)
    {
        if (item.id == mHoldTargetId)
        {
            item.holdProgress = 1.0f;
            break;
        }
    }
    mHoldActive = false;
    mHoldFromCommand = false;
    mHoldTargetId = 0;
    mHoldElapsedTicks = 0;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "interaction.hold.completed", owner,
        "F hold interaction completed", 0,
        {{"targetId", targetId},
         {"owner", owner},
         {"label", label},
         {"activated", activated}},
        __FILE__, __LINE__, __func__);
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

    std::string text = (target->inputMode == InteractionInputMode::Hold
                            ? "Hold F to "
                            : "Press F to ") + target->label;
    if (target->inputMode == InteractionInputMode::Hold)
    {
        const int percent = static_cast<int>(
            std::clamp(target->holdProgress, 0.0f, 1.0f) * 100.0f);
        text += "  " + std::to_string(percent) + "%";
    }
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
