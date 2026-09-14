// 09 13 2026
/* purpose
* EXE-side bridge to the hot editor module. Owns the ABI-safe capability
* functions (spatial query, inspection snapshot, UI/world draw primitives, map
* info, fork editing) and the per-tick / per-frame invocation. All selection,
* inspection formatting, layout, and edit policy lives in the hot module.
* Does NOT own selection policy, inspector formatting, or overlay layout.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

struct World;

namespace LiveEditor {

bool available();

// Per fixed simulation tick. Polls editor input edges and invokes the hot
// module. Returns true when the hot module handled it.
bool tick(const World& world, const EditorStateV1& state, EditorResultV1& out);

// Per render/UI frame. Returns true when the hot module drew the overlay.
bool drawOverlay();

const EditorStateV1& lastState();
const EditorResultV1& lastResult();

} // namespace LiveEditor
