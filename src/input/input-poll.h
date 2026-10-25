// C:\important\quiet\n\mimita-priv-v7\src\input\input-poll.h
// feb 10 2026
/**
 * purpose
 * header for input polling
 * which is the rewrite so phsics is modular and buncehs 
 * off small files in src/phsics/movement
 */

#pragma once

#include <string>

struct GLFWwindow;
struct InputState;
struct InputFrame;
class Camera;

InputState pollInput(GLFWwindow* win, const Camera& cam);
InputFrame buildInputFrame(GLFWwindow* win, const Camera& cam);

extern InputFrame gTerminalInputOverride;
void consumeTerminalInputOverride();

// Correlates a terminal-input override (agent.action) with the request that
// requested it so the applied event can carry the same request id.
void noteTerminalInputOverrideRequestId(const std::string& requestId);

// Schedule deterministic camera-relative movement axes through the shared
// input boundary. moveX is side (-left/+right), moveY is forward
// (-back/+forward). The override expires automatically after fixed ticks.
// requestId (optional) is echoed by the agent.move.applied evidence so a
// queued command can be correlated with the fixed tick that applied it.
void scheduleTerminalMove(float moveX, float moveY, int durationTicks,
                          const std::string& requestId = std::string());
bool peekScheduledTerminalMove(float& moveX, float& moveY, bool* justStarted = nullptr);
bool consumeScheduledTerminalMove(float& moveX, float& moveY,
                                  bool* justStarted = nullptr,
                                  bool* justEnded = nullptr);
bool advanceScheduledTerminalMoveFixedTick(bool* justStarted = nullptr,
                                           bool* justEnded = nullptr);
bool consumeScheduledTerminalMoveForNetwork(float& moveX, float& moveY);
int scheduledTerminalMoveTotalTicks();
const std::string& scheduledTerminalMoveRequestId();
