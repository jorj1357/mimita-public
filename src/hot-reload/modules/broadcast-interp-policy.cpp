// 09 23 2026
/* purpose
* Hot server broadcast-interpolation policy module. Registers the generic
* `net.broadcast-interp` capability and serves the shared decision from
* `hot-reload/hot-broadcast-interp.h`. Editing that header (or this file)
* changes whether/how far remote players' broadcasts smooth live: the cold path
* in `network/server-players.cpp` prefers this provider over its compiled
* fallback, with no EXE call site.
* Does NOT own the sample buffer or the interpolation mechanism.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-broadcast-interp.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateBroadcastInterp(void* /*host*/,
                                              GameBroadcastInterpV1* request)
{
    HotBroadcastInterpImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kBroadcastInterpProvider{
    GAME_CAP_BROADCAST_INTERP, GAME_SIG_BROADCAST_INTERP, 0,
    reinterpret_cast<void*>(&evaluateBroadcastInterp), "net.broadcast-interp"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_broadcastInterpProviderRegistrar{
    kBroadcastInterpProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_broadcastInterpRequirement{
    GAME_CAP_BROADCAST_INTERP, GAME_SIG_BROADCAST_INTERP, 0};

#endif // MIMITA_GAME_DLL
