# NPC AimBody hybrid and triangle collision migration slice

Date: 2026-10-08
Status: PASS_WITH_HUMAN_REVIEW

## Scope completed

- Added a normative NPC AimBody hybrid section to `docs/architecture/collision/actor-vs-actor-collisions.md`.
- Generalized the existing player AimBody step into a shared `stepAimBody` owner.
- Added per-NPC AimBody state and lifecycle cleanup. NPC aim uses target direction when a target exists and facing direction otherwise; it does not use camera or literal RMB state.
- NPC body output is written through the same hybrid physical solve and model-transform path as the player.
- Removed the explicit NPC exclusion from the opt-in triangle world-collision backend.
- Added bounded `actor-hybrid.pose-input`, `actor-hybrid.pose-final`, and explicit `actor-collision.fallback` structured events using the canonical logger.

## Exact files changed by this session

- `docs/architecture/collision/actor-vs-actor-collisions.md`
- `src/ragdoll/ragdoll-mode.h`
- `src/ragdoll/ragdoll-mode.cpp`
- `src/npc/npc.cpp`
- `src/physics/movement/physics-collision-glb-main.cpp`

## Evidence

- Source compilation and link: `python build_agent.py` succeeded after one fixed compile error.
- Timestamped executable: `mimita-20261008T1635-npc-hybrid.exe`.
- Startup/version evidence: `--versioninfo` succeeded and emitted `EVENTS_JSONL_PATH=logs/10-08-2026/20261008_163502/events.jsonl`.
- `git diff --check` reported only pre-existing trailing whitespace in `docs/jorj-docs/20261008plan.md`; no new whitespace error was introduced by this slice.

## Not proven by this slice

- There is not yet one server-side actor-pair broadphase/manifold response owner for player/player, player/NPC, NPC/NPC, and ragdoll contacts.
- Existing network transport has not yet been extended with generic actor kind/lifecycle hybrid pose records.
- Support inheritance, actor-vs-actor response, server validation/reconciliation, deterministic component tests, swarm scenarios, latency scenarios, and human gameplay acceptance remain open.
- Runtime `events.jsonl` from an actual NPC gameplay scenario has not been observed in this session; the version-info run proves executable identity and logger path only.

## Pre-existing work preserved

The working tree already contained user edits across configuration, avatar, combat, engine, gamemode, networking, NPC, ragdoll, planning, and multiple changelogs. Those changes were not reset, overwritten, or claimed as part of this slice.
