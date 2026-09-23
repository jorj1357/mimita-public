// 09 23 2026
/* purpose
* Hot server session-handshake policy module. Registers the generic
* `net.session-policy` capability and serves the reconnect grace/rotation and
* map-ready spawn/rearm decisions from `hot-reload/hot-session-policy.h`.
* Editing that header (or this file) changes the reconnect grace window and the
* map-ready handling live: the cold paths in `network/server-packets.cpp` prefer
* this provider, with no EXE call site.
* Does NOT own token storage, transport, or transforms.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-session-policy.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL sessionReconnectGrace(void* /*host*/, GameReconnectGraceV1* request)
{
    HotSessionPolicyImpl::reconnectGrace(*request);
}

void MIMITA_GAME_CALL sessionMapReady(void* /*host*/, GameMapReadyV1* request)
{
    HotSessionPolicyImpl::mapReady(*request);
}

const GameSessionPolicyV1 kSessionPolicy{
    sizeof(GameSessionPolicyV1), 1, &sessionReconnectGrace, &sessionMapReady,
    "net.session-policy"};

const GameSessionPolicyV1* MIMITA_GAME_CALL lookupSessionPolicy(void* /*host*/)
{
    return &kSessionPolicy;
}

const GameCapabilityDescriptorV1 kSessionPolicyProvider{
    GAME_CAP_SESSION_POLICY, GAME_SIG_SESSION_POLICY, 0,
    reinterpret_cast<void*>(&lookupSessionPolicy), "net.session-policy"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_sessionPolicyProviderRegistrar{
    kSessionPolicyProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_sessionPolicyRequirement{
    GAME_CAP_SESSION_POLICY, GAME_SIG_SESSION_POLICY, 0};

#endif // MIMITA_GAME_DLL
