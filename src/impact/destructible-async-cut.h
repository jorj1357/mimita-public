// 2026-10-31
/* purpose
* Apply replicated destructible cuts off the client render thread. The worker
* observes an immutable base mesh + ordered cutter history and produces a
* Boolean result; the main thread installs the resulting surface at a safe
* point. This keeps remote map cuts from causing 3-6 ms frame spikes.
* Does NOT own authoritative cuts, entity transforms, or fragment authority.
*/

#pragma once

#include <cstdint>
#include <vector>

#include "impact/boolean-mesh.h"

namespace MimitaImpact {

class DestructibleAsyncCutWorker
{
public:
    static DestructibleAsyncCutWorker& instance();

    // Replaces any pending job for this network id with the newest full cut
    // history. The worker owns its copy of the base mesh and cutters.
    void submit(uint32_t networkId, BooleanMesh baseMesh,
                std::vector<BooleanCutter> cutters, uint64_t cutCount);

    // Main thread: install completed results into their mirrored entities.
    // Results older than the live cut history are discarded.
    void drainAndApply();

    // Main thread: drop queued/completed work for a despawned network id.
    void forget(uint32_t networkId);

private:
    DestructibleAsyncCutWorker();
    ~DestructibleAsyncCutWorker();
    DestructibleAsyncCutWorker(const DestructibleAsyncCutWorker&) = delete;
    DestructibleAsyncCutWorker& operator=(const DestructibleAsyncCutWorker&) = delete;

    struct Impl;
    Impl* mImpl = nullptr;
};

} // namespace MimitaImpact
