# Header fan-out reduction plan

## Goal

Reduce the time from a C++ header edit to live activation by compiling only translation units that actually depend on the changed header, shrinking unnecessarily broad dependency surfaces, and avoiding a monolithic link where a stable module boundary is available. Preserve correct dependency tracking and the existing game-DLL fallback.

## Evidence at planning time

- Root dev-loop attempt `game-dll-20261124T171210403561Z-968a7e` was still active at the 2026-11-24 17:36 UTC status check. It began at 17:12:09 UTC and targets game generation 596 from source generation 17.
- Its initiating batch included `src/combat/weapon-types.h` and three other files. A direct-include search found 23 include sites for `weapon-types.h` in the combat tree. This is a direct-include count, not the full transitive translation-unit closure.
- `src/pch.h` does not directly include `weapon-types.h`; compiler commands do force-include `src/pch.h`. The actual `.d` dependency files must decide whether the changed header is in any PCH transitive closure.
- The attempt reported 110.453 seconds for `manifest_snapshot`, including 101.609 seconds for `build_manifest`, 24.659 seconds for `owner_resolver`, and 41.861 seconds for `header_digest`. It reported 64.500 seconds for `changed_source_preflight`, 8.781 seconds for `source_snapshot`, and 8.172 seconds for `object_cache_scan`.
- Only six files / 151,243 bytes were rehashed and 2,226 event-cache hits were reported. The long manifest subphase wall times may include host scheduling delay; they are not evidence that hashing six files took 100 seconds of CPU.
- The CPU sidecar for this same build reported host CPU near saturation, available RAM as low as 27 MiB, and 216 of 645 units compiled at 17:31 UTC. The current runtime was Bloodtest on `slaughterhouse1`, one NPC and one player, not the Juggernaut acceptance workload. A same-run sample around 17:31 showed severe runtime timing degradation. This is pressure correlation, not proof that a specific process caused the build delay.

## Plan

### 1. Close and preserve the active attempt

Let the root-owned build finish. Record its final source revision, changed paths, compiled and skipped translation units, link/publish duration, exact resource interval, and client/server generation activation. Do not start a competing build or profiler while it is active.

### 2. Build the authoritative header-to-TU map

Use GCC depfiles (`.d`) from the object cache as the authoritative transitive dependency list. For each changed header, report:

- direct includers and all transitive translation units;
- translation units actually recompiled and those safely skipped;
- the exact invalidation reason for every selected translation unit;
- whether the dependency entered through `src/pch.h`, a generated wrapper, a compiler/flag/toolchain identity change, a missing or corrupt object, a source race, or an explicit force rebuild.

Do not claim that a header caused the whole build until this closure is reconciled with the attempt log. In particular, measure the final closure for `weapon-types.h`; the current 23 direct includes are not enough to infer the number of affected objects.

### 3. Make invalidation reasons visible and cheap

Record `invalidated_by` for every translation unit, including the changed path and the cache-key fields that differed. Keep cache keys complete for source bytes, transitive headers, generated inputs, compiler binary/version, flags, PCH identity, external libraries, and linker inputs. Missing, stale, or corrupt index data must trigger a safe rebuild rather than a false skip.

Maintain a persistent reverse dependency index from compiler depfiles. Update it when file-change events arrive, then inspect only changed inputs and their dependents at build time. Run bounded full reconciliation periodically and after watcher overflow, compiler/flag/PCH changes, generated-file changes, or dependency-index version changes. Report reconciliation cost separately from the normal edit path.

### 4. Reduce header fan-out from measured high-cost owners

Rank headers by both dependent-TU count and aggregate compile CPU/wall time. For the highest-cost headers, make one behavior-preserving change at a time:

- replace implementation-only includes in headers with forward declarations where the type contract allows;
- move non-inline function bodies and heavy template definitions into `.cpp` files;
- separate stable IDs, enums, and small ABI/value contracts from volatile implementation details;
- keep frequently edited domain headers out of broad PCH inputs unless matched PCH measurements show a net win;
- use opaque handles or a stable module API at genuine domain boundaries, while keeping hot-loop data access allocation-free.

Start with the measured `weapon-types.h` closure. Split it only if the depfile map and compiler timing show a large avoidable closure. Preserve semantics, object layout, and all valid dependencies; do not use include removal as a way to suppress required rebuilds.

### 5. Run controlled header cases

After the active attempt and other agents' pending batch are complete, measure warm-cache cases through the root dev-loop:

1. one `.cpp`-only edit;
2. one header-only edit with a known dependent-TU set;
3. one high-fan-out header edit, selected from the measured reverse map;
4. one header edit outside the PCH closure as a control.

For each case, keep runtime mode, client/server pair, source tree, compiler flags, cache state, and resource collection comparable. Record host and process CPU/RAM throughout, actual sample cadence and sampler overhead, compile queue wait, each TU's CPU/wall time, link/publish time, and journal-confirmed activation. Separate scheduler delay from work performed by the build and sampler.

### 6. Introduce module boundaries after the dependency map is trustworthy

Use coarse NPC and vehicle DLL targets for edits within those domains so a behavior edit does not always relink the full game. Keep shared interfaces narrow and stable, move persistent state and object layout ownership into the host, and retain the monolithic game DLL as fallback. A changed cross-module ABI may still require a host build and must be reported separately.

## Acceptance gates

- For every tested edit, the compiler's selected translation units equal the verified dependency closure; every skipped unit has a recorded cache-freshness reason.
- A warm-cache header edit activates the intended behavior without a stale generation or crash, and the dependency scan/preflight and complete edit-to-activation time are reported separately.
- The full target is still 15–20 C++ files edited in one coordinated Mode 3 batch, live on a connected Juggernaut Horde server/client pair, under 10 seconds from edit to verified activation, with continuous host/process CPU and RAM evidence and matching DLL/module generations.

## Risks and current blockers

- Skipping required dependents can silently run stale code; depfile/index loss must fail closed.
- A PCH or command-line identity change may legitimately invalidate many objects; it needs an explicit reason, not a blanket exemption.
- Reducing a public header's surface may require domain API changes and can expose object-layout or ABI migration work.
- The current attempt's resource pressure and Bloodtest workload do not prove performance under Juggernaut Horde.
- The current root Mode 3 target model routes source changes to `game` or `cold`; separate module routing remains an implementation step.

## Terminal outcome from build attempt 968a7e

The later result confirms the fanout is worth addressing, but does not justify skipping valid dependents. Attempt `game-dll-20261124T171210403561Z-968a7e` ended in compile failure at 17:36:53.955Z, before link or publish. It examined 645 translation units, recorded 275 compile results (273 compiled and 2 failed), and spent 1179.360 seconds in compile. Of 271 invalidations, 264 (40.9% of all units) were explicitly due to `src/combat/weapon-types.h` being newer than the cached object. The other causes were five `vehicle-types.h`, one `vehicle-profile.h`, and one additional `vehicle-types.h` spelling. These are legitimate depfile freshness decisions until proven otherwise.

This attempt did not produce generation 596; generation 595 remained live. The two compile failures were vehicle profile/system code referencing jump-profile members missing from the captured header; a subsequent root retry failed preflight on `vehicle-system.cpp`. The sampled build window was nearly fully CPU-loaded and reached about 27 MiB available RAM, on a Bloodtest 1-NPC run with client mode unverified. Pressure is correlated with the long build, not established as its cause. No Juggernaut runtime activation or latency acceptance was demonstrated.

Concrete narrowing lead: `src/actor/player-ai.cpp` includes `combat/weapon-registry.h`, which includes `weapon-types.h`; player AI reads `WeaponDefinition.behaviorType`. Investigate a narrow behavior-classification API for this consumer, and audit the remaining dependents against actual type use before splitting declarations. Keep the existing depfile graph as the correctness oracle while designing a persistent reverse index; require the index to match a full depfile audit before it can drive incremental invalidation.
