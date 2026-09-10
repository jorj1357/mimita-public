// 2026-09-10
/* purpose
* Provide a small reusable rigid-body primitive with capsule shape, angular
* dynamics, joints, and world/self collision.
* Shared, deterministic, and free of rendering, transport, and client/server
* identity so it can run on a client now and be reused by server authority,
* replays, tests, and other moving physical objects later.
* Does NOT own input, networking, rendering, damage, or weapon policy.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics/physics-types.h"

struct World;

struct RigidBody {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};

    float mass = 1.0f;
    float invMass = 1.0f;
    float inertia = 1.0f;
    float invInertia = 1.0f;

    float capsuleRadius = 0.15f;
    float capsuleHalfHeight = 0.15f;
    glm::vec3 localAxis{0.0f, 0.0f, 1.0f};
    glm::vec3 capsuleCenter{0.0f};

    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float restitution = 0.0f;
    float friction = 0.5f;

    float maxLinearSpeed = 60.0f;
    float maxAngularSpeed = 25.0f;

    // Below these speeds the part is considered at rest and its velocity is
    // zeroed, so a limb naturally comes to a stop instead of jittering.
    float stopLinearSpeed = 0.0f;
    float stopAngularSpeed = 0.0f;

    bool staticBody = false;
};

// Capsule endpoints in world space from the body transform.
Capsule capsuleOf(const RigidBody& body);

// Sets mass and derives the inverse mass and isotropic capsule inertia.
void setBodyMass(RigidBody& body, float mass);

// Linear velocity of a world-space point rigidly attached to the body.
glm::vec3 pointVelocity(const RigidBody& body, const glm::vec3& worldPoint);

// Inverse effective mass along a direction at an offset from the center.
float inverseMassAlong(const RigidBody& body, const glm::vec3& r, const glm::vec3& n);

// Semi-implicit integration of gravity, damping, linear and angular motion.
void integrate(RigidBody& body, const glm::vec3& gravity, float dt);

// Applies an impulse at a world point, producing linear and angular change.
void applyImpulseAtPoint(RigidBody& body, const glm::vec3& impulse, const glm::vec3& worldPoint);

// Rotates the body by a world-space axis-angle delta.
void rotateBody(RigidBody& body, const glm::vec3& deltaTheta);

// Removes relative velocity along the joint error direction.
void solvePointJointVelocity(RigidBody& a, const glm::vec3& anchorA,
                             RigidBody& b, const glm::vec3& anchorB);

// Projects the two anchors together (position based), applied to both bodies.
void solvePointJointPosition(RigidBody& a, const glm::vec3& anchorA,
                             RigidBody& b, const glm::vec3& anchorB,
                             float beta);

// Cancels the body's velocity at a fixed world point (grab / static
// attachment). This is what keeps a hanging grab from accumulating gravity.
void solvePointToWorldVelocity(RigidBody& body, const glm::vec3& bodyAnchor);

// Projects a body point onto a fixed world point (grab / static attachment).
void solvePointToWorld(RigidBody& body, const glm::vec3& bodyAnchor,
                       const glm::vec3& worldPoint, float beta);

// Position-only overlap recovery against the world (no velocity integration).
// Used as a final solidity pass after constraints.
bool depenetrateWorld(RigidBody& body, const World& world, int passes);

// Swept, substepped world collision. Returns true when a contact occurred.
bool collideWithWorld(RigidBody& body, const World& world, float dt);

// Capsule-vs-capsule collision between two bodies. Contacts whose closest
// points fall within excludeRadius of excludePoint are ignored (used to let
// directly-jointed parts collide away from their shared joint). Returns true
// on a resolved contact.
bool collideBodies(RigidBody& a, RigidBody& b,
                   const glm::vec3& excludePoint = glm::vec3(0.0f),
                   float excludeRadius = 0.0f,
                   float correctionBeta = 0.8f,
                   float slop = 0.0f,
                   float maxCorrection = 0.0f);
