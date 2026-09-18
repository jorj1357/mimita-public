// 09 17 2026
/* purpose
* Actor-movement compatibility shim. The single movement tuning table now lives
* in hot-movement-presets.h (source/default/heavy/retrograd_fast/counterstrike),
* shared by local prediction and the server actor system. This header only keeps
* the shared constants hot actor systems still reference.
* Plain numbers only; no cold structs, no entity-type branches.
* Does NOT own collision, storage, or the network transport.
*/
#pragma once

#include "hot-reload/hot-movement-presets.h"

namespace MimitaHotMovement {

inline constexpr float kRadPerDegree = 0.01745329252f;

} // namespace MimitaHotMovement
