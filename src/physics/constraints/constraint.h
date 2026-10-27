// 2026-10-27
/* purpose
* Define the generalized constraint record used by vehicles, doors, ragdoll
* limbs, grabs, and moving machinery.
* Constraints are data. One solver owner consumes them; no feature-specific
* hinge or suspension implementations live in gameplay code.
* Does NOT integrate bodies or own collision.
* Does NOT render or transport constraints.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "physics/physical-entity.h"

enum class ConstraintType : uint8_t {
    Point,
    Hinge,
    Slider
};

// entityA/entityB are PhysicalEntity ids. An id of 0 means a fixed world
// anchor: localAnchorB/localAxisB are then interpreted as world space.
struct Constraint {
    uint32_t id = 0;
    ConstraintType type = ConstraintType::Point;
    uint32_t entityA = 0;
    uint32_t entityB = 0;

    glm::vec3 localAnchorA{0.0f};
    glm::vec3 localAnchorB{0.0f};
    glm::vec3 localAxisA{0.0f, 0.0f, 1.0f};
    glm::vec3 localAxisB{0.0f, 0.0f, 1.0f};

    bool limitsEnabled = false;
    float minLimit = 0.0f;
    float maxLimit = 0.0f;

    float springStiffness = 0.0f;
    float springDamping = 0.0f;
    float restLength = 0.0f;

    bool motorEnabled = false;
    float motorSpeed = 0.0f;
    float motorMaxForce = 0.0f;

    float breakForce = 0.0f;
    float breakTorque = 0.0f;

    bool enabled = true;
};
