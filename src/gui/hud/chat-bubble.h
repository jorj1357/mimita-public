#pragma once

#include <string>
#include <deque>
#include <glm/glm.hpp>

struct Player;
struct Camera;

struct ChatMessage {
    std::string text;
    std::string senderName;
    float durationTicks;
    float ageTicks;
};

struct ActorChatState {
    std::deque<ChatMessage> activeMessages;
    static constexpr int MAX_BUBBLES = 3;
};

// ── Config-driven chat visuals (config/gui/hud.json) ────────────────
// Fade semantics mirror config/healthbar.json: max/start/end distance and
// tick-based timing. Read each frame so hud.json hot reloads.
struct TypingIndicatorConfig {
    bool enabled = true;
    bool showAboveHead = true;
    bool showInChat = true;
    bool showSelfAboveHead = true;
    bool showSelfInChat = true;
    float fontSize = 0.28f;
    float heightOffset = 0.5f;
    float maxDistance = 60.0f;
    float fadeStartDistance = 20.0f;
    float fadeEndDistance = 60.0f;
    int timeoutTicks = 300;
    int fadeTicks = 30;
    int blinkTicks = 30;
    int heartbeatTicks = 120;
    glm::vec4 textColor{1.0f, 1.0f, 1.0f, 0.8f};
};

struct ChatBubbleConfig {
    bool enabled = true;
    int maxItems = 3;
    float fontSize = 0.30f;
    float nameFontSize = 0.22f;
    float lineHeight = 20.0f;
    float padding = 8.0f;
    float maxWidth = 260.0f;
    float heightOffset = 0.8f;
    float maxDistance = 60.0f;
    float fadeStartDistance = 20.0f;
    float fadeEndDistance = 60.0f;
    int durationBaseTicks = 120;
    int durationPerCharTicks = 15;
    int fadeTicks = 45;
    glm::vec4 backgroundColor{0.05f, 0.05f, 0.08f, 0.82f};
    glm::vec4 outlineColor{0.35f, 0.4f, 0.5f, 0.6f};
    glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
};

TypingIndicatorConfig getTypingIndicatorConfig();
ChatBubbleConfig getChatBubbleConfig();

float computeChatDuration(int messageLength);
float computeChatPitch(int messageLength);
void addChatMessage(ActorChatState& state, const std::string& text, const std::string& senderName);
void updateChatBubbles(ActorChatState& state);
void renderChatBubbles(const ActorChatState& state, const Player& player, const Camera& camera);
void renderTypingIndicator(const Player& player, const Camera& camera, bool isLocal = false);
void playChatSound(int messageLength);
