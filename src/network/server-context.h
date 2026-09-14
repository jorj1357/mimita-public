// 09 14 2026
/* purpose
* A transient, generic handle to the live authoritative server state so kernel
* capabilities (resolved by id) can perform real world actions on behalf of hot
* code without exposing the raw players/projectiles containers.
* The server owns the lifetime; the pointer is valid only during the server
* process's run loop. Hot code sees entities/components/relationships and the
* generic capability ops below, never the containers.
* Does NOT own networking, simulation, or the containers themselves.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct ServerContextV1 {
    std::uintptr_t sock = 0;             // SOCKET
    void* players = nullptr;             // unordered_map<uint32_t, ServerPlayer>*
    void* npcs = nullptr;                // unordered_map<uint32_t, ServerNpc>*
    void* projectiles = nullptr;         // unordered_map<uint32_t, ServerProjectile>*
    std::uint32_t* nextProjectileId = nullptr;
    const std::uint32_t* tick = nullptr;
    std::uint64_t* totalPacketsOut = nullptr;
    const void* world = nullptr;         // HeadlessWorld*
};

ServerContextV1* activeServerContext();
void setActiveServerContext(ServerContextV1* context);

// Generic authoritative operations. Implemented by the projectile/damage owners
// so the containers stay private to the network layer.
bool serverSpawnGenericProjectile(const GameProjectileSpawnSpecV1& spec,
                                  std::uint64_t* outEntity);
bool serverApplyEntityDamage(GameDamageApplyV1& request);

} // namespace MimitaNet
