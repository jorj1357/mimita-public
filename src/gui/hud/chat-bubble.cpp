// 08 05 2026, 00 00
/* purpose
* Renders short-lived 3D chat bubbles above speaking players.
* Uses shared VIP name drawing so bubble sender labels match chat and nameplates.
* Keeps bubble lifetime, scale, and distance fade bounded for HUD readability.
* Visuals are driven by config/gui/hud.json (chatBubble, typingIndicator).
* Fade semantics mirror config/healthbar.json: max/start/end distance and ticks.
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
#include "gui/gui-layout.h"
#include "gui/hud/ui-tick-clock.h"
#include "gui/ui-system.h"
#include "gui/hud/player-nameplates.h"
#include "audio/audio.h"
#include "vip/vip-name-render.h"
#include "network/net_common.h"
#include <GLFW/glfw3.h>

extern UiTickClock gChatUiTickClock;

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

// Distance fade copied from the healthbar pattern:
// 1.0 at/below start, linearly to 0.0 at end, with maxDistance as the cull.
float distanceFade(float distance, float start, float end)
{
    const float range = end - start;
    if (range <= 0.0f || distance <= start)
        return 1.0f;
    return std::clamp(1.0f - (distance - start) / range, 0.0f, 1.0f);
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

// ── Config readers (hud.json, hot-reloadable) ───────────────────────

TypingIndicatorConfig getTypingIndicatorConfig()
{
    TypingIndicatorConfig cfg;
    GuiLayout& hud = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
    const GuiElement* e = hud.get("typingIndicator");
    if (!e)
        return cfg;

    cfg.enabled = e->visible;
    cfg.showAboveHead = e->showAboveHead;
    cfg.showInChat = e->showInChat;
    cfg.showSelfAboveHead = e->showSelfAboveHead;
    cfg.showSelfInChat = e->showSelfInChat;
    if (e->fontSize > 0.0f) cfg.fontSize = e->fontSize;
    if (e->heightOffset != 0.0f) cfg.heightOffset = e->heightOffset;
    if (e->maxDistance > 0.0f) cfg.maxDistance = e->maxDistance;
    if (e->fadeStartDistance > 0.0f) cfg.fadeStartDistance = e->fadeStartDistance;
    if (e->fadeEndDistance > 0.0f) cfg.fadeEndDistance = e->fadeEndDistance;
    if (e->timeoutTicks > 0) cfg.timeoutTicks = e->timeoutTicks;
    if (e->fadeTicks > 0) cfg.fadeTicks = e->fadeTicks;
    if (e->blinkTicks > 0) cfg.blinkTicks = e->blinkTicks;
    if (e->heartbeatTicks > 0) cfg.heartbeatTicks = e->heartbeatTicks;
    cfg.textColor = e->getTextColorVec();
    return cfg;
}

ChatBubbleConfig getChatBubbleConfig()
{
    ChatBubbleConfig cfg;
    GuiLayout& hud = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
    const GuiElement* e = hud.get("chatBubble");
    if (!e)
        return cfg;

    cfg.enabled = e->visible;
    if (e->maxItems > 0) cfg.maxItems = e->maxItems;
    if (e->fontSize > 0.0f) cfg.fontSize = e->fontSize;
    if (e->nameFontSize > 0.0f) cfg.nameFontSize = e->nameFontSize;
    if (e->lineHeight > 0.0f) cfg.lineHeight = e->lineHeight;
    if (e->padding > 0.0f) cfg.padding = e->padding;
    if (e->maxWidth > 0.0f) cfg.maxWidth = e->maxWidth;
    if (e->heightOffset != 0.0f) cfg.heightOffset = e->heightOffset;
    if (e->maxDistance > 0.0f) cfg.maxDistance = e->maxDistance;
    if (e->fadeStartDistance > 0.0f) cfg.fadeStartDistance = e->fadeStartDistance;
    if (e->fadeEndDistance > 0.0f) cfg.fadeEndDistance = e->fadeEndDistance;
    if (e->durationBaseTicks > 0) cfg.durationBaseTicks = e->durationBaseTicks;
    if (e->durationPerCharTicks > 0) cfg.durationPerCharTicks = e->durationPerCharTicks;
    if (e->fadeTicks > 0) cfg.fadeTicks = e->fadeTicks;
    cfg.backgroundColor = e->getBackgroundColorVec();
    cfg.outlineColor = e->getOutlineColorVec();
    cfg.textColor = e->getTextColorVec();
    return cfg;
}

// ── Chat bubbles ────────────────────────────────────────────────────

void addChatMessage(ActorChatState& state, const std::string& text, const std::string& senderName)
{
    const ChatBubbleConfig cfg = getChatBubbleConfig();

    ChatMessage msg;
    msg.text = text;
    msg.senderName = senderName;
    msg.durationTicks = (float)std::max(cfg.durationBaseTicks,
                                        (int)text.size() * cfg.durationPerCharTicks);
    msg.ageTicks = 0.0f;

    const int maxItems = std::max(1, cfg.maxItems);
    if ((int)state.activeMessages.size() >= maxItems)
        state.activeMessages.pop_front();

    state.activeMessages.push_back(std::move(msg));
}

void updateChatBubbles(ActorChatState& state)
{
    // Called once per fixed 60 Hz combat tick, so age advances one tick/call.
    for (auto it = state.activeMessages.begin(); it != state.activeMessages.end(); )
    {
        it->ageTicks += 1.0f;
        if (it->durationTicks > 0.0f && it->ageTicks >= it->durationTicks)
            it = state.activeMessages.erase(it);
        else
            ++it;
    }
}

static glm::vec3 getChatBubbleAnchor(const Player& player, float heightOffset)
{
    return playerHealthbarAnchor(player) + glm::vec3(0.0f, 0.0f, heightOffset);
}

void renderChatBubbles(const ActorChatState& state, const Player& player, const Camera& camera)
{
    const ChatBubbleConfig cfg = getChatBubbleConfig();
    if (!cfg.enabled || state.activeMessages.empty())
        return;

    const glm::vec3 baseWorld = getChatBubbleAnchor(player, cfg.heightOffset);

    float screenX = 0.0f, screenY = 0.0f;
    if (!DebugVis::projectToScreen(camera, baseWorld, screenX, screenY))
        return;

    const float distance = glm::length(camera.pos - baseWorld);
    if (distance > cfg.maxDistance)
        return;

    const float distanceScale = std::clamp(1.0f - distance / cfg.maxDistance, 0.35f, 1.0f);
    const float distAlpha = distanceFade(distance, cfg.fadeStartDistance, cfg.fadeEndDistance);
    const float bubbleScale = distanceScale * cfg.fontSize;
    const float nameScale = distanceScale * cfg.nameFontSize;
    const float lineHeight = cfg.lineHeight * distanceScale;
    const float bubblePadding = cfg.padding * distanceScale;

    float baseY = screenY;

    for (int i = (int)state.activeMessages.size() - 1; i >= 0; --i)
    {
        const ChatMessage& msg = state.activeMessages[i];
        float fade = 1.0f;
        const float fadeStart = msg.durationTicks - (float)cfg.fadeTicks;
        if (cfg.fadeTicks > 0 && msg.ageTicks > fadeStart)
            fade = 1.0f - (msg.ageTicks - fadeStart) / (float)cfg.fadeTicks;
        fade = std::clamp(fade * distAlpha, 0.0f, 1.0f);

        std::string displayText = "\"" + msg.text + "\"";
        const float maxTextW = cfg.maxWidth * distanceScale;
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

        glm::vec4 bg = cfg.backgroundColor;
        bg.a *= fade;
        glm::vec4 border = cfg.outlineColor;
        border.a *= fade;
        glm::vec4 textColor = cfg.textColor;
        textColor.a *= fade;

        uiDrawRect({bx, by, bubbleW, bubbleH}, bg, "chat-bubble-bg");
        uiDrawRectOutline({bx, by, bubbleW, bubbleH}, border, "chat-bubble-border");

        float nameX = screenX - nameW * 0.5f;
        vipDrawStyledName(msg.senderName, player.vipAppearance, nameX,
                          by + bubblePadding, nameOptions);

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            const float textLineW = uiMeasureText(lines[lineIndex].c_str(), bubbleScale);
            const float textX = screenX - textLineW * 0.5f;
            uiDrawText(lines[lineIndex].c_str(), textX,
                       by + bubblePadding + lineHeight * ((float)lineIndex + 1.0f),
                       bubbleScale, textColor);
        }

        baseY = by - 4.0f * distanceScale;
    }
}

void playChatSound(int messageLength)
{
    float pitch = computeChatPitch(messageLength);
    AudioManager::instance().play({"ui/chat/chat1", AudioCategory::UI, false, {}, 1.0f, pitch});
}

void renderTypingIndicator(const Player& player, const Camera& camera, bool isLocal)
{
    const TypingIndicatorConfig cfg = getTypingIndicatorConfig();
    if (!cfg.enabled || !cfg.showAboveHead)
        return;
    if (!player.isTyping)
        return;
    if (isLocal && !cfg.showSelfAboveHead)
        return;

    const uint64_t elapsedMs = MimitaNet::nowMs() - player.typingStartedMs;
    const uint64_t timeoutMs = (uint64_t)cfg.timeoutTicks * 1000ull / 60ull;
    if (elapsedMs > timeoutMs)
        return;

    // Fade out over the final fadeTicks of the timeout.
    float timeFade = 1.0f;
    const uint64_t fadeMs = (uint64_t)cfg.fadeTicks * 1000ull / 60ull;
    if (fadeMs > 0 && timeoutMs > fadeMs && elapsedMs > timeoutMs - fadeMs)
        timeFade = (float)(timeoutMs - elapsedMs) / (float)fadeMs;

    const glm::vec3 worldPos = getChatBubbleAnchor(player, cfg.heightOffset);

    float screenX = 0.0f, screenY = 0.0f;
    if (!DebugVis::projectToScreen(camera, worldPos, screenX, screenY))
        return;

    const float distance = glm::length(camera.pos - worldPos);
    if (distance > cfg.maxDistance)
        return;

    const float distanceScale = std::clamp(1.0f - distance / cfg.maxDistance, 0.35f, 1.0f);
    const float distAlpha = distanceFade(distance, cfg.fadeStartDistance, cfg.fadeEndDistance);
    const float textScale = distanceScale * cfg.fontSize;

    // Frame-independent blink driven by the fixed 60 Hz UI tick clock.
    const uint64_t blinkTicks = (uint64_t)std::max(1, cfg.blinkTicks);
    const int dotCount = (int)((gChatUiTickClock.getTick() / blinkTicks) % 3ull) + 1;
    const std::string dots((size_t)dotCount, '.');

    glm::vec4 color = cfg.textColor;
    color.a *= std::clamp(timeFade * distAlpha, 0.0f, 1.0f);

    const float textW = uiMeasureText(dots.c_str(), textScale);
    uiDrawText(dots.c_str(), screenX - textW * 0.5f, screenY, textScale, color);
}
