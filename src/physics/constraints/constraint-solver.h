// 09 12 2026
/* purpose
* Resolve generic physics constraints against a caller-supplied body table. The
* solver is subsystem-agnostic: ragdoll, corpse, persistent object, and future
* vehicle/VR bodies register in the table and share one constraint path.
* Does NOT own body state, the ECS, input, or networking.
*/
#pragma once

#include <cstddef>

#include "physics/constraints/constraint-components.h"
#include "physics/physical-body.h"

namespace Physics {

// Resolves a referenced body + limb index to a live RigidBody. Returns null for
// the world or an unresolved/dead body.
class SolveBodyTable {
public:
    virtual ~SolveBodyTable() = default;
    virtual RigidBody* resolve(std::uint32_t body, std::int32_t limb) = 0;
};

// Solve a single constraint in place. `baseBeta` is the position-correction
// factor (scaled by the constraint's strength). `iterations` is the velocity/
// position relaxation count.
void solveConstraint(SolveBodyTable& table, const Constraint& constraint,
                     float baseBeta, int iterations);

// Solve every active constraint in the list. No allocation.
void solveConstraints(SolveBodyTable& table, const Constraint* constraints,
                      std::size_t count, float baseBeta, int iterations);

} // namespace Physics
