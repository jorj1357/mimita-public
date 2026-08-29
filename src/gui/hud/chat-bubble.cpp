// 08 05 2026, 00 00
/* purpose
* Renders short-lived 3D chat bubbles above speaking players.
* Uses shared VIP name drawing so bubble sender labels match chat and nameplates.
* Keeps bubble lifetime, scale, and distance fade bounded for HUD readability.
* DOES NOT own chat history, packet parsing, or account entitlement verification.
* DOES NOT mutate player state or send chat packets.
* DOES NOT render the 2D chat window.
*/

#include "chat-bubble.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "audio/audio.h"
#include "vip/vip-name-render.h"
#include "network/net_common.h"
#include <GLFW/glfw3.h>

namespace
{
void wrapBubbleText(const std::string& text,
                    float maxWidth, float scale,
                    std::vector<std::string>& lines)
{
    lines.clear();
    std::istringstream words(text);
    std::string word;
    std::string current;
    while (words >> word)
    {
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && uiMeasureText(candidate.c_str(), scale) > maxWidth)
        {
            lines.push_back(current);
            current = word;
        }
        else
            current = candidate;
    }
    if (!current.empty())
        lines.push_back(current);
    if (lines.empty())
        lines.push_back("");
}
}

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
        const float maxTextW = 260.0f * scale;
        static thread_local std::vector<std::string> lines;
        wrapBubbleText(displayText, maxTextW, bubbleScale, lines);
        float textW = 0.0f;
        for (const auto& line : lines)
            textW = std::max(textW, uiMeasureText(line.c_str(), bubbleScale));
        VipNameDrawOptions nameOptions;
        nameOptions.scale = nameScale;
        nameOptions.alpha = fade;
        nameOptions.phase = 0.0f;
        nameOptions.detail = &player.vipStyleDetail;
        float nameW = vipMeasureStyledName(msg.senderName, player.vipAppearance, nameOptions);
        float bubbleW = std::max(textW, nameW) + bubblePadding * 2.0f;
        float bubbleH = lineHeight * ((float)lines.size() + 1.0f) + bubblePadding * 2.0f;

        float bx = screenX - bubbleW * 0.5f;
        float by = baseY - bubbleH;

        uiDrawRect({bx, by, bubbleW, bubbleH}, {0.05f, 0.05f, 0.08f, 0.82f * fade}, "chat-bubble-bg");
        uiDrawRectOutline({bx, by, bubbleW, bubbleH}, {0.35f, 0.4f, 0.5f, 0.6f * fade}, "chat-bubble-border");

        float nameX = screenX - nameW * 0.5f;
        vipDrawStyledName(msg.senderName, player.vipAppearance, nameX,
                          by + bubblePadding, nameOptions);

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            const float textLineW = uiMeasureText(lines[lineIndex].c_str(), bubbleScale);
            const float textX = screenX - textLineW * 0.5f;
            uiDrawText(lines[lineIndex].c_str(), textX,
                       by + bubblePadding + lineHeight * ((float)lineIndex + 1.0f),
                       bubbleScale, {1.0f, 1.0f, 1.0f, fade});
        }

        baseY = by - 4.0f * scale;
    }
}

void playChatSound(int messageLength)
{
    float pitch = computeChatPitch(messageLength);
    AudioManager::instance().play({"ui/chat/chat1", AudioCategory::UI, false, {}, 1.0f, pitch});
}

void renderTypingIndicator(const Player& player, const Camera& camera)
{
    if (!player.isTyping)
        return;

    const uint64_t elapsedMs = MimitaNet::nowMs() - player.typingStartedMs;
    if (elapsedMs > 5000)
        return;

    const glm::vec3 worldPos = playerHealthbarAnchor(player) + glm::vec3(0.0f, 0.0f, 0.5f);

    float screenX = 0.0f, screenY = 0.0f;
    if (!DebugVis::projectToScreen(camera, worldPos, screenX, screenY))
        return;

    const float distance = glm::length(camera.pos - worldPos);
    if (distance > 60.0f)
        return;

    const float scale = std::clamp(1.0f - distance / 60.0f, 0.35f, 1.0f);
    const float textScale = scale * 0.28f;

    const float time = (float)glfwGetTime();
    const int dotCount = ((int)(time * 2.0f) % 3) + 1;
    const std::string dots(dotCount, '.');

    const float textW = uiMeasureText(dots.c_str(), textScale);
    uiDrawText(dots.c_str(), screenX - textW * 0.5f, screenY, textScale,
               {1.0f, 1.0f, 1.0f, 0.8f});
}
