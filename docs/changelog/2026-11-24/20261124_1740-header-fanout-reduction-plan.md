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

## Live attempt 2aff70 dependency and queue evidence

During active attempt `game-dll-20261124T174700746286Z-2aff70`, the raw build JSONL contained 87 `cache.invalidation` records at inspection. Grouping equivalent relative and absolute dependency spellings gives 60 translation units invalidated by `src/gamemode/map-config.h`, 19 by `src/physics/physical-entity.h`, seven by `src/vehicles/vehicle-types.h`, and one by `src/input/gamepad-input.h`. The input header is not in this attempt context's seven `changed_paths`; its timestamp invalidation needs reconciliation before attributing it to this batch. This is depfile-backed evidence for broad header closure in this attempt, but it remains necessary work unless interface usage proves a dependency unnecessary.

At 18:02:44.656Z, 589/645 units had been checked, with 36 compiled and 553 skipped; at 18:10:17.069Z, 607/645 had been checked, with 54 compiled and 553 skipped. Host CPU was 93.0–95.8%, available RAM 135–285 MiB. The 18:11:57Z worker records show `server-physical-contact.cpp` took 170.633s compiler time plus 975.547s queue wait, and `server-melee-actions.cpp` took 338.786s compiler time plus 864.453s queue wait. These measurements separate multi-minute compiler time from long worker-queue residence, but do not yet explain either cause. At the last inspected resource sample, tracked compiler/linker processes summed 4.08% of whole-host CPU and MiMITA processes 29.51%; tracked and top-process rows did not account for all reported host CPU, so process attribution is incomplete. The build was still active at this checkpoint and the runtime context was Bloodtest/slaughterhouse1/1 NPC, not the Juggernaut acceptance workload. Await terminal attempt data before drawing a result.

At a later raw-worker-log read, 70 successful TU completion records were present. Their compiler-time median was 188.716s, p95 359.611s, max 512.089s; queue-wait median 393.415s, p95 1266.391s, max 1384.860s. Longest compiler records included `src/network/server-npcs.cpp` (512.089s compile +872.8s queued), `src/network/server-packets.cpp` (454.0s +919.3s), and `src/mapeditor/map-editor.cpp` (433.3s +486.0s). Sidecar progress can lag worker JSONL by a few completions, so these counts are taken from worker records. The attempt remained active; these are not terminal build totals.

## Fast-fail dependent compilation (2026-11-24)

The `968a7e` failure showed why a long header build could fail near the end:
the existing preflight selected explicitly changed `.cpp` files, while stale
`.cpp` dependents of changed headers were found only after object-cache
discovery. In `968a7e`, `vehicle-profile.cpp` and `vehicle-system.cpp` each
spent about 19 minutes queued before failing; their actual compiler work took
16.5s and 20.8s. A later one-source attempt `4b9969` confirms direct `.cpp`
preflight already catches that class of direct-source error early.

The worker now uses cached depfile tokens to rank stale translation units
referencing changed headers ahead of unrelated stale units after the full
correctness scan. It preserves the stale set and compiles every unit once;
this changes scheduling order only. It records priority counts, a bounded path
list, and ordering duration. It does not provide a hard failure deadline or
catch linker errors before linking. The focused test passed (2/2), worker and
test `py_compile` passed, and `git diff --check` passed. No compiler or game
build was run, and no intentionally broken source was added to the shared
checkout. Runtime activation of the worker change is not yet proven.

## Practical header-hygiene roadmap (2026-11-24)

### What the evidence means

`weapon-types.h` is the first high-value target: attempt `968a7e` had 264 of
645 translation units (40.9%) invalidated because that header was newer than
their cached objects; the current cached reverse map lists 267 dependents.
That is a real dependency closure, not evidence that 264 compiles are
unnecessary. A separate measured header case in attempt `2aff70` attributed
60 stale units to `map-config.h`, 19 to `physical-entity.h`, and seven to
`vehicle-types.h`; a direct timestamp reason for `gamepad-input.h` also needs
reconciliation. Do not combine counts from different attempts as if they were
one build.

A later one-`.cpp` control, attempt
`game-dll-20261124T192206880822Z-db2cf7`, completed in 15.177s: changed-source
preflight 9.197s, compile phase 0.429s, link 2.120s, publish 1.184s, and
object-cache scan 0.011s. This is one successful `vehicle-system.cpp` edit,
not a header benchmark or a pressure-matched run. It demonstrates that the
current sub-10-second goal cannot be reached by compile fan-out reduction
alone: preflight, link, and publish already sum to about 12.5s in this sample.
First remove avoidable fan-out; then separately reduce/replace the monolithic
link and fixed per-attempt costs. Keep this sample's unverified runtime
alignment caveat.

### Recommended sequence

1. **Measure before splitting.** Build one normalized depfile-to-header reverse
   index and rank each project header by dependent TU count, aggregate compile
   CPU/wall time, and observed edit frequency. For every invalidation, report
   its cause (source/header/PCH/toolchain/flags/generated input/missing object)
   and name the exact dependent header. Deduplicate relative and absolute path
   spellings. Do not use direct `#include` counts as transitive fan-out.
2. **Audit the top header's actual consumers.** For each TU depending on
   `weapon-types.h`, classify whether it needs a complete `WeaponDefinition`,
   only a stable enum/ID, or only a narrow query such as `behaviorType`. The
   known `src/actor/player-ai.cpp` consumer reaches the header through
   `weapon-registry.h` and reads `WeaponDefinition.behaviorType`; test a narrow
   read-only classification/API boundary for this case. Record remaining
   consumers and their required fields before changing declarations.
3. **Remove only implementation coupling.** In one behavior-preserving batch,
   replace header includes with forward declarations only for pointer/reference
   use; move non-inline bodies out of headers; move volatile implementation
   details behind `.cpp` owners; and put stable small enums/IDs/value contracts
   in a minimal leaf header. Keep complete types where layout, by-value use,
   inline code, or templates require them. Do not hide a dependency or retain
   a stale object to improve the metric.
4. **Keep correctness checks authoritative.** Compile every TU in the depfile
   closure of a changed header. Every skip must have current source/dependency,
   toolchain, flags, generated-input, and PCH identities. The persistent index
   may make the lookup incremental, but a cache miss, watcher overflow,
   schema/toolchain change, or periodic reconciliation must fail closed and
   compare against compiler depfiles. A smaller dependency graph is the win;
   skipping a still-valid dependency is not.
5. **Run a controlled progression.** With a warm cache and one coordinated
   Mode 3 request per case, compare: (a) one `.cpp` change, (b) a leaf-header
   change, (c) the refactored top-fan-out header, and (d) a no-op/control. Keep
   compiler, flags, PCH, machine load, and live runtime fixed. Record exact
   changed/compiled/skipped TU lists, dependency closure, per-TU compile time,
   preflight, scan, link, publish, resource samples, and DLL activation. Reject
   any optimization that causes a stale behavior or omits a true dependent.
6. **Treat PCH and modules as separate experiments.** A PCH can reduce repeated
   parsing but does not erase semantic dependents; changing its content can
   invalidate nearly every TU. Test PCH membership/cost independently. Only
   after the graph is clean should the team prototype coarse NPC/vehicle DLL
   targets with stable narrow interfaces; this can avoid unrelated domain
   compiles and perhaps a full-game link for domain-local edits. Preserve the
   monolithic MinGW DLL fallback. A changed shared ABI may still fan out or
   require a cold host build.

### What not to do

- Do not turn off dependency scanning or skip header dependents just because
  the user edited one file.
- Do not add every frequently edited header to the PCH to make its immediate
  compile look fast; account for PCH invalidation and rebuild cost.
- Do not split headers by file size alone; choose by avoidable dependency
  edges and measured recompilation cost.
- Do not expect header cleanup to make full 15–20-file edit-to-live latency
  sub-second on today's monolithic link path; the measured 2.120s link plus
  1.184s publish and 9.197s preflight already exceed the ten-second budget.

### First prototype and success criteria

The first code prototype to evaluate should be extracting `WeaponRuntime`
from `weapon-types.h` into a small leaf header and including that header from
`entities/player.h`. Current source evidence: the depfile index has 267 TUs
depending on `weapon-types.h`; a source include-graph walk found a path through
`entities/player.h` for 180. `player.h` explicitly includes `weapon-types.h`,
and the targeted symbol search found `WeaponRuntime` as its only use from that
header. `WeaponRuntime::reset` takes `const WeaponDefinition&`, which can be
forward-declared in a runtime-state header; the out-of-line implementation
would retain the complete definition. This is a hypothesis from source
inspection, not proof that all 180 depfile edges disappear: some TUs may have
another include path to the definition. The parser also does not replace
compiler depfiles.

A second source-graph pass excluding just the `player.h` include edge still
found a quoted-include route to `weapon-types.h` for 184 of the 267 TUs; 83
appear to lose their last parsed source-include route. This is only a static
estimate. Compiler depfiles remain authoritative and may expose generated,
angle-bracket, PCH, or other paths missed by this parser. The migration build
will recompile users of `player.h`; the expected benefit is fewer dependents
on later edits to `WeaponDefinition`, not a free change in the first build.

Before any edit, capture the complete depfile consumer list, exact include
paths, and a baseline build. Check current ownership of `weapon-types.h` first;
it has an unrelated active field change and must not be overwritten. Move
only the runtime struct in the prototype, preserve its fields and behavior,
and keep compatibility includes where existing consumers require them. After
the root Mode 3 build, compare the new depfile closure against the old one and
confirm that each removed edge is truly unnecessary; explicitly include the
new leaf in each remaining consumer that needs the complete runtime state.
Require no unexplained stale skips, fewer selected TUs on a later
`WeaponDefinition`-only header edit, unchanged runtime behavior, reduced total
compile CPU, and the same measured live-generation proof. If compile work
falls but end-to-live remains above 10s, proceed to the separately measured
preflight/link path and coarse domain-DLL prototype rather than weakening
dependency checks.
