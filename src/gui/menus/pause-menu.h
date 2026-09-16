// 08 22 2026, 12 00
/* purpose
* Owns the in-game ESC modal and its mode-aware return actions.
* Draws the hot-reloadable pause layout and routes its modal input.
* Reuses the existing settings, duel history, queue, and notification owners.
* Does NOT own gameplay simulation, networking transport, or settings values.
* Does NOT create a second settings implementation or persist room data itself.
* Does NOT render when the player is not in an active game.
*/

#pragma once

#include <cstdint>

struct GLFWwindow;

namespace PauseMenu {

bool isOpen();
void toggle(GLFWwindow* window);
void close(GLFWwindow* window);
void handleKey(GLFWwindow* window, int key, int action);
void render(GLFWwindow* window);

// Generic bridge for hot UI: the active view as a logical hash
// (gameHash("pause.main") etc.), and a logical action dispatcher mapping
// gameHash("pause.resume"/"pause.settings"/...) to the cold mechanism.
// Returns false for an unknown id.
std::uint64_t viewHash();
bool requestAction(GLFWwindow* window, std::uint64_t actionId);

} // namespace PauseMenu
