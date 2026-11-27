# Config limit retirement

- **Status:** proposed
- **Recorded:** 2026-11-26
- **Owner:** configuration / presentation
- **Scope:** numeric `min` / `max` bounds in authored config schemas

## Purpose

Authored config is data, not validation. A designer must be able to write a
value outside the current comfort range and see what happens at runtime. The
long-term target is that no authored numeric setting has a `min` or `max` gate.

The engine still needs to survive bad data. That is a consumer problem: reject
NaN/infinite values, keep shader branches finite, and diagnose the applied
value. Re-adding an artistic limit is not the fix.

## What counts as a limit

In scope:

- numeric `min` and `max` keys in `config/**/masterlist/*.json` and other
  schema files;
- loader or setter clamps that mirror those ranges (`std::clamp` on config
  values).

Out of scope (these are not artistic limits):

- `type`, `required`, `nullable`, and string `enum` selectors;
- `isfinite` / NaN guards and shader absurd-value rejection;
- division-by-zero and buffer-size guards;
- fixed simulation invariants (for example 60 Hz gameplay).

## Baseline 2026-11-26

`config/gamemodes/masterlist/gamemode-masterlist.json` is the only masterlist
with numeric ranges today:

- 118 fields carry a range (109 after the Phase 1 lens removal);
- 118 `min` keys and 42 `max` keys in the file (some fields carry a max only);
- the raw occurrence count is not a field count; use the script below.

Largest groups (pre-Phase 1):

| Group | Fields with min/max |
|---|---:|
| `settings.presentation` | 42 (33 after Phase 1) |
| `settings.world_streaming` | 12 |
| `settings.objectives.item` | 8 |
| `settings.payload` | 8 |
| `settings.rounds` | 7 |
| `settings.teams.item` | 5 |
| `settings.aftermath` | 4 |
| `settings.npc_travel` | 3 |
| `settings.npc_spawn` | 3 |
| `settings.camera` | 2 |
| smaller top-level settings | 24 |

The spec appendix is regenerated with:

```
python devscripts/config-limit-inventory.py
```

(script added when the migration starts; it counts `min` / `max` occurrences
per section and writes a markdown table).

## Migration phases

Each phase removes the range keys, removes the matching consumer clamp, and
proves an out-of-range value applies at runtime. A phase is not "done" until the
journal shows the extreme value and the feature still runs.

| Phase | Scope | State |
|---|---|---|
| 0 | this policy + baseline inventory | this document |
| 1 | `settings.presentation.bodycam.lens` | **in progress 2026-11-26** |
| 2 | rest of `settings.presentation` | pending |
| 3 | `world_streaming`, `camera`, `npc_spawn`, `npc_travel` | pending |
| 4 | objectives, payload, rounds, teams, aftermath | pending |
| 5 | remaining smaller groups | pending |

## Acceptance for a migrated value

1. The schema has no `min` / `max` for that field.
2. No consumer clamps the value back into the old range.
3. A value outside the 2026-11-26 band loads without a config error.
4. The applied value is visible in the journal (`config.value.extreme`,
   `gamemode.bodycam.applied`, or the feature's own settings event).
5. The feature may look broken; that is allowed. A crash, NaN, or black screen
   is fixed at the consumer, never by restoring the range.

## Diagnostics

- `config.limits.inventory` — baseline counts and file path.
- `config.limits.policy` — fields that currently have no range.
- `config.value.extreme` — a value outside the 2026-11-26 band was applied.
- Feature events (`presentation.lens.policy`, etc.) always report the effective
  value, not the requested one.

These are advisory records, not rejections.

## Why not remove everything at once

An out-of-range value can produce a black screen, a full-screen vignette, a
zero-scale HUD, or a physics blow-up. Batching keeps the change traceable and
lets each group get a runtime proof. The end state is still "no numeric
min/max"; only the order is gradual.
