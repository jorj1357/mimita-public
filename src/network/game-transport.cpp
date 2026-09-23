// 09 23 2026
/* purpose
* Implements the hot-reload safety registry for live transports. Holds
* non-owning pointers to transports whose OS/thread callbacks must not observe a
* module swap, and quiesces them before an activation barrier.
* Does NOT own transports, sockets, or emit packets.
*/
#include "network/game-transport.h"

#include <algorithm>
#include <mutex>

namespace MimitaTransport {
namespace {

std::mutex gMutex;
std::vector<IGameTransport*> gTransports;

} // namespace

void registerTransport(IGameTransport* transport)
{
    if (!transport)
        return;
    std::lock_guard<std::mutex> lock(gMutex);
    if (std::find(gTransports.begin(), gTransports.end(), transport) == gTransports.end())
        gTransports.push_back(transport);
}

void unregisterTransport(IGameTransport* transport)
{
    if (!transport)
        return;
    std::lock_guard<std::mutex> lock(gMutex);
    gTransports.erase(
        std::remove(gTransports.begin(), gTransports.end(), transport),
        gTransports.end());
}

void quiesceAllTransports()
{
    // Snapshot under the lock, call outside it: a quiesce must never run while
    // holding the registry mutex (it may log or touch transport-owned state).
    std::vector<IGameTransport*> snapshot;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        snapshot = gTransports;
    }
    for (IGameTransport* transport : snapshot)
        if (transport)
            transport->quiesceForReload();
}

} // namespace MimitaTransport
