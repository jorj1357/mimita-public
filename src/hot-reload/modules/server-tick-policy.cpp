// 09 23 2026
/* purpose
* Hot server fixed-tick orchestration policy module. Registers the generic
* `network.tick` capability and serves the shared orchestration decisions from
* `hot-reload/hot-server-tick.h` (catch-up cap, snapshot cadence, phase gates,
// shutdown request). Editing that header (or this file) changes the server
* tick policy live: the cold path in `network/server.cpp` prefers this provider
* over its compiled fallback, with no EXE call site.
* Does NOT own the loop, transport, or persistent state.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-server-tick.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL serverTick(void* /*host*/, GameServerTickV1* request)
{
    HotServerTickImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kServerTickProvider{
    GAME_CAP_SERVER_TICK, GAME_SIG_SERVER_TICK, 0,
    reinterpret_cast<void*>(&serverTick), "network.tick"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_serverTickProviderRegistrar{
    kServerTickProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_serverTickRequirement{
    GAME_CAP_SERVER_TICK, GAME_SIG_SERVER_TICK, 0};

#endif // MIMITA_GAME_DLL
