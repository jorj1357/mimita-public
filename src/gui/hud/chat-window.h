// 07 30 2026, 13 26
/* purpose
* Renders the 2D chat window at bottom-left of the screen.
* Owns the text input state, scroll state, fade timer, and message display.
* Loads position/size/font from JSON config (config/gui/chat-hud.json).
* Uses UiTickClock for fade timing indepependent of render FPS.
* Does NOT own ChatHistory, network state, or 3D chat bubble rendering.
* Does NOT handle the "/" key or mouse lock — those are wired in engine ticks.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "gui/ui-system.h"
#include "gui/ui-text-input.h"
#include "gui/hud/ui-tick-clock.h"

struct ChatHistory;
class GLFWwindow;

// How many ui-ticks the background holds at full opacity after a new message
constexpr uint64_t CHAT_FADE_HOLD_TICKS = 900;
// How many ui-ticks the fade from full to 0% takes
constexpr uint64_t CHAT_FADE_DURATION_TICKS = 300;

struct ChatWindowState {
    bool open = false;               // chat input is actively open
    bool mouseUnlocked = false;      // mouse was unlocked for interaction
    uint64_t lastMessageUiTick = 0;   // ui tick when last message arrived (for fade)
    float backgroundOpacity = 1.0f;   // current bg opacity (0..1)
    bool hovered = false;             // mouse is hovering the chat window
    bool scrolledUp = false;          // player manually scrolled up
    uint64_t newMessageCount = 0;     // messages received while scrolled up

    UITextInputState textInput;
    UIScrollState scroll;
};

void initChatWindowState(ChatWindowState& state);
void renderChatWindow(ChatWindowState& state, GLFWwindow* win,
                      const ChatHistory& history, const UiTickClock& clock,
                      float screenW, float screenH);
bool handleChatWindowKey(ChatWindowState& state, int key, int action, int mods,
                         GLFWwindow* win, const UiTickClock& clock,
                         std::string& outMessage);
void handleChatWindowChar(ChatWindowState& state, unsigned int codepoint);
void openChatWindow(ChatWindowState& state);
void closeChatWindow(ChatWindowState& state);
void setChatMouseUnlocked(ChatWindowState& state, bool unlocked);

// Accessible from preferences toggle
extern bool gChatWindowVisible;

// Global chat window state (defined in chat-window.cpp)
extern ChatWindowState gChatWindowState;
extern UiTickClock gChatUiTickClock;

// Check if chat is currently open (for mouse lock logic)
bool isChatOpen();
