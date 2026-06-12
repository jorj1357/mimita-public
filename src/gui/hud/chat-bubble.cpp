#include "chat-bubble.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "audio/audio.h"

float computeChatDuration(int messageLength)
{
    return std::max(2.0f, (float)messageLength * 0.25f);
}

float computeChatPitch(int messageLength)
{
    float t = std::clamp((messageLength - 1) / 34.0f, 0.0f, 1.0f);
    return 1.4f - t * 0.7f;
}

void addChatMessage(ActorChatState& state, const std::string& text, const std::string& senderName)
{
    ChatMessage msg;
    msg.text = text;
    msg.senderName = senderName;
    msg.durationSeconds = computeChatDuration((int)text.size());
    msg.age = 0.0f;

    if ((int)state.activeMessages.size() >= ActorChatState::MAX_BUBBLES)
        state.activeMessages.pop_front();

    state.activeMessages.push_back(msg);
}

void updateChatBubbles(ActorChatState& state, float dt)
{
    for (auto it = state.activeMessages.begin(); it != state.activeMessages.end(); )
    {
        it->age += dt;
        if (it->age >= it->durationSeconds)
            it = state.activeMessages.erase(it);
        else
            ++it;
    }
}

static glm::vec3 getChatBubbleAnchor(const Player& player)
{
    return playerHealthbarAnchor(player) + glm::vec3(0.0f, 0.0f, 0.8f);
}

void renderChatBubbles(const ActorChatState& state, const Player& player, const Camera& camera)
{
    if (state.activeMessages.empty())
        return;

    const glm::vec3 baseWorld = getChatBubbleAnchor(player);

    float screenX = 0.0f, screenY = 0.0f;
    if (!DebugVis::projectToScreen(camera, baseWorld, screenX, screenY))
        return;

    const float distance = glm::length(camera.pos - baseWorld);
    constexpr float MAX_CHAT_DISTANCE = 60.0f;
    if (distance > MAX_CHAT_DISTANCE)
        return;

    float scale = std::clamp(1.0f - distance / MAX_CHAT_DISTANCE, 0.35f, 1.0f);
    const float bubbleScale = scale * 0.30f;
    const float nameScale = scale * 0.22f;
    const float lineHeight = 20.0f * scale;
    const float bubblePadding = 8.0f * scale;

    float baseY = screenY;

    for (int i = (int)state.activeMessages.size() - 1; i >= 0; --i)
    {
        const ChatMessage& msg = state.activeMessages[i];
        float fade = 1.0f;
        float fadeStart = msg.durationSeconds * 0.7f;
        if (msg.age > fadeStart)
            fade = 1.0f - (msg.age - fadeStart) / (msg.durationSeconds - fadeStart);
        fade = std::clamp(fade, 0.0f, 1.0f);

        std::string displayText = "\"" + msg.text + "\"";
        float textW = uiMeasureText(displayText.c_str(), bubbleScale);
        float nameW = uiMeasureText(msg.senderName.c_str(), nameScale);
        float bubbleW = std::max(textW, nameW) + bubblePadding * 2.0f;
        float bubbleH = lineHeight * 2.0f + bubblePadding * 2.0f;

        float bx = screenX - bubbleW * 0.5f;
        float by = baseY - bubbleH;

        uiDrawRect({bx, by, bubbleW, bubbleH}, {0.05f, 0.05f, 0.08f, 0.82f * fade}, "chat-bubble-bg");
        uiDrawRectOutline({bx, by, bubbleW, bubbleH}, {0.35f, 0.4f, 0.5f, 0.6f * fade}, "chat-bubble-border");

        float nameX = screenX - nameW * 0.5f;
        uiDrawText(msg.senderName.c_str(), nameX, by + bubblePadding, nameScale, {0.7f, 0.8f, 1.0f, fade});

        float textX = screenX - textW * 0.5f;
        uiDrawText(displayText.c_str(), textX, by + bubblePadding + lineHeight, bubbleScale, {1.0f, 1.0f, 1.0f, fade});

        baseY = by - 4.0f * scale;
    }
}

void playChatSound(int messageLength)
{
    float pitch = computeChatPitch(messageLength);
    AudioManager::instance().play({"ui/chat/chat1", AudioCategory::UI, false, {}, 1.0f, pitch});
}
