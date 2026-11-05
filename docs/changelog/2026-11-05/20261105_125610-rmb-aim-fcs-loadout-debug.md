# RMB Aim and Fun Counter Strike Loadout Debugging

- Timestamp: 2026-11-05 12:56:10 EDT (2026-11-05T17:56:10Z)
- Branch: `20261011zt`
- Scope: remove the mandatory agent-gate requirement, document the user's
  five-step live proving loop, investigate reported weapon aiming, and fix the
  Fun Counter Strike launch loadout override with journal evidence.
- Pre-existing work: preserved unrelated dirty analytics, Blender AI,
  accessibility, vision, animation, entity-editor, gamemode, networking,
  visuals, weapon-direction, and gameplay-intake edits.

## Changes

- `AGENTS.md`: made the agent gate optional and named the live game plus its
  own `events.jsonl` as the required gameplay proof path.
- `docs/specs/dev-loop/dev-loop.md`: replaced the old mandatory workflow with
  the requested five-step build, launch, exercise, inspect, and iterate loop.
- `devscripts/quality/agent-gate.py`: labels the script as an optional manual
  quality gate; no gameplay path calls it.
- `devscripts/dev-loop.py` and `devscripts/dev-launch-modes.json`: added an
  Auto weapon-set option for launch mode 45, so the `npc-navigation` profile's
  explicit set 12 is not passed when starting Fun Counter Strike.
- `src/network/server-players.cpp`: added `actor.loadout.spawned`, recording
  gamemode, role, set ID/key/name, full inventory, spawn generation, and
  equipped weapon/slot at authoritative player spawn.
- Added a separate Fun Counter Strike launch-loadout regression record and
  updated the master gameplay regression ledger. Updated the existing RMB aim
  tracker with current build and journal evidence; held-RMB confirmation stays
  open.

## Findings

The prior live run `20261105_174726` proved the defect: its server journal
recorded Fun Counter Strike mode with set 12 from `host_override`; the player
inventory was `nothing` and `captain_falcon_moveset`; and the client received
set 12. The dev-loop command builder appended Fun Counter Strike mode flags
after the profile's explicit set argument.

The corrected live run `20261105_175404` used build ID
`2026-11-05T17:52:25.918603Z:de51dc23ec632b6f78efba1575fc50feed5be90a0e427e525d03262bede40d9d`.
Its server journal records mode `funcounterstrike`, set 30 from
`gamemode_default`, the `funcounterstrike` key and name, and 13 weapons. After
role assignment, it records `fun_counter_terrorist` and equipped `revolver`.
The client journal receives set 30. The game window showed the Fun Counter
Strike HUD and Revolver.

The source contains the weapon-frame aim correction and expanded
`AIMBODY_LIVE_SAMPLE` transform fields. The corrected run emitted 139 aim
samples, but none had `right_mouse_held=true`; the available Windows UI action
produced only a brief right-click. The aim direction therefore remains
unverified at runtime and visually. Do not mark the aim regression solved
without a sustained held-RMB run and human view confirmation.

## Evidence

- Build: dev-loop compile succeeded at `2026-11-05T17:52:25.918603Z`; one C++
  source compiled and the current executable was published.
- Live server journal:
  `logs/2026-11-05/20261105_175404/server-01/events.jsonl`.
- Live client journal:
  `logs/2026-11-05/20261105_175404/client-01/events.jsonl`.
- Agent gate: not run after the rule was made optional. A previous required
  gate attempt in this task hit its existing damage-authority baseline
  mismatch and collision-stress finding before an access-denied launch issue;
  it is not used as the gameplay proving loop.
- Human acceptance: the mode and Revolver were visible in the live window;
  visual aim direction for the four weapons remains pending.
- Tests: no synthetic test suite was run; user-requested live launch and
  journal inspection were performed.
