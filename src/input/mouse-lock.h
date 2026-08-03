// 08 03 2026, 15 00
/* purpose
* Owns the gameplay mouse-lock state (single source of truth).
* The cursor starts locked during gameplay; pressing L unlocks it so the
* player can click on-screen notifications, and pressing L again re-locks.
* Provides locked(), set(), and toggle() that also apply the GLFW cursor mode.
* Does NOT own terminal/chat cursor handling or any other input systems.
* Does NOT fire weapons, render UI, or read the game state.
*/
#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace MouseLock {

// Whether gameplay cursor lock is currently enabled (default true).
bool locked();

// Set the gameplay mouse-lock state and apply the cursor mode immediately.
void set(GLFWwindow* win, bool on);

// Flip the state and apply the cursor mode immediately.
void toggle(GLFWwindow* win);

} // namespace MouseLock
