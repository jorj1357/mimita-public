// 09 13 2026
/* purpose
* Implements the constraint wire codec.
* Does NOT own the constraint store or transport.
*/
#include "network/constraint-codec.h"

#include <cstring>

namespace MimitaNet {

void encodeConstraint(const Physics::ConstraintComponent& component, ConstraintWire& out)
{
    out = ConstraintWire{};
    const Physics::Constraint& c = component.constraint;
    out.constraintSerial = component.constraintSerial;
    out.ownerActorId = component.ownerActor;
    out.type = (std::uint8_t)c.type;
    out.worldTarget = (c.bodyB == Physics::kWorldBody) ? 1u : 0u;
    out.bodyA = c.bodyA;
    out.limbA = c.limbA;
    out.bodyB = c.bodyB;
    out.limbB = c.limbB;
    out.strength = c.strength;
    out.damping = c.damping;
    out.minDistance = c.minDistance;
    out.maxDistance = c.maxDistance;
    out.createdTick = component.createdTick;
    for (int i = 0; i < 3; ++i) {
        out.anchorA[i] = c.anchorA[i];
        out.anchorB[i] = c.anchorB[i];
        out.worldPoint[i] = c.worldPoint[i];
    }
}

void decodeConstraint(const ConstraintWire& wire, Physics::ConstraintComponent& out)
{
    out = Physics::ConstraintComponent{};
    Physics::Constraint& c = out.constraint;
    c.type = (Physics::ConstraintType)wire.type;
    c.active = true;
    c.bodyA = wire.bodyA;
    c.limbA = wire.limbA;
    c.bodyB = wire.worldTarget ? Physics::kWorldBody : wire.bodyB;
    c.limbB = wire.limbB;
    c.strength = wire.strength;
    c.damping = wire.damping;
    c.minDistance = wire.minDistance;
    c.maxDistance = wire.maxDistance;
    for (int i = 0; i < 3; ++i) {
        c.anchorA[i] = wire.anchorA[i];
        c.anchorB[i] = wire.anchorB[i];
        c.worldPoint[i] = wire.worldPoint[i];
    }
    out.constraintSerial = wire.constraintSerial;
    out.ownerActor = wire.ownerActorId;
    out.createdTick = wire.createdTick;
}

} // namespace MimitaNet
