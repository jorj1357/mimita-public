// 09 23 2026
/* purpose
* Hot projectile cancellation policy module. Registers the generic
* `net.projectile-cancel` capability and serves the shared decision from
* `hot-reload/hot-projectile-cancel.h`. Editing that header (or this file)
* changes when a live projectile is cancelled (for example its owner died) live:
* the cold path in `network/server-projectiles.cpp` prefers this provider over
* its compiled fallback, with no EXE call site.
* Does NOT own projectile storage, despawn, or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile-cancel.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL projectileCancel(void* /*host*/, GameProjectileCancelV1* request)
{
    HotProjectileCancelImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kProjectileCancelProvider{
    GAME_CAP_PROJECTILE_CANCEL, GAME_SIG_PROJECTILE_CANCEL, 0,
    reinterpret_cast<void*>(&projectileCancel), "net.projectile-cancel"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_projectileCancelProviderRegistrar{
    kProjectileCancelProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_projectileCancelRequirement{
    GAME_CAP_PROJECTILE_CANCEL, GAME_SIG_PROJECTILE_CANCEL, 0};

#endif // MIMITA_GAME_DLL
