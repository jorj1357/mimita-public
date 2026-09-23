// 09 23 2026
/* purpose
* Hot authoritative damage-application policy module. Registers the generic
* `net.damage-application` capability and serves the shared rules from
* `hot-reload/hot-damage-application.h` (accept/reject, friendly-fire filter,
* damage clamp, death/respawn rule). Editing that header (or this file) changes
* the server's damage rules live: the cold bridge in `network/server-damage.cpp`
* prefers this provider over its compiled fallback, with no EXE call site.
* Does NOT own health storage, transport, or kill recording.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-damage-application.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateDamageApplication(void* /*host*/,
                                                GameDamageApplicationV1* request)
{
    HotDamageApplicationImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kDamageApplicationProvider{
    GAME_CAP_DAMAGE_APPLICATION, GAME_SIG_DAMAGE_APPLICATION, 0,
    reinterpret_cast<void*>(&evaluateDamageApplication), "net.damage-application"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_damageApplicationProviderRegistrar{
    kDamageApplicationProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_damageApplicationRequirement{
    GAME_CAP_DAMAGE_APPLICATION, GAME_SIG_DAMAGE_APPLICATION, 0};

#endif // MIMITA_GAME_DLL
