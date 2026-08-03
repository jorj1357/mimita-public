// 08 03 2026, 10 30
/* purpose
* Implements remote-entity membership ordering and removal grace helpers.
* Separates authoritative membership reconciliation from interpolation history.
* Prevents stale/reordered snapshots from destroying newer world state.
* Does NOT send packets, render, simulate, or own the player registry.
* Does NOT decode packets or own snapshot chunk reassembly.
*/

#include "network/remote-entity-lifecycle.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"

namespace MimitaNet {
bool snapshotMayMutateMembership(uint32_t serverTick,
                                 uint32_t latestAppliedMembershipTick)
{
    if (latestAppliedMembershipTick == 0)
        return true;
    return serverTick >= latestAppliedMembershipTick;
}

void MissingEntityTracker::noteSeen()
{
    confirmations = 0;
    firstMissingMs = 0;
}

void MissingEntityTracker::noteMissing(uint64_t nowMs)
{
    if (confirmations == 0)
        firstMissingMs = nowMs;
    ++confirmations;
}

bool MissingEntityTracker::shouldRemove(uint32_t confirmationCount,
                                        uint64_t graceMs,
                                        uint64_t nowMs) const
{
    if (confirmations < confirmationCount)
        return false;
    if (graceMs == 0)
        return true;
    if (firstMissingMs == 0)
        return false;
    return nowMs >= firstMissingMs + graceMs;
}

void baselinePresentationSerials(Player& player,
                                 const SnapshotTransform& target)
{
    player.networkLastDashSerial = target.dashSerial;
    player.networkLastGroundJumpSerial = target.groundJumpSerial;
    player.networkLastAirJumpSerial = target.airJumpSerial;
    player.networkLastDownDashSerial = target.downDashSerial;
    player.networkLastDirectionChangeSerial = target.directionChangeSerial;
    player.networkLastFreezeSerial = target.freezeSerial;
    player.networkLastEquipSerial = target.equipSerial;
}

} // namespace MimitaNet
