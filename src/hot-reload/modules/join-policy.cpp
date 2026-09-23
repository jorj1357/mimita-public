// 09 23 2026
/* purpose
* Hot server join-acceptance policy module. Registers the generic
* `net.join-policy` capability and serves the shared rules from
* `hot-reload/hot-join-policy.h` (full, token, local/coordinator, password).
* Editing that header (or this file) changes who may join live: the cold path in
* `network/server-packets.cpp` prefers this provider over its compiled fallback,
* with no EXE call site.
* Does NOT own token validation, transport, or player state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-join-policy.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateJoinPolicy(void* /*host*/, GameJoinPolicyV1* request)
{
    HotJoinPolicyImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kJoinPolicyProvider{
    GAME_CAP_JOIN_POLICY, GAME_SIG_JOIN_POLICY, 0,
    reinterpret_cast<void*>(&evaluateJoinPolicy), "net.join-policy"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_joinPolicyProviderRegistrar{
    kJoinPolicyProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_joinPolicyRequirement{
    GAME_CAP_JOIN_POLICY, GAME_SIG_JOIN_POLICY, 0};

#endif // MIMITA_GAME_DLL
