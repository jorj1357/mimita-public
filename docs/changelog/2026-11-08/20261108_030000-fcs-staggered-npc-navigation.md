# FCS server workload scheduling and reuse

## Change

FCS server NPC sensing, target selection, and route replanning now run on a
three-tick per-NPC schedule. The last target/sensor/navigation snapshot is used
between updates, accumulated simulation time is supplied to sensing and route
updates, and dead/disconnected/hostile-invalid targets trigger immediate
reacquisition. Respawn invalidates target, sensing, and navigation caches.
Movement, traversal progression, physics, collision, combat, and server
authority continue every fixed tick. Other modes retain their existing
per-tick NPC thinking cadence.

Actor-pair broadphase candidate pairs are deduplicated when generated, before
the configured candidate cap is applied. This avoids storing and sorting the
same pair repeatedly when actors share multiple broadphase cells; final pair
ordering remains deterministic. FCS projectile hazard snapshots now reuse a
thread-local vector buffer across ticks, in addition to the NPC system's
retained-capacity copy. Projectile physics, collision, and damage still run
every tick; its existing swept-triangle candidate reuse path is unchanged.

Other modes keep replanning each tick. This change schedules navigation only;
it does not establish that navigation was the sole or remaining cause of the
measured FCS overload.

## Build evidence

Dev-loop build published successfully at `2026-11-08T03:15:36.150786Z` with
SHA-256 `d9b31a254c2a0093ee61626b3ee6c99c65078daf422b97c16bed5013423ee303`.
Artifact: `.dev/builds/2026-11-08T03-15-36.150786Z/mimita.exe`.
Build identity reports source generation 41 and
`source_changed_during_build=false`. No runtime trial or gameplay tests were
run, as requested. The existing connected run remains on its prior
executable.

## Review still needed

When the user runs this artifact, look for continued NPC movement/combat,
stable client responsiveness, and no slowdown or reconnect over several
minutes. The compile result alone does not establish a performance improvement.
