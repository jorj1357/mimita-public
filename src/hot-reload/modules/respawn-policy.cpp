// 09 23 2026
/* purpose
* Hot respawn rule module. Registers the generic `net.respawn` capability and
* serves the shared initial-timer/countdown rules from
* `hot-reload/hot-respawn.h`. Editing that header (or this file) changes whether
* and how fast players and NPCs respawn live: the cold paths in
* `network/server-players.cpp` and `network/server-npcs.cpp` prefer this
* provider over their compiled fallback, with no EXE call site.
* Does NOT own actor state, spawn selection, or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-respawn.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL respawnInitialTimer(void* /*host*/, GameRespawnRuleV1* request)
{
    HotRespawnImpl::initialTimer(*request);
}

void MIMITA_GAME_CALL respawnTick(void* /*host*/, GameRespawnRuleV1* request)
{
    HotRespawnImpl::tick(*request);
}

const GameRespawnPolicyV1 kRespawnPolicy{
    sizeof(GameRespawnPolicyV1), 1, &respawnInitialTimer, &respawnTick,
    "net.respawn"};

const GameRespawnPolicyV1* MIMITA_GAME_CALL lookupRespawn(void* /*host*/)
{
    return &kRespawnPolicy;
}

const GameCapabilityDescriptorV1 kRespawnProvider{
    GAME_CAP_RESPAWN, GAME_SIG_RESPAWN, 0,
    reinterpret_cast<void*>(&lookupRespawn), "net.respawn"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_respawnProviderRegistrar{
    kRespawnProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_respawnRequirement{
    GAME_CAP_RESPAWN, GAME_SIG_RESPAWN, 0};

#endif // MIMITA_GAME_DLL
