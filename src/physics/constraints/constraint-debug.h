// 2026-10-27
/* purpose
* Draw generalized constraint/attachment anchors, axes, limits, and labels.
* Visible only while entity_visibility is on (the existing authoring/debug gate).
* Does NOT solve constraints or own their state.
*/
#pragma once

class Camera;

void drawConstraintDebug(const Camera& camera);
