// 2026-10-27
/* purpose
* One lifecycle owner for runtime-only physical world objects. Stopping a
* server, changing maps, returning to the menu, or resetting a network session
* must not leave vehicles, constraints, grenades, corpses, or props behind.
* Runtime objects are never persisted to map JSON.
* Does NOT own simulation or rendering.
*/
#pragma once

#include <cstddef>

struct RuntimeObjectCounts {
    size_t vehicles = 0;
    size_t constraints = 0;
    size_t persistentObjects = 0;
    size_t corpses = 0;
    size_t physicalEntities = 0;
};

// Clears runtime objects in dependency-safe order and emits one structured
// Physics event with the counts and reason. Safe to call when nothing exists.
RuntimeObjectCounts clearRuntimeObjects(const char* reason);
