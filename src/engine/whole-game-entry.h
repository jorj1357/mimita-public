// 2026-11-13
/* purpose
* Own the whole-game generation's tick/render entry.
* The host calls tickWholeGame from GameModuleV1::tick and renderWholeGame from
* GameModuleV1::render. The generation owns an Engine that borrows the resident
* Renderer through CAP_RENDER_DEVICE; it never constructs or destroys the device.
* Does NOT change the host loop, fixed 60 Hz rules, or VSync.
*/
#pragma once

namespace mimita::game {

// Acquire the resident renderer and prepare the generation-owned Engine.
// Returns false when the host did not provide a usable device.
bool initializeWholeGameEntry(int width, int height, const char* title);

// Run the simulation half of one host frame. Fixed 60 Hz stepping happens
// inside the engine replay phase and is unchanged.
void tickWholeGame(double deltaSeconds);

// Run the render half of the same frame. renderContext is the host Engine*
// supplied by the host loop and is reserved for future render-context state.
void renderWholeGame(void* renderContext);

// Release generation-owned state only. The resident device stays alive.
bool wholeGameShouldClose();

void shutdownWholeGameEntry();

}  // namespace mimita::game
