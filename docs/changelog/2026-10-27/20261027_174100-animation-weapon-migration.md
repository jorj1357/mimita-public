# Animation and weapon configuration migration

- Time: 2026-10-27T17:41:00Z (2026-10-27 13:41:00 EDT)
- Branch: `20261011zt`; no commit made.
- Result: `PASS_WITH_HUMAN_REVIEW` for the imported six-part animation and configuration slice. The generic tool/entity conversion, nonstandard rig retargeting, and combinatorial procedural animation system remain unfinished.

## Starting state and authority

Read `AGENTS.md`, `docs/ROUTER.md`, the routed animation, avatar, weapon, NPC, networking, ECS, JSON configuration, asset, runtime, build, time-format, and completion documents. The previous avatar-specific `animations.json` and aggregate `config/weapons.json` were the active paths. Pre-existing unrelated edits were present in `docs/architecture/code-ownership/code-ownership.md`, `docs/architecture/vision-mentality/vision.md`, `docs/specs/animations/animations.md`, `docs/specs/open-gameplay-issues.md`, and untracked spark art, changelogs, and gold notes; these were preserved. The running dev loop was reused.

The animation spec's prior exclusive-pose wording said to suppress AimBody, while the user's current request explicitly requires AimBody over Blender poses. The spec was aligned to that requested behavior, with a per-clip `aimLayer: false` escape hatch. `docs/specs/weapons/weapons.md` described a per-weapon path ending in `<weapon-id>.json`; the requested and implemented path is `config/weapons/<id>/config.json`.

## Changes

- `tools/mimita_animation_batch.py` inventories the named collections in `C:\important\quiet\blender-projects\mimita sizing 6.blend`, records collection paths, rig roots, part objects, Actions, frame ranges, output paths and status in `config/animations/imports/mimita_sizing_6.json`, preserves reviewed decisions on repeat inventory, and exports approved clips through the calibrated repository exporter. Existing Blender source was not saved. Of 105 source rigs, 44 approved clips were exported, five boomer rigs remain under review for a different root basis, 50 were skipped, and six lack a supported six-part rig.
- `devscripts/animation_json_check.py` checks clip data, action markers, avatar references, and ready manifest outputs. `config/animations/masterlist/animation-masterlist.json` records the current clip and profile field contract.
- Eight legacy avatar `animations.json` profiles moved into `avatar.json.animation_profile`, with shared clips extracted under `config/animations/library/avatar/`; 269 duplicate clip files were deduplicated into 45 shared files. Twenty-six standard or six-part avatars received imported movement states. `src/entities/player-avatar-animations.cpp` reads inline profiles while retaining a legacy fallback.
- Mace, sword, and chainsaw animation profiles now reference imported held clips; mace and sword actions use imported attack clip IDs. `src/entities/player-animation.cpp` selects a held-tool default idle pose, avoids the legacy arm pose when a tool clip owns the pose, and adds AimBody by default. `src/entities/player-animation-config.cpp` parses `aimLayer`; AimBody pitch is smoothed once per tick, and opt-out works on layered and authored-only profiles.
- `devscripts/weapon_json_sync.py` split 34 definitions into `config/weapons/<id>/config.json` and generated `config/weapons/masterlist/weapon-masterlist.json`. Every definition carries the common top-level field list; unused fields are `null`. `config/weapons.json` now holds only compatibility metadata. `src/combat/weapon-json-config.cpp` overlays per-weapon definitions, validates their identity, fields, and types, watches their signatures, and rejects invalid snapshots with `config.weapon.invalid`. The grenade and viewmodel readers use the same merged root. `devscripts/config_selftest.py` reads the new canonical files.
- Updated the animation, Blender authoring, weapon, and JSON configuration documents with current paths, AimBody behavior, batch commands, and the boundary of the current architecture.

## Evidence

- Blender 5.0 reread the named `.blend` and reproduced 44 ready, five review, 50 skip, and six unsupported entries. Re-inventory preserved the four selected Melee clip decisions and all five review holds.
- `py devscripts/animation_json_check.py`: `clips=116 avatar_profiles_checked=34 errors=0`.
- `py devscripts/weapon_json_sync.py --check`: `masterlist=1 definitions=34 errors=0`.
- `git diff --check` on changed code and focused documentation passed; the full diff still reports trailing whitespace in pre-existing informal lines at the top of `docs/specs/animations/animations.md`.
- `py devscripts/config_selftest.py` read 34 weapons but failed on thousands of avatar normalization-field complaints. This check cannot be counted as a pass; it is not evidence of a new animation-loader failure, and no baseline comparison was run.
- Canonical dev-loop build 3482 succeeded. `C:\mimita-v9\.dev\builds\3482\mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-27-2026/20261027_133957/events.jsonl`. Dev-loop profile `npc-navigation`, launch mode 6, was running build 3482 with server and client ready, journal `logs/2026-10-27/20261027_173321_01/events.jsonl`.
- In that run after the final launch: eight `weapon.melee.moveset_loaded`, four `anim.set.loaded` (Captain Falcon and Jason on both peers), 69 `anim.state.changed`, and three `anim.melee.authored_pose` events. Imported idle, walk, sprint, jump, fall, and landing clips were selected. No `config.weapon.invalid` or `anim.set.reload_rejected` appeared in the inspected interval. This proves configuration and state selection, not rendered pose quality or combat timing.

## Review and remaining work

- Human visual review must check imported poses, aim up/down on idle and attacks, attachments/hitboxes, player and NPC movement, and the sword/mace contact windows. Nonstandard monster/boomer rigs need calibrated export or retargeting before mapping.
- The current weapon runtime still has specialized combat, inventory, networking, and physical-object owners. A skateboard or other non-weapon entity cannot yet be defined and used from the weapon masterlist alone. The present masterlist enforces top-level completeness; nested component schemas and a general tool/entity capability model are future work.
- The procedural system currently combines authored clips with AimBody and existing selected overlays. It does not yet generate a large combinatorial move set from a few source animations.
- Relevant unresolved documentation notes were left in place: `docs/specs/weapons/weapons.md:74` says `todo explain` near weapon damage tuning; replace it with the exact desired damage and falloff controls. `docs/specs/weapons/tool-weapon-ideas.md:1` says `todo explain like`; replace it with an explicit purpose and distinction between proposals and active tool contracts.

Focused review: `docs/skills/spec-behavior-review-v1.md` — `PASS_WITH_HUMAN_REVIEW`, with visual and generic-tool acceptance open; `docs/skills/asset-checker-v1.md` — generated clip paths and referenced files checked, visual approval open; `docs/skills/logging-checker-v1.md` — bounded owner events and rejected-config diagnostics present, with rendered output not inferable from logs; `docs/skills/documentation-checker-v1.md` — current paths updated, relevant TODOs reported above; `docs/skills/efficiency-checker-v1.md` — AimBody smoothing moved out of the per-part loop, weapon file signature polled at the existing 250 ms interval.
