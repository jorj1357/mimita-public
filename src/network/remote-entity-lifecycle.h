// 08 03 2026, 10 30
/* purpose
* Declares pure helpers for remote-entity membership ordering and removal grace.
* Gates destructive snapshot reconciliation behind authoritative tick ordering.
* Tracks missing-snapshot confirmations so brief packet gaps cannot kill players.
* Does NOT poll sockets, parse packets, render, play audio, or own server state.
* Does NOT simulate movement, spawn avatars, or own the player registry.
*/

#pragma once

#include <cstdint>

struct Player;

namespace MimitaNet {

struct SnapshotTransform;

// A snapshot may mutate entity membership (create/destroy/remove/revive)
// only when it is not older than the newest already-applied membership
// snapshot. Older snapshots may still feed interpolation but must never
// reconcile world membership against newer state.
bool snapshotMayMutateMembership(uint32_t serverTick,
                                 uint32_t latestAppliedMembershipTick);

// Per-entity missing-from-snapshot grace tracker. Note each complete
// membership snapshot the entity was absent from; removal requires the
// configured confirmation count AND grace window to both elapse.
struct MissingEntityTracker
{
    void noteSeen();
    void noteMissing(uint64_t nowMs);
    // Returns true only when absent across >= confirmationCount membership
    // snapshots for >= graceMs of wall-clock time.
    bool shouldRemove(uint32_t confirmationCount, uint64_t graceMs,
                      uint64_t nowMs) const;

    uint32_t confirmations = 0;
    uint64_t firstMissingMs = 0;
};

// Seeds all remote presentation serial baselines from the first observed
// authoritative target, so creating/resyncing a replica never replays
// historical one-shot effects (dash, jumps, freeze, direction change).
// Only a later serial increase triggers a sound/effect.
void baselinePresentationSerials(Player& player,
                                 const SnapshotTransform& target);

} // namespace MimitaNet
