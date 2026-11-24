# Gold evidence: Juggernaut Horde NPCs felt overwhelming

Date: 2026-11-24 (UTC)
Evidence class: **human acceptance / gold behavior**
Mode: Juggernaut Horde
Map: `dust2cyberiav4`

## Human observation (verbatim)

> i played against teh npcs in juggernaut horde and it was overwhelming so this is gold behavior

## What this proves

This is positive human acceptance of the elite NPC pressure:

- the player experienced continuous hostile pressure;
- NPC crowds and engagements felt overwhelming, not passive or easy;
- the behavior is the desired direction for elite NPC combat.

It does **not** prove a numeric difficulty target, a specific kill count, or a
performance result. Human acceptance is a separate evidence layer from the
runtime journal.

## Candidate pinned runtimes

**Live session observed during this work:**

- Run: `20261124_022440_01`
- Mode: Juggernaut Horde
- Map: `dust2cyberiav4`
- Roster: 67 NPCs / 1 player (68 participants)
- Server PID: 33892
- Server journal:
  `logs/2026-11-24/20261124_022440_01/server-01/events.jsonl`
- Client journals: `client-01`, `client-02` in the same run directory
- Evidence: `devscripts/evidence-watch.py list/show` mapped the live PID to the
  exact journal while the run was active around 2026-11-24T02:41Z.

**Earlier preserved session:**

- Run: `20261123_195307`
- Mode: Juggernaut Horde
- Map: `dust2cyberiav4`
- Roster: 67 NPCs / 1 player
- Server PID: 22184
- Journal: `logs/2026-11-23/20261123_195307/server-01/events.jsonl`
- Game DLL: generation 567 / pinned EXE build recorded in that run's `run.json`

Related measured context from `resource-profiler` (not acceptance):

- `performance.server-tick-window`: sample 15, total average 59.232 ms,
  p95/p99/max 131.759 ms, NPC average 53.901 ms / max 129.369 ms, 26.7% of
  sampled ticks over 100 ms.
- `npc.phase-window`: 1,027 updates, total 459.736 ms, physics 239.569 ms,
  collision 141.699 ms, decision 39.155 ms.
- Callsite estimates: `updateOneNpc` at `src/npc/npc.cpp:1580` about 12.40% of
  server CPU (~3.043% host); `doCollisions` at
  `src/physics/movement/physics-collision-dispatch.cpp:181` about 5.33%
  (~1.308% host). These are partial 1/64 estimates, not full attribution.

If neither `20261124_022440_01` nor `20261123_195307` was the exact session the
player refers to, keep this record as a human-only report and attach the correct
journal later.

## Why this is gold behavior

- The player did not describe NPCs as passive, stuck, or easy.
- The pressure came from the shared NPC combat/movement/objective path, not a
  scripted cinematic or a forced hit.
- It is the desired “elite feels dangerous” outcome, and the next step is to
  preserve it while reducing wall/stuck time and improving objective flow.

## Limits and follow-up

- Human feel is not deterministic evidence; repeat with the same map/mode and
  record kills, deaths, TTK, round outcomes, and player skill assumptions.
- Preserve this as a positive regression baseline: future tuning must not make
  Juggernaut Horde passive again.
- Link the exact run/journal to the NPC feature dossier and the canonical NPC
  issue when confirmed.

## Links

- `docs/features/npc-behavior.md`
- `docs/workflows/measurement-hypothesis-driven-improvement.md`
- `docs/regressions/npc-movement/`
- `issue-be1adb51b6ac`
- `docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md`
