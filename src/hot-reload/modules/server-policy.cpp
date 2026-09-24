// 09 23 2026
/* purpose
* Hot dedicated-server startup policy module. Registers the generic
* `net.server-policy` capability and serves the shared mode-selection and
* startup-NPC-count decisions from `hot-reload/hot-server-policy.h`. Editing that
* header (or this file) changes how a dedicated server picks its mode and how
* many startup NPCs it spawns, live: the cold path in `network/server.cpp`
* prefers this provider over its compiled fallback, with no EXE call site.
* Does NOT own the loop, transport, or match state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-server-policy.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL serverMode(void* /*host*/, GameServerModeV1* request)
{
    HotServerPolicyImpl::mode(*request);
}

void MIMITA_GAME_CALL serverStartupNpc(void* /*host*/, GameServerStartupNpcV1* request)
{
    HotServerPolicyImpl::startupNpc(*request);
}

const GameServerPolicyV1 kServerPolicy{
    sizeof(GameServerPolicyV1), 1, &serverMode, &serverStartupNpc, "net.server-policy"};

const GameServerPolicyV1* MIMITA_GAME_CALL lookupServerPolicy(void* /*host*/)
{
    return &kServerPolicy;
}

const GameCapabilityDescriptorV1 kServerPolicyProvider{
    GAME_CAP_SERVER_POLICY, GAME_SIG_SERVER_POLICY, 0,
    reinterpret_cast<void*>(&lookupServerPolicy), "net.server-policy"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_serverPolicyProviderRegistrar{
    kServerPolicyProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_serverPolicyRequirement{
    GAME_CAP_SERVER_POLICY, GAME_SIG_SERVER_POLICY, 0};

#endif // MIMITA_GAME_DLL
