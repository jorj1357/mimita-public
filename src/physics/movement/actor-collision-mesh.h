// 2026-09-27
/* purpose
* Define the one generic actor collision input: a labelled local triangle set
* plus the previous and desired world transforms for that set.
* Collect every physical part of an actor (body, weapon, future held objects)
* through one function so the collision owner treats them identically.
* Provide GL-free GLB loaders for body-part and weapon-render-mesh triangles so
* NPC, headless-server, and replay actors can supply the same input.
* Does NOT solve, correct, or respond; the triangle solver owns that.
* Does NOT render, upload GPU resources, or require a GL context.
* Does NOT own movement, damage, networking, or effects.
*/
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "physics/physics-types.h"

class Player;

// One actor part's collision geometry. localTriangles points into actor-owned
// storage that must outlive the solve call (a body part collider, the weapon
// collider mesh). previousTransform is the sweep start, desiredTransform the
// pose to reach this tick.
struct ActorCollisionMesh {
    const char* label = "actor";
    const std::vector<CollisionTriangle>* localTriangles = nullptr;
    glm::mat4 previousTransform{1.0f};
    glm::mat4 desiredTransform{1.0f};
    bool affectsMovement = true;
};

// All collision geometry that belongs to the actor: every physical body part
// and the equipped weapon. Body parts and weapons use the same representation.
// Order is stable: body parts in physicalBody order, then the weapon.
std::vector<ActorCollisionMesh> collectActorCollisionMeshes(Player& player);

// Body parts only, in physicalBody order. Used where the weapon is handled by a
// different owner (e.g. the legacy body-mesh contact path).
std::vector<ActorCollisionMesh> collectActorBodyCollisionMeshes(Player& player);

// Advance the sweep-start transforms to the desired pose. Call once per tick
// after solving so the next tick's sweep starts where this one ended.
void commitActorCollisionMeshes(Player& player);

// ── GL-free GLB loaders ────────────────────────────────────────────

struct ActorMeshPart {
    std::string name;
    int nodeIndex = -1;
    std::vector<CollisionTriangle> triangles;   // node-local
    glm::vec3 localMin{0.0f};
    glm::vec3 localMax{0.0f};
};

// Node-local body-part triangles from a character GLB (head/torso/arms/legs).
bool loadActorBodyMeshParts(const char* glbPath, std::vector<ActorMeshPart>& out);

// All triangles of a weapon render GLB in the same model-local space the
// rendered weapon mesh uses (node hierarchy baked, like walkGLBScene).
bool loadActorWeaponTriangles(const char* glbPath, std::vector<CollisionTriangle>& out);

// Populate the player's weapon collider mesh from a GLB when needed. Returns
// true when triangles are available.
bool ensureActorWeaponColliderMesh(Player& player, const char* glbPath);

// Populate the weapon collider mesh from the player's equipped weapon model.
// Returns false when there is no equipped weapon or the weapon world transform
// is the unset identity placeholder.
bool ensureActorWeaponColliderMeshFromEquipped(Player& player);

// Populate the player's body-part collision triangles (skeleton + colliders)
// with no GL when the actor has none. Returns true when parts are available.
bool ensureActorBodyCollisionMesh(Player& player, const char* glbPath);

// Deterministic Phase 2 test: a headless actor builds body + weapon triangle
// meshes through collectActorCollisionMeshes and reports sweep-transform deltas.
bool actorCollisionMeshSelfTest(std::string* outSummary = nullptr);
