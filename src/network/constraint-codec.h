// 09 13 2026
/* purpose
* Encode/decode between the canonical Physics::ConstraintComponent and the
* network ConstraintWire payload. One codec for every constraint type.
* Does NOT own the constraint store or transport.
*/
#pragma once

#include "network/packets.h"
#include "physics/constraints/constraint-components.h"

namespace MimitaNet {

void encodeConstraint(const Physics::ConstraintComponent& component, ConstraintWire& out);
void decodeConstraint(const ConstraintWire& wire, Physics::ConstraintComponent& out);

} // namespace MimitaNet
