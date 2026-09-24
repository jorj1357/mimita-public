// 09 23 2026
/* purpose
* Hot client connection-health policy module. Registers the generic
* `net.connection-policy` capability and serves the shared next-state and
* reconnect-cadence decisions from `hot-reload/hot-connection-health.h`.
* Editing that header (or this file) changes when the client warns, reconnects,
* or gives up live: the cold path in `network/multiplayer-packets.cpp` prefers
* this provider over its compiled fallback, with no EXE call site.
* Does NOT own transport, teardown, or notifications.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-connection-health.h"
#include "hot-reload/hot-connection-transition.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL connectionNextState(void* /*host*/, GameConnectionHealthV1* request)
{
    HotConnectionHealthImpl::nextState(*request);
}

void MIMITA_GAME_CALL connectionCadence(void* /*host*/, GameReconnectCadenceV1* request)
{
    HotConnectionHealthImpl::cadence(*request);
}

const GameConnectionPolicyV1 kConnectionPolicy{
    sizeof(GameConnectionPolicyV1), 1, &connectionNextState, &connectionCadence,
    "net.connection-policy"};

const GameConnectionPolicyV1* MIMITA_GAME_CALL lookupConnectionPolicy(void* /*host*/)
{
    return &kConnectionPolicy;
}

const GameCapabilityDescriptorV1 kConnectionPolicyProvider{
    GAME_CAP_CONNECTION_POLICY, GAME_SIG_CONNECTION_POLICY, 0,
    reinterpret_cast<void*>(&lookupConnectionPolicy), "net.connection-policy"};

// connection.transition: the hot join/reconnect state-machine decision. The cold
// side supplies POD facts and applies the returned state/action/label.
void MIMITA_GAME_CALL connectionTransition(void* /*host*/,
                                           GameConnectionTransitionV1* request)
{
    HotConnectionTransitionImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kConnectionTransitionProvider{
    GAME_CAP_CONNECTION_TRANSITION, GAME_SIG_CONNECTION_TRANSITION, 0,
    reinterpret_cast<void*>(&connectionTransition), "connection.transition"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_connectionPolicyProviderRegistrar{
    kConnectionPolicyProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_connectionPolicyRequirement{
    GAME_CAP_CONNECTION_POLICY, GAME_SIG_CONNECTION_POLICY, 0};
const MimitaHotPackage::CapabilityRegistrar s_connectionTransitionProviderRegistrar{
    kConnectionTransitionProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_connectionTransitionRequirement{
    GAME_CAP_CONNECTION_TRANSITION, GAME_SIG_CONNECTION_TRANSITION, 0};

#endif // MIMITA_GAME_DLL
