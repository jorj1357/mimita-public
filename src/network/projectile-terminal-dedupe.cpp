// 07 19 2026, 12 05
/* purpose
* Implement bounded projectile terminal event deduplication.
* Store authoritative projectile IDs that have already terminated on a client.
* Evict oldest tombstones to keep memory usage predictable.
* Does NOT inspect projectile packets, play effects, or mutate render objects.
* Does NOT trust client impacts or alter server projectile behavior.
* Does NOT provide weapon-specific lifecycle logic.
*/

#include "network/projectile-terminal-dedupe.h"

namespace MimitaNet {

bool ProjectileTerminalDedupe::has(uint32_t projectileId) const
{
    return projectileId != 0 && mSeen.find(projectileId) != mSeen.end();
}

bool ProjectileTerminalDedupe::record(uint32_t projectileId)
{
    if (projectileId == 0)
        return false;
    if (mSeen.find(projectileId) != mSeen.end())
        return false;

    mSeen.insert(projectileId);
    mOrder.push_back(projectileId);
    while (mOrder.size() > mLimit)
    {
        uint32_t old = mOrder.front();
        mOrder.pop_front();
        mSeen.erase(old);
    }
    return true;
}

void ProjectileTerminalDedupe::clear()
{
    mOrder.clear();
    mSeen.clear();
}

} // namespace MimitaNet
