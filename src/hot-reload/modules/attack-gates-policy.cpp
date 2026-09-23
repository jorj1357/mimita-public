// 09 23 2026
/* purpose
* Hot server attack-gate policy module. Registers the generic
* `net.attack-gates` capability and serves the shared thresholds from
* `hot-reload/hot-attack-gates.h` (cooldown grace, per-tick shot rate limit,
* hitscan origin geometry tolerance). Editing that header (or this file)
* changes the gates live: the cold path in `network/server-attack.cpp` prefers
* this provider over its compiled fallback, with no EXE call site.
* Does NOT own weapon state, transport, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-attack-gates.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateAttackGates(void* /*host*/, GameAttackGatesV1* request)
{
    HotAttackGatesImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kAttackGatesProvider{
    GAME_CAP_ATTACK_GATES, GAME_SIG_ATTACK_GATES, 0,
    reinterpret_cast<void*>(&evaluateAttackGates), "net.attack-gates"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_attackGatesProviderRegistrar{
    kAttackGatesProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_attackGatesRequirement{
    GAME_CAP_ATTACK_GATES, GAME_SIG_ATTACK_GATES, 0};

#endif // MIMITA_GAME_DLL
