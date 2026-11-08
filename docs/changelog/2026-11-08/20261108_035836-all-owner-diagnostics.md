# FCS owner-level diagnostics

**Recorded:** 2026-11-08 03:58:36 UTC (2026-11-07 22:58:36 America/New_York)
**Branch:** `20261011zt`
**Commits:** none

## Final state

Added bounded NPC work-volume counters to the canonical structured event
stream. Existing relevant dirty work in this checkout was preserved; this
session only added the counters below. No confirmed regression cause or
performance fix is claimed.

### Source changes

- `src/network/server-npcs.cpp`, `simulateSharedNpcs` (target counters begin
  around line 1816; summary fields around lines 2702-2707): the existing
  once-per-second `npc.server-phase-window` now includes target scans,
  staggered scans skipped, forced refreshes after cached targets become
  invalid, player/NPC candidate visits, and total target-selection time.
  These distinguish expensive target work from the already-recorded AI,
  recovery, pair, respawn, rebuild, and total timings.
- `src/npc/npc.cpp`, `NpcSystem::updateOneNpc` (run counters around line 1492;
  event fields around lines 3294-3297): the existing one-second
  `npc.phase-window` now includes sensing and navigation run/skip counts,
  alongside the existing phase milliseconds, navigation candidate counts,
  and module generation.

### Existing source-level event coverage reviewed

- Server fixed-tick timing, loop rate/catch-up, stalls, and projectile workload:
  `src/network/server.cpp` (`performance.server-tick-window`,
  `performance.server-tick-stall`, `performance.server-loop-window`).
- NPC aggregate phases and actor-pair/collision candidate work:
  `src/network/server-npcs.cpp` (`npc.server-phase-window`, `npc.tick-cost`,
  `npc.pair-cost`, `actor.collision.profile`).
- NPC sense/decision/navigation/physics phase timing:
  `src/npc/npc.cpp` (`npc.phase-window`).
- Projectile and Deathball transitions/candidate workload:
  `src/network/server-projectiles.cpp` and the server loop summary.
- Client frame slices and network/snapshot health:
  `src/perf/perf-frame.cpp` and `src/network/multiplayer-packets.cpp`.

This is an audit of event call sites and payloads, not a claim that these
events were observed in a live journal during this session.

## Reason and constraints

The additional counters make an NPC slowdown attributable to scan frequency,
candidate volume, or target-selection time without per-actor/per-tick log spam.
They do not move authority, collision, physics, or file writing out of the
host. Existing game mechanics and shared writer behavior were not changed.

## Documents and focused review

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/logging-checker-v1.md` — PASS for source ownership, bounded
  summary cadence, and useful workload context; runtime event shape remains
  unobserved.
- `docs/specs/performance/performance.md`,
  `docs/skills/efficiency-checker-v1.md`
- `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/specs/networking/networking.md`,
  `docs/specs/gamemodes/funcounterstrike.md`,
  `docs/specs/weapons/deathball.md`
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/regressions/README.md`

## Validation and executable state

- `git diff --check -- src/network/server-npcs.cpp src/npc/npc.cpp` — PASS
  (line-ending conversion warning only).
- No gameplay tests or `events.jsonl` inspection were performed.
- A dev-loop build had been requested before the user's instruction to skip
  the build. The request returned failure (exit code 1); subsequent status
  showed the loop still building with stale source. The last published
  executable reported at that point was build
  `2026-11-08T03:24:24.129677Z`, SHA-256
  `3e221f872694a53bee8a8d8512c648b85b957a17fa996f2ee0e429aae2a81bd8`.
  This session did not verify a successful executable containing these
  diagnostics. The connected FCS run was left untouched.

## Human review still needed

Build/publish the current checkout before using these fields. Then inspect the
server's canonical `events.jsonl` during the reported FCS slowdown and compare
the one-second phase/work counters with tick and connection-health events.
The counters help locate the owner; they do not demonstrate that the slowdown
is fixed.
