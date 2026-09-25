// 09 24 2026
/* purpose
* Hot NPC navigation package (migration Phase 5g). Registers the generic
* `npc.navigation` capability; the logic lives in navigation-logic.h and may be
* edited freely, or split/added/removed across whole files under
* src/hot-reload/packages/navigation/ — the whole directory is hot source.
* Cold keeps its original NpcNavigation implementation as the fallback.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/packages/navigation/navigation-logic.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

const GameCapabilityDescriptorV1 kNpcNavigationProvider{
    GAME_CAP_NPC_NAVIGATION, GAME_SIG_NPC_NAVIGATION, 0,
    reinterpret_cast<void*>(&HotNavigationPackage::evaluate), "npc.navigation"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcNavigationProvider{
    kNpcNavigationProvider};

#endif // MIMITA_GAME_DLL
