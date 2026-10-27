// 2026-10-27
/* purpose
* Own the generalized constraint set and solve it once per fixed physics tick.
* Reuses PhysicalEntity mass, center of mass, velocity, and inverse inertia.
* Does NOT integrate bodies, run collision, or know about vehicles.
* Does NOT send packets or draw anything.
*/
#pragma once

#include <cstdint>
#include <vector>

#include "physics/constraints/constraint.h"

class ConstraintSystem {
public:
    static ConstraintSystem& instance();

    uint32_t add(const Constraint& constraint);
    bool remove(uint32_t id);
    void clear();

    const std::vector<Constraint>& constraints() const { return mConstraints; }
    std::vector<Constraint>& constraints() { return mConstraints; }
    size_t count() const { return mConstraints.size(); }

    // Solves every enabled constraint. Called by the fixed physical-entity tick.
    // `dt` is the fixed step, never a render delta.
    void solve(float dt);

private:
    ConstraintSystem() = default;

    std::vector<Constraint> mConstraints;
    uint32_t mNextId = 1;
};
