# Remove repeated empty NPC AimBody initialization

- Task ID: server-npc-empty-aimbody-fix
- Summary: Fix the low-Hz server regression caused by an unused NPC AimBody
  path repeatedly rebuilding an empty body.
- Status: build verified; live 67-NPC runtime proof pending
- Date, time, timezone: 2026-10-10T20:44:33-04:00, America/New_York

## Root cause

The fresh journal
`C:\mimita-v9\logs\2026-10-11\20261011_003742\events-000001.jsonl`
contains approximately 82,958 server-side `actor-hybrid.pose-input` events.
Each event reports `parts: 0`. The condition in
`RagdollModeSystem::updateNpcAim` was `body.parts.empty()`, so the server
re-entered `buildAimBody` and emitted an IMPORTANT initialization event on
every NPC update when construction produced no parts.

This is why the visible NPCs still used normal movement: no ragdoll body was
successfully constructed. The server nevertheless paid for the failed path and
flooded the canonical journal while the fixed-tick NPC scope was already
overloaded.

## Source change

In `src/npc/npc.cpp`:

- Removed the `ragdoll/ragdoll-mode.h` dependency.
- Removed the dead-NPC `clearNpcAim` call.
- Removed the per-NPC `RagdollModeSystem::updateNpcAim` call and its synthetic
  aim-point setup.

The normal server-authoritative NPC movement, collision, targeting, navigation,
and combat code remains in place. The client/player AimBody implementation was
not changed.

## Validation

- `git diff --check`: passed; only pre-existing line-ending warnings were
  reported.
- Canonical build: SUCCESS; one changed C++ translation unit compiled and the
  executable linked.
- Version info: SUCCESS from `C:\mimita-v9\mimita.exe --versioninfo`.
- Version-info journal:
  `C:\mimita-v9\logs\10-10-2026\20261010_204433\events-000001.jsonl`
- Live gameplay acceptance: not yet performed. A real 67-NPC run must verify
  the repeated server `actor-hybrid.pose-input` events are gone and measure
  restored fixed-tick timing.

## Regression record

Updated:
`docs/regressions/2026-10-10/server-npc-simulation-tick-rate-REG.md`
to `ATTEMPTED FIX (1)`. The prior user-owned working-tree changes were
preserved.
