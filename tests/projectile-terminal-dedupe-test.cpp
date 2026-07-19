// 07 19 2026, 12 10
/* purpose
* Verify client authoritative projectile terminal deduplication rules.
* Covers first terminal, duplicate terminal, late correction, and late spawn tombstones.
* Exercises the generic policy used by rocket and grenade terminal packets.
* Does NOT simulate projectile physics, render effects, or apply server damage.
* Does NOT create weapon-specific packet handling or client prediction.
* Does NOT depend on OpenGL, audio, or a live network transport.
*/

#include "network/projectile-terminal-dedupe.h"

#include <cstdio>

static bool expect(bool value, const char* name)
{
    std::printf("%-48s %s\n", name, value ? "PASS" : "FAIL");
    return value;
}

int main()
{
    MimitaNet::ProjectileTerminalDedupe dedupe(3);
    bool ok = true;

    ok &= expect(!dedupe.has(28), "new projectile is not tombstoned");
    ok &= expect(dedupe.record(28), "PlayerImpact terminal records once");
    ok &= expect(dedupe.has(28), "PlayerImpact removes/resurrection-blocks id");
    ok &= expect(!dedupe.record(28), "duplicate terminal is ignored");
    ok &= expect(dedupe.record(29), "WorldImpact terminal uses same path");
    ok &= expect(dedupe.record(30), "Lifetime terminal uses same path");
    ok &= expect(!dedupe.record(0), "invalid id is ignored");
    ok &= expect(dedupe.record(31), "bounded tombstone accepts newer id");
    ok &= expect(!dedupe.has(28), "oldest tombstone evicted at bound");
    ok &= expect(dedupe.has(29) && dedupe.has(30) && dedupe.has(31),
                 "new tombstones remain active");
    dedupe.clear();
    ok &= expect(dedupe.size() == 0 && !dedupe.has(31), "clear resets session state");

    std::printf("\n=== Projectile Terminal Dedupe: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
