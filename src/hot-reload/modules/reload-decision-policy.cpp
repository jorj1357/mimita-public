// 09 23 2026
/* purpose
* Hot server reload-decision module. Registers the generic `net.reload-decision`
* capability and serves the shared accept/reject reason ordering from
* `hot-reload/hot-reload-decision.h`. Editing that header (or this file) changes
* reload acceptance live: the cold path in `network/server-packets.cpp` prefers
* this provider over its compiled fallback, with no EXE call site.
* Does NOT own weapon state or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-reload-decision.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateReloadDecision(void* /*host*/,
                                             GameReloadDecisionV1* request)
{
    HotReloadDecisionImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kReloadDecisionProvider{
    GAME_CAP_RELOAD_DECISION, GAME_SIG_RELOAD_DECISION, 0,
    reinterpret_cast<void*>(&evaluateReloadDecision), "net.reload-decision"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_reloadDecisionProviderRegistrar{
    kReloadDecisionProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_reloadDecisionRequirement{
    GAME_CAP_RELOAD_DECISION, GAME_SIG_RELOAD_DECISION, 0};

#endif // MIMITA_GAME_DLL
