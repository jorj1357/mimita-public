// 2026-11-07
/* purpose
* maintain a live streamed collision surface for server NPC simulation on
* connected/procedural grid worlds
* this owner does not render, generate visuals, or own client chunk state
*/
#pragma once

#include <vector>

#include <glm/glm.hpp>

struct World;

// Rebuild the server NPC collision surface around active actor anchors. The
// streamed surface is appended to the same World collision mesh tail used by
// authored map geometry, so NPC movement and navigation see one surface.
void serverInfiniteCollisionTick(const std::vector<glm::vec3>& anchors,
                                 World& npcWorld, bool streamingEnabled);
void serverInfiniteCollisionReset();
