// 2026-11-09
/* purpose
* thin host-side world query adapter for the hot NPC perception module
* owns the immutable world pointer for one fixed-tick sense call
* does NOT own perception, memory, belief, or NPC decisions
*/
#pragma once

struct World;

void setNpcWorldQueryWorld(const World* world);
bool npcWorldLineOfSightBlocked(const float from[3], const float to[3]);
bool npcWorldRaycastBlocked(const float from[3], const float direction[3],
                            float maxDistance, float* outDistance);
