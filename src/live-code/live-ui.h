// 09 14 2026
/* purpose
* Kernel-side buffer for generic HUD/UI commands emitted by hot ui.frame
* systems. The kernel owns the low-level draw (text/rect/bar/image); hot code
* owns what widgets exist and their layout. No gamemode-specific UI type.
* Does NOT own gameplay state or font rasterization.
*/
#pragma once

#include <cstddef>

#include "hot-reload/game-api.h"

namespace LiveUi {

// Clear the frame buffer at the start of the ui.frame domain run.
void beginFrame();

// Hot ui.frame systems submit widgets through the render.ui capability.
void submit(const GameUiCommandV1& command);

// Draw the buffered commands with the immediate-mode UI backend and clear.
// Sets hotOwnsHud() based on whether anything was emitted this frame.
void endFrameAndDraw();

// Number of commands buffered for the current frame.
std::size_t commandCount();

// True when the last completed ui.frame run emitted commands, so the cold
// composition should yield ownership of the migrated HUD regions.
bool hotOwnsHud();

// Generic UI interaction mechanism. Hot systems emit GAME_UI_BUTTON widgets with
// logical element ids; the backend hit-tests them and reports a generic
// GAME_EVENT_UI_ACTION. Generation-safe: only element ids are stored, never hot
// function pointers.
// Returns true when a hot handler consumed the click (the cold legacy UI for
// that element must not also act).
bool handlePointerClick(float x, float y, std::uint64_t tick);
// Number of interactive widgets emitted in the last completed frame.
std::size_t buttonCount();

// True when hot UI policy owns the given screen (screenId 0 = any screen), so
// the cold legacy composition for that screen must yield (one owner).
bool hotOwnsScreen(std::uint64_t screenId);

// Resolve a UI image logical resource id to the current generation handle (0 if
// unknown). Exposed for headless verification of generation-aware UI resources.
std::uint64_t resolveUiImageHandle(std::uint64_t resourceId);

} // namespace LiveUi
