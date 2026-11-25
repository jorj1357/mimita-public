# Header fan-out plan for edit-to-live latency

Date: 2026-11-25
Agent: root-header-fanout-plan
Issue: issue-136d4553ff92
Related issue: issue-196c3045b4cb

## Request

The user asked for a plan to trace and reduce header fan-out so that editing a
header does not turn into a very long build.

## Findings

- Generation 634 compiled 55 of 645 units in 181.396 seconds. Its invalidation
  records attribute 40 TUs to `src/network/packets.h` and 4 to
  `src/network/server.h`; 9 objects were missing and 1 was empty.
- Generation 635 compiled 17 and skipped 628 in 255.459 seconds. Its
  `src/combat/weapon-fire.h` change caused 13 exact dependency-newer
  invalidations (the build metadata reports 16 dependents). The recorded
  phases include 116.777 seconds changed-source preflight, 95.041 seconds
  remaining compile work, 26.332 seconds linking, 2.576 seconds object-cache
  scan, and 0.948 seconds dependency prepass.
- A prior NPC debug-flag split reduced the affected set from 284 sources for
  `src/config.h` to 6 for `src/debug/config-npc.h`. Its 829.795-second build
  compiled 271 units under heavy resource pressure, so the graph reduction is
  proven but the build-time benefit is not isolated.
- The current generated graph lists `src/pch.h` with 628 dependents. It is a
  stable third-party/system PCH and is not a split target. Other large fan-in
  values include `structured-log.h` (337), `physics-types.h` (291),
  `map-mesh-types.h` (280), and `config.h` (272). Fan-in is not identical to
  actual invalidation count.

## Recommended sequence

1. Snapshot the exact compiler `.d` graph, source/build generation, header
   hash, affected TU list, invalidation reason per object, and last compile
   duration before each header experiment.
2. Rank by measured invalidated TUs and their compile time, not include count
   alone. First pilot: one `src/network/packets.h` seam, after its current
   owner releases it. Fallback: one `src/combat/weapon-fire.h` seam.
3. Keep each pilot behavior-neutral. Move heavy includes into `.cpp` files,
   forward-declare where valid, separate stable type/schema declarations from
   owner interfaces and implementation details, and keep project headers out
   of `src/pch.h`. Do not change packet layouts or live object layouts during
   a fan-out-only experiment.
4. Rebuild every TU that the dependency graph says is affected. Compare the
   before/after fan-in, actual invalidation list, compile wall/CPU time, and
   the full attempt timeline. Do not claim success from a smaller compile
   count if preflight or link dominates.
5. Continue the existing header-hygiene tranches: NPC/debug and dimension
   headers; player/actor boundaries; physics, movement, weapon, map and avatar
   vocabulary; packet aggregates; cold ABI/logging headers last.
6. Treat the monolithic link and preflight as separate latency work. The
   measured link alone was 15.410 seconds in generation 634 and 26.332 seconds
   in generation 635, so header splitting alone cannot meet a 10-second
   edit-to-live target. A stable component/module link boundary needs its own
   experiment and runtime activation proof.

## Ownership and verification

The existing `header-fanout-plan` agent is running a current compiler-graph
census. This session did not start another graph scan, edit compiler inputs,
request a build, or change a runtime. The next safe action is to use that one
census, confirm source ownership, and run one isolated header pilot through the
root dev-loop.

The latency issue now links this plan to the existing header-fan-out issue.
No runtime benefit is claimed from this documentation-only work.

## Additional source-verified activation blocker

At 2026-11-25T04:17Z, a read-only source review explained the already-recorded
host DLL generation 635 versus NPC module generation 632 mismatch. The
resident `ModuleHost` resolves local module exports from the active game DLL
once, stores each module's generation, then skips both repeated startup and
poll refresh for local modules. `HostRuntime` commits a new game DLL without
rebinding those local exports. The host loader retains old DLL images, so the
finding is a stale module API/generation, not proof of an unmapped function
pointer.

The proposed correction is a transactional resident rebind when the new game
generation calls module startup: quiesce calls, validate and initialize the
new exports, migrate any state supported by the module ABI, publish the new
entries only after success, and retain the previous active entries on failure.
This resident fix likely needs a one-time EXE build/restart; subsequent game
DLL generations should refresh the folded NPC module. Acceptance requires
same-run server events showing host and NPC module generations equal after a
swap, plus an NPC behavior event proving the edited logic ran without a crash
or stale callback. No implementation or build was performed.

At 2026-11-25T04:16:51.723Z, a single WMI point reported 508,641,280 bytes
free of 16,802,918,400 on a 12-logical-CPU host. The root dev-loop inventory
showed three live pairs: Juggernaut registry run `20261125_014732`
(server36452/client25552) and Bloodtest runs `20261125_020100`
(13336/36632) and `20261125_022449` (6244/38720). The registry mode is not
independent in-game mode proof at that instant; CPU rates and exact player/NPC
counts are unknown. No build or capture was active from this session. The
available RAM is below the 2 GiB build floor, so the rebind build remains held.
