// 09 23 2026
/* purpose
* Hot NPC ground-clamp policy module. Registers the generic
* `net.npc-ground-clamp` capability and serves the shared clamp from
* `hot-reload/hot-npc-ground-clamp.h`. Editing that header (or this file)
* changes when a server NPC is pinned back onto the floor live: the cold path in
* `network/server-npcs.cpp` prefers this provider over its compiled fallback,
* with no EXE call site.
* Does NOT own the world query or NPC movement.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-npc-ground-clamp.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateGroundClamp(void* /*host*/, GameNpcGroundClampV1* request)
{
    HotNpcGroundClampImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kGroundClampProvider{
    GAME_CAP_NPC_GROUND_CLAMP, GAME_SIG_NPC_GROUND_CLAMP, 0,
    reinterpret_cast<void*>(&evaluateGroundClamp), "net.npc-ground-clamp"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_groundClampProviderRegistrar{
    kGroundClampProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_groundClampRequirement{
    GAME_CAP_NPC_GROUND_CLAMP, GAME_SIG_NPC_GROUND_CLAMP, 0};

#endif // MIMITA_GAME_DLL
