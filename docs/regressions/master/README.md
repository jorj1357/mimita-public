# Master regression ledgers

This folder is the human-facing index of confirmed regressions by domain.

Each ledger is append-only. Each entry links to the dated record under
`docs/regressions/YYYY-MM-DD/`, which remains the detailed evidence owner.

## Ledgers

- `collisions-REG.md` — movement, contact, penetration, containment, floors.
- `performance-REG.md` — frame time, fixed-tick budget, stalls, allocations.
- `networking-REG.md` — packets, replication, authority, join/ICE paths.
- `npc-movement-REG.md` — NPC behavior, targeting, lifecycle, role data.
- `dev-loop-REG.md` — dev loop, build, logger, scenario, process tools.
- `gameplay-REG.md` — modes, weapons, melee, avatars, UI, other systems.

## Rules

1. A regression is confirmed by runtime evidence or human review, not by a
   build passing or a source inspection alone.
2. Add one numbered entry to the matching master ledger.
3. Add or update the dated detail file with observed, expected, actual, cause,
   wrong code, corrected code, fix, and proof.
4. Keep entries append-only. Do not rewrite an earlier entry; add a correction.
5. Repeated failures of the same regression stay in the same detail file and
   add a new occurrence section.
6. Status vocabulary is defined in `docs/regressions/README.md`.
7. Ordinary work history belongs in `docs/changelog/`, not here.

## Entry shape

```md
## Entry 11 — Short title

- Recorded: 2026-11-03T19:09:00Z
- Status: UNRESOLVED
- Summary: One sentence naming the wrong behavior and the confirmed cause.
- Detail: `docs/regressions/2026-11-03/short-title-REG.md`
```
