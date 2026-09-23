// 09 23 2026
/* purpose
* Hot client predicted-projectile correction policy module. Registers the
* generic `net.projectile-correction` capability and serves the shared decision
* from `hot-reload/hot-projectile-correction.h`. Editing that header (or this
* file) changes when/how a predicted projectile is corrected toward server state
* live: the cold path in `network/multiplayer-projectiles.cpp` prefers this
* provider over its compiled fallback, with no EXE call site.
* Does NOT own projectile state or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile-correction.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateProjectileCorrection(
    void* /*host*/, GameProjectileCorrectionV1* request)
{
    HotProjectileCorrectionImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kProjectileCorrectionProvider{
    GAME_CAP_PROJECTILE_CORRECTION, GAME_SIG_PROJECTILE_CORRECTION, 0,
    reinterpret_cast<void*>(&evaluateProjectileCorrection),
    "net.projectile-correction"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_projectileCorrectionProviderRegistrar{
    kProjectileCorrectionProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_projectileCorrectionRequirement{
    GAME_CAP_PROJECTILE_CORRECTION, GAME_SIG_PROJECTILE_CORRECTION, 0};

#endif // MIMITA_GAME_DLL
