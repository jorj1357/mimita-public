// 09 13 2026
/* purpose
* Engine glue for creation/inspection mode: a fixed-tick look-at update and a
* per-render-frame overlay. All logic lives in Editor::CreationMode; this only
* connects the live camera/world and the UI draw pass.
* Does NOT own selection state or raycasting.
*/
#pragma once

struct World;
struct Camera;

// Runs from the fixed simulation tick (not the render frame).
void engineTickCreationUpdate(const World& world, const Camera& camera);

// Draws the always-on inspector overlay during the UI pass.
void engineRenderCreationOverlay(const Camera& camera);
