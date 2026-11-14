// 2026-11-14
// DLL-side server cutover behavior. The resident server loop passes opaque
// state through ServerFrameV1; these functions run on the resident thread with
// the game generation barrier held by the host.

#pragma once

#include "host/abi/game-server-abi.h"

namespace mimita::game {

int serverRuntimeProcessPacket(void* frame, const void* event,
                               mimita::host::ServerPacketResultV1* out);
int serverRuntimeSimulatePlayers(void* frame);
int serverRuntimeSimulateWorld(void* frame);
int serverRuntimePublish(void* frame);
int serverRuntimeGamemode(void* frame);

}  // namespace mimita::game
