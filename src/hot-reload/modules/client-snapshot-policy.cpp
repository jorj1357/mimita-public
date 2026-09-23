// 09 23 2026
/* purpose
* Hot client snapshot-apply policy module. Registers the generic
* `net.client-snapshot` capability and serves the shared local-stale/membership
* decision from `hot-reload/hot-client-snapshot.h`. Editing that header (or this
* file) changes which snapshots may mutate lifecycle live: the cold path in
* `network/multiplayer-tick.cpp` prefers this provider over its compiled
* fallback, with no EXE call site.
* Does NOT own replicas, interpolation, or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-client-snapshot.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL evaluateSnapshotApply(void* /*host*/,
                                            GameSnapshotApplyV1* request)
{
    HotClientSnapshotImpl::evaluate(*request);
}

const GameCapabilityDescriptorV1 kClientSnapshotProvider{
    GAME_CAP_CLIENT_SNAPSHOT, GAME_SIG_CLIENT_SNAPSHOT, 0,
    reinterpret_cast<void*>(&evaluateSnapshotApply), "net.client-snapshot"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_clientSnapshotProviderRegistrar{
    kClientSnapshotProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_clientSnapshotRequirement{
    GAME_CAP_CLIENT_SNAPSHOT, GAME_SIG_CLIENT_SNAPSHOT, 0};

#endif // MIMITA_GAME_DLL
