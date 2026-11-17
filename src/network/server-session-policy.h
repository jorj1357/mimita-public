// 2026-11-17
// Hot room/session publication policy for the resident server pump.
#pragma once

#include "host/abi/game-server-abi.h"

namespace MimitaNet {

void serverSessionPolicyDecide(mimita::host::ServerSessionPolicyV1& policy);

}  // namespace MimitaNet
