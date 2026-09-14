// 09 12 2026
/* purpose
* Implements the generic constraint solver.
* Does NOT own body state, the ECS, input, or networking.
*/
#include "physics/constraints/constraint-solver.h"

#include <algorithm>

#include "telemetry/telemetry.h"

namespace Physics {

void solveConstraint(SolveBodyTable& table, const Constraint& c,
                     float baseBeta, int iterations)
{
    if (!c.active || iterations <= 0)
        return;
    MIMITA_TELEMETRY_SCOPE("ConstraintSolve");
    RigidBody* a = table.resolve(c.bodyA, c.limbA);
    if (!a)
        return;

    const float grip = std::clamp(c.strength, 0.0f, 1.0f);
    // A weak constraint relaxes toward lossless free motion instead of rigid.
    const float beta = std::clamp(baseBeta * grip + 1.0f * (1.0f - grip), 0.0f, 1.0f);

    RigidBody* b = (c.bodyB == kWorldBody) ? nullptr : table.resolve(c.bodyB, c.limbB);

    for (int i = 0; i < iterations; ++i) {
        const glm::vec3 anchorA = a->position + a->orientation * c.anchorA;

        if (b) {
            const glm::vec3 anchorB = b->position + b->orientation * c.anchorB;
            if (c.type == ConstraintType::Distance) {
                if (c.maxDistance > 0.0f) {
                    solvePointJointMaxDistanceVelocity(*a, anchorA, *b, anchorB, c.maxDistance);
                    solvePointJointMaxDistance(*a, anchorA, *b, anchorB, c.maxDistance, beta);
                } else {
                    solvePointJointVelocity(*a, anchorA, *b, anchorB);
                    solvePointJointPosition(*a, anchorA, *b, anchorB, beta);
                }
            } else {
                solvePointJointVelocity(*a, anchorA, *b, anchorB);
                solvePointJointPosition(*a, anchorA, *b, anchorB, beta);
            }
        } else {
            // World anchor: cancel anchor velocity, then project to the point.
            solvePointToWorldVelocity(*a, anchorA);
            solvePointToWorld(*a, anchorA, c.worldPoint, beta);
        }
    }
}

void solveConstraints(SolveBodyTable& table, const Constraint* constraints,
                      std::size_t count, float baseBeta, int iterations)
{
    if (!constraints)
        return;
    for (std::size_t i = 0; i < count; ++i)
        solveConstraint(table, constraints[i], baseBeta, iterations);
}

} // namespace Physics
