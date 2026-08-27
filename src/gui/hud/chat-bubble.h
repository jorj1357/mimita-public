#pragma once

#include <string>
#include <deque>
#include <glm/glm.hpp>

struct Player;
struct Camera;

struct ChatMessage {
    std::string text;
    std::string senderName;
    float durationSeconds;
    float age;
};

struct ActorChatState {
    std::deque<ChatMessage> activeMessages;
    static constexpr int MAX_BUBBLES = 3;
};

float computeChatDuration(int messageLength);
float computeChatPitch(int messageLength);
void addChatMessage(ActorChatState& state, const std::string& text, const std::string& senderName);
void updateChatBubbles(ActorChatState& state, float dt);
void renderChatBubbles(const ActorChatState& state, const Player& player, const Camera& camera);
void renderTypingIndicator(const Player& player, const Camera& camera);
void playChatSound(int messageLength);
