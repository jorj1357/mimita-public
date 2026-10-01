// 2026-09-28
/* purpose
* Draw the generated destructible mesh for one physical entity.
* Lives beside the destructible owner so physical-entity.cpp stays focused on
* rigid-body state; the geometry owner supplies the vertex soup.
* Does NOT own geometry generation or collision.
*/

#pragma once

struct PhysicalEntity;
class Camera;

// Draws the entity's generated destructible triangle soup. Returns true when it
// drew (destructible enabled and vertices present); false to use the box fallback.
bool drawGeneratedEntityMesh(const PhysicalEntity& entity, const Camera& camera);

// Frees the per-entity GPU buffers for `entityId`. Call when an entity is
// removed so fragment churn does not leak GPU buffers.
void releaseGeneratedEntityMesh(unsigned int entityId);
