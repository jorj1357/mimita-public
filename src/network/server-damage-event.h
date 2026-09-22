// 09 22 2026
/* purpose
* Per-victim authoritative damage application + replication event, exposed as a
* generic capability so hot consequence orchestration can run the exact same
* pipeline as the cold path.
* Does NOT own the trace or weapon behavior.
*/
#pragma once

#include "hot-reload/game-api.h"

namespace MimitaNet {

bool serverDamagePolicyQuery(GameDamagePolicyV1& request);
void serverApplyDamageEvent(const GameDamageEventV1& event);

} // namespace MimitaNet
