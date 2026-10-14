// C:\important\quiet\n\mimita-priv-v7\src\input\input-poll.h
// feb 10 2026
/**
 * purpose
 * header for input polling
 * which is the rewrite so phsics is modular and buncehs 
 * off small files in src/phsics/movement
 */

#pragma once

struct GLFWwindow;
struct InputState;
struct InputFrame;
class Camera;

InputState pollInput(GLFWwindow* win, const Camera& cam);
InputFrame buildInputFrame(GLFWwindow* win, const Camera& cam);

extern InputFrame gTerminalInputOverride;
void consumeTerminalInputOverride();

// Schedule deterministic camera-relative movement axes through the shared
// input boundary. moveX is side (-left/+right), moveY is forward
// (-back/+forward). The override expires automatically after fixed ticks.
void scheduleTerminalMove(float moveX, float moveY, int durationTicks);
bool peekScheduledTerminalMove(float& moveX, float& moveY);
bool consumeScheduledTerminalMove(float& moveX, float& moveY);
