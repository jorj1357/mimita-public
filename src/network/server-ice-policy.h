// 2026-11-17
// Hot ICE/room policy decisions for the resident server pump.
//
// The resident host owns libjuice, sockets, worker threads, and opaque handles.
// This owner decides timeouts, admission, expiry, and relay filtering. It runs
// inside the replaceable game generation, so editing these decisions does not
// require an EXE rebuild.
#pragma once

#include "host/abi/game-server-abi.h"

namespace MimitaNet {

void serverIcePolicyDecide(mimita::host::ServerIcePolicyV1& policy);

}  // namespace MimitaNet
