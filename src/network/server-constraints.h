// 09 13 2026
/* purpose
* Server-authoritative generic constraint lifecycle: validate create/release
* intent, own the canonical ConstraintStore, and broadcast reliable events.
* Does NOT simulate ragdoll physics; the owner client remains the body authority.
*/
#pragma once

#include <cstdint>
#include <unordered_map>

#include "network/server.h"

namespace MimitaNet {

void handleConstraintCreateRequest(SOCKET sock, const char* buffer, int bytes,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   uint32_t tick, const ServerPlayer* authenticatedPlayer,
                                   uint64_t& totalPacketsOut);

void handleConstraintReleaseRequest(SOCKET sock, const char* buffer, int bytes,
                                    std::unordered_map<uint32_t, ServerPlayer>& players,
                                    uint32_t tick, const ServerPlayer* authenticatedPlayer,
                                    uint64_t& totalPacketsOut);

// Sends the currently active constraint set to one (joining) player.
void sendActiveConstraintSnapshot(SOCKET sock, ServerPlayer& player, uint32_t tick,
                                  uint64_t& totalPacketsOut);

} // namespace MimitaNet
