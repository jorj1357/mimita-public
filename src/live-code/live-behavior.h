// 09 12 2026
/* purpose
* EXE-side generic behavior bridge. Dispatches GameEventV1 events to the active
* hot gameplay module and reports whether a behavior handled them.
* The kernel owns the payload; hot code only reads/writes plain data.
* Does NOT own damage, authority, or networking.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace LiveBehavior {

// Dispatch a GAME_EVENT_DAMAGE_POLICY event. Returns true when a hot behavior
// handled it (payload.handled set). `payload` is filled by the caller with base
// values and may be overridden by the behavior.
bool dispatchDamagePolicy(DamagePolicyV1& payload, std::uint64_t tick);

// Dispatch an arbitrary event to the active behavior table.
bool dispatchEvent(const GameEventV1& event, std::uint64_t tick);

// Kernel event queue used by the emitEvent capability. Bounded and FIFO.
void enqueueEvent(const GameEventV1& event);
int drainEvents(int maxEvents);

// True when the active gameplay module provides an event handler.
bool available();

} // namespace LiveBehavior
