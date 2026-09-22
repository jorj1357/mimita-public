// 09 22 2026
/* purpose
* Cold-side access to the single per-instance tool state (ToolInstanceStateV1).
* When a tool has hot state, cold code must read/report it and must NOT write
* the legacy WeaponToolState component: hot state is the one owner of ammo,
* cooldown, and reload for that tool.
* Does NOT own the state math (that lives in hot-tool-state.h) or networking.
*/
#pragma once

#include <cstdint>

#include "hot-reload/hot-tool-state.h"

namespace MimitaNet {

bool serverHotToolStateHas(std::uint64_t toolEntity);
bool serverHotToolStateRead(std::uint64_t toolEntity, ToolInstanceStateV1& out);
bool serverHotToolStateWrite(std::uint64_t toolEntity,
                             const ToolInstanceStateV1& in);

} // namespace MimitaNet
