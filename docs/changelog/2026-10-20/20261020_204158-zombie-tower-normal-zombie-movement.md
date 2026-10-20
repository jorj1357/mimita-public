# Zombie Tower normal zombie movement preset

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Session time: 2026-10-20 16:41:58 EDT (`2026-10-20T20:41:58Z`).
- Branch: `20261011zt`.
- HEAD: `7556a218ec6b6bb8443485ff47b51c9ebf844e0d`.
- Pre-existing workspace edits: numerous unrelated Zombie Tower actor, weapon, animation, movement archive, map-pool, audio, lighting, account, analytics, spec, and changelog changes were present. `config/roles.json` already contained user edits for Boomer, Stabber, Crawler, Brute, and Charger roles; those were preserved. `config/movement/movement-source-monster.json` and other listed movement/archive files were already untracked user work; the monster movement tuning was preserved.

## Specification and owner

The final TODO in `docs/specs/gamemodes/zombie-tower/zombie-tower.md` says: `make it so  C:\mimita-v9\config\movement\movement-source-monster.json this is the movemnt mode that normal monsters use`. The Actor Presets spec says a role may override the actor preset movement, and defines separate player and NPC roles pointing to the same monster preset. Therefore both Normal Zombie roles and the shared actor preset now select the dedicated movement profile.

The movement loader resolves presets by the JSON `name` before falling back to a filename. Before this change, both `movement-source.json` and the user-provided `movement-source-monster.json` declared `name: cs16`, making the selector ambiguous. The monster file now declares the unique name `source-monster` so the requested selection resolves to that file. Its movement values were not changed.

## Changes

- `config/actor-presets/zombie_tower_normal_zombie.json`, `movement_preset` (line 5): before `"movement_preset": "source",`; after `"movement_preset": "source-monster",`.
- `config/roles.json`, role `zombie_tower_zombie` (line 182): before `"movement_preset": "source",`; after `"movement_preset": "source-monster",`.
- `config/roles.json`, role `zombie_tower_normal_zombie_npc` (line 301): before `"movement_preset": "source",`; after `"movement_preset": "source-monster",`.
- `config/movement/movement-source-monster.json`, preset name (line 23): before `"name": "cs16",`; after `"name": "source-monster",`.

## Review and validation

- Read `docs/ROUTER.md`, the Zombie Tower, movement, actor preset, and NPC behavior specifications, the NPC movement architecture, runtime scenario workflow, debug logging spec, behavior review skill, regressions, and time/filename rules.
- `docs/skills/spec-behavior-review-v1.md`: PASS. The requested config selection was clear; no spec decision was needed. Live behavior acceptance remains open.
- Static config validation: PASS. Actor preset and roles JSON parsed; monster movement JSONC parsed; `source-monster` resolved uniquely to `movement-source-monster.json`; both normal zombie roles and the actor preset selected that name.
- Build: NOT RUN. No C++ source changed; this is runtime configuration selection and the new movement file is already present in the workspace.
- Runtime: NOT VERIFIED. The existing dev-loop session was left running and untouched. It was already active on build 2680, launched before these edits, with `--no-npcs`; its journal cannot prove this new role selection.
- Human movement/feel acceptance: still needed in a newly loaded Zombie Tower scenario. The bottom-of-spec NPC collision TODO remains outside this request.
- No confirmed regression was found; no regression entry was added.
