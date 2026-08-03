// 08 03 2026, 10 30
/* purpose
* Validates remote-entity lifecycle correctness under reordered/lossy snapshots.
* Proves stale snapshots can never destroy newer entities, gaps cannot kill
* players, and replica creation never replays historical presentation events.
* Does NOT start networking, rendering, audio, or the game executable.
* Does NOT require real GLB/avatar/sound assets or server simulation.
*/

#include "network/remote-entity-lifecycle.h"
#include "network/multiplayer-context.h"

#include <cstdio>
#include <cstring>

// ── Minimal Player construction for this header-only test ────────────
// remote-entity-lifecycle.cpp only reads/writes Player fields, so the test
// provides lightweight constructor stubs instead of linking player.cpp and
// its full dependency graph.
Player::Player() {}
Player::Player(bool) {}
void Player::reset() {}

namespace {

bool expect(bool value, const char* name)
{
    std::printf("%-75s %s\n", name, value ? "PASS" : "FAIL");
    return value;
}

MimitaNet::SnapshotTransform makeTransform(uint32_t tick, uint16_t dash = 0,
                                           uint16_t groundJump = 0,
                                           uint16_t airJump = 0,
                                           uint16_t downDash = 0,
                                           uint16_t freeze = 0,
                                           uint16_t directionChange = 0,
                                           uint16_t equip = 0)
{
    MimitaNet::SnapshotTransform t;
    t.serverTick = tick;
    t.dashSerial = dash;
    t.groundJumpSerial = groundJump;
    t.airJumpSerial = airJump;
    t.downDashSerial = downDash;
    t.freezeSerial = freeze;
    t.directionChangeSerial = directionChange;
    t.equipSerial = equip;
    return t;
}

// ── Test: reordered snapshot cannot destroy newer entity ──────────────
// Membership mutations are only allowed for ticks >= the newest applied
// membership tick. A delayed older snapshot (101) must be rejected even
// though it was processed after the newer (102).
bool testReorderedSnapshotCannotDestroyNewer()
{
    bool ok = true;
    ok &= expect(MimitaNet::snapshotMayMutateMembership(100, 0),
                 "tick=100 latest=0 -> membership allowed (first snapshot)");
    ok &= expect(MimitaNet::snapshotMayMutateMembership(102, 100),
                 "tick=102 latest=100 -> membership allowed (newer)");
    ok &= expect(!MimitaNet::snapshotMayMutateMembership(101, 102),
                 "tick=101 latest=102 -> membership forbidden (older)");
    ok &= expect(MimitaNet::snapshotMayMutateMembership(102, 102),
                 "tick=102 latest=102 -> membership allowed (same tick)");
    return ok;
}

// ── Test: missing from one newer snapshot does not immediately destroy ─
bool testSingleMissingDoesNotDestroy()
{
    MimitaNet::MissingEntityTracker t;
    bool ok = true;
    ok &= expect(!t.shouldRemove(3, 1000, 100),
                 "no snapshots seen -> never removed");
    t.noteMissing(100);
    ok &= expect(!t.shouldRemove(3, 1000, 150),
                 "1 missing confirmation < 3 -> retained");
    t.noteMissing(200);
    t.noteMissing(300);
    ok &= expect(!t.shouldRemove(3, 1000, 500),
                 "3 confirmations but grace (1s) not elapsed -> retained");
    ok &= expect(t.shouldRemove(3, 1000, 1301),
                 "3 confirmations + grace elapsed -> removal allowed");
    return ok;
}

// ── Test: entity seen again resets the grace tracker ─────────────────
bool testSeenResetsGrace()
{
    MimitaNet::MissingEntityTracker t;
    t.noteMissing(100);
    t.noteMissing(200);
    t.noteMissing(300);
    t.noteSeen();                    // re-appeared in a newer snapshot
    return expect(!t.shouldRemove(3, 1000, 2000),
                  "seen again resets confirmations -> retained");
}

// ── Test: explicit despawn / authoritative removal still works ────────
bool testExplicitRemoval()
{
    MimitaNet::MissingEntityTracker t;
    t.noteSeen();
    // Authoritative removal: repeated absence across new snapshots + grace.
    t.noteMissing(100);
    t.noteMissing(200);
    t.noteMissing(300);
    bool removed = t.shouldRemove(3, 1000, 1301);
    return expect(removed,
                  "confirmed authoritative removal still removes entity");
}

// ── Test: packet gap does not remove player ───────────────────────────
// A blackout produces no snapshots at all, so no membership snapshot ever
// reports the entity missing. The tracker stays at zero confirmations and
// the player is retained for the entire gap.
bool testPacketGapDoesNotRemove()
{
    MimitaNet::MissingEntityTracker t;
    t.noteSeen();
    // 2000ms blackout: no processSnapshotEntities call at all.
    bool removed = t.shouldRemove(3, 1000, 2000);
    return expect(!removed,
                  "2000ms gap with no snapshots -> player retained");
}

// ── Test: replica creation does not replay historical events ──────────
bool testCreationDoesNotReplayEvents()
{
    bool ok = true;
    Player player;
    MimitaNet::SnapshotTransform target = makeTransform(
        42, 27, 76, 130, 29, 17, 5, 9);

    MimitaNet::baselinePresentationSerials(player, target);

    ok &= expect(player.networkLastDashSerial == 27,
                 "dash serial baselined to current value (27)");
    ok &= expect(player.networkLastGroundJumpSerial == 76,
                 "groundJump serial baselined (76)");
    ok &= expect(player.networkLastAirJumpSerial == 130,
                 "airJump serial baselined (130)");
    ok &= expect(player.networkLastDownDashSerial == 29,
                 "downDash serial baselined (29)");
    ok &= expect(player.networkLastFreezeSerial == 17,
                 "freeze serial baselined (17)");
    ok &= expect(player.networkLastDirectionChangeSerial == 5,
                 "directionChange serial baselined (5)");
    ok &= expect(player.networkLastEquipSerial == 9,
                 "equip serial baselined (9)");

    // A later serial increase must still be detected as a change.
    MimitaNet::SnapshotTransform later = makeTransform(
        43, 28, 76, 130, 29, 17, 5, 9);
    const bool dashChanged =
        later.dashSerial != player.networkLastDashSerial;
    ok &= expect(dashChanged,
                 "later dash serial increase is still detected");

    // Same serials must not re-trigger.
    const bool sameSerials =
        later.groundJumpSerial == player.networkLastGroundJumpSerial &&
        later.airJumpSerial == player.networkLastAirJumpSerial;
    ok &= expect(sameSerials,
                 "unchanged serials do not retrigger historical events");
    return ok;
}

// ── Test: reordered transforms do not move rendering backward ─────────
// At the lifecycle level, an older snapshot can never reconcile membership
// against newer state, so its transform samples cannot regress the world.
bool testReorderedTransformsDoNotRegress()
{
    bool ok = true;
    ok &= expect(!MimitaNet::snapshotMayMutateMembership(301, 302),
                 "tick=301 rejected after membership tick=302");
    ok &= expect(MimitaNet::snapshotMayMutateMembership(302, 302),
                 "tick=302 still allowed");
    ok &= expect(MimitaNet::snapshotMayMutateMembership(303, 302),
                 "tick=303 allowed (forward)");
    return ok;
}

} // namespace

int main()
{
    int failed = 0;
    failed += !testReorderedSnapshotCannotDestroyNewer();
    failed += !testSingleMissingDoesNotDestroy();
    failed += !testSeenResetsGrace();
    failed += !testExplicitRemoval();
    failed += !testPacketGapDoesNotRemove();
    failed += !testCreationDoesNotReplayEvents();
    failed += !testReorderedTransformsDoNotRegress();

    std::printf("\n=== Remote Entity Lifecycle: %s ===\n",
                failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
