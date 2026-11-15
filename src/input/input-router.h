// 2026-11-15
// Single hot input owner for all local input sources.
// Adapters feed normalized key/pointer/text/focus state; gameplay and UI read
// actions or pointer state from this owner only.
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct GLFWwindow;
class Camera;

namespace InputRouter {

enum class Source : std::uint32_t { Unknown, Host, Win32, Agent, Scenario };

void setWindow(GLFWwindow* window);
GLFWwindow* window();
void beginFrame(std::uint64_t clientTick, std::uint64_t serverTick);
void ingestKey(int key, int scancode, int action, int mods, Source source);
void ingestMouseButton(int button, int action, Source source);
void ingestCursor(double x, double y, Source source);
void ingestText(unsigned int codepoint, Source source);
void ingestFocus(bool focused, Source source);
void ingestLookDelta(float dx, float dy, Source source);

bool keyDown(int key);
bool mouseDown(int button);
bool focused();
bool hasHostKeys();
glm::vec2 lookDelta();
bool cursorPos(GLFWwindow* window, double* x, double* y);
double cursorX();
double cursorY();
std::uint64_t frameId();
std::uint64_t clientTick();
std::uint64_t serverTick();

void applyPendingLook(Camera& camera);
bool gameWindowFocused();
bool focusGameWindow();
void setPointerLock(bool locked);
bool pointerLocked();
bool hostCursorActive();
int lookSource();

}  // namespace InputRouter
