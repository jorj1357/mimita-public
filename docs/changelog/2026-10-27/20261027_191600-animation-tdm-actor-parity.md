# Animation sets, TDM actor parity, and loadout enforcement

- Time: 2026-10-27T19:16:00Z (2026-10-27 15:16:00 EDT)
- Branch: `20261011zt`; no commit made.
- Result: `PASS_WITH_HUMAN_REVIEW` for the implemented configuration and runtime
  wiring. Visual pose quality, hurt/equip presentation, and replay playback still
  need direct human review.

## Starting state and authority

Read `AGENTS.md`, `docs/ROUTER.md`, the routed animation, avatar, weapon, NPC,
gamemode, networking, ECS, performance, build, runtime, time-format, and
completion documents. Pre-existing unrelated edits were present in
`config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `config/command-aliases.json`,
`config/devtimertips.json`, `config/weaponcollisions.json`,
`config/weapons.json`, Zombie Tower/open-gameplay docs, and untracked changelogs
and evidence; these were preserved. The running dev loop was reused.

## Changes

- `config/gamemodes/masterlist/gamemode-masterlist.json`: removed the wildcard
  denies from `competitive_no_headless` that rejected every avatar. The allow
  list is now the effective TDM human list.
- `src/gamemode/match-roles.cpp`, `src/network/server-players.cpp`,
  `src/network/server-avatar-network.cpp`, `src/network/community-match-client.cpp`:
  reject out-of-policy avatars and replace them with a permitted fallback on
  spawn, on manifest rejection, and in local presentation. TDM NPCs now select
  from seven permitted human avatars instead of retaining monster bodies.
- `src/network/server-gamemode.cpp`, `src/network/server.cpp`, `src/gui/gui-main.cpp`,
  `src/gui/menus/online-menu.cpp`, `src/network/community-server-config.cpp`:
  team modes now apply their configured `weapon_set_id` automatically; the
  sandbox GUI selection no longer counts as a TDM override. Role/preset sets
  cannot replace a team mode's set. The GUI default is `Auto (mode set)`.
- `src/network/server-players.cpp`: added `actor.loadout.resolved` evidence for
  each player spawn/respawn.
- `config/animations/settings.json` and
  `config/animations/masterlist/animation-sets.json`: added hot-reloadable state
  priorities, hurt tiers, root translation mode, death presentation, render pose
  interpolation, and the ordered `general_set_1`.
- `src/entities/player-avatar-animations.*`: load an avatar profile's `set`
  before its own includes, hot-reload the set file, and parse the new state keys.
- `src/entities/player.h`, `player-animation-config.cpp`, `player-animation.cpp`:
  added dash, downDash, freeze, knockback, crouch transitions, equip, hurt tiers,
  death-small/respawn states; replaced the hard-coded state chain with scored
  candidates using JSON priorities; landing now yields to movement; damage and
  replicated health drops both select hurt tiers.
- Death presentation: non-ragdoll deaths use the authored small-death clip;
  ragdoll deaths keep the ragdoll path. A mode may set
  `death_presentation: "disappear"` to hide the body.
- Root translation: authored translation is kept by default; mode `2` zeroes
  part translation while preserving rotation/scale. A clip may override it.
- `src/entities/player-render.cpp`, `src/engine/engine-tick-replay.cpp`: added
  presentation-only interpolation between fixed-tick poses. Gameplay and
  hitboxes continue to use the authoritative fixed pose.
- `src/engine/engine-tick-render.cpp`: replay actors with an avatar animation set
  use the same `updateProceduralAnimation` state machine as live actors; legacy
  recordings without a set still use recorded body-part poses.
- `config/animations/library/weapons/*.json`: added equip state references for
  chainsaw, mace, and swordsword.
- `src/perf/perf.cpp`: `PERFORMANCE_FRAME` now records p50/p95/p99 frame time and
  frames over budget.
- `devscripts/animation_json_check.py`: validates ordered sets, set includes,
  avatar `set` references, settings values, and new state references.

## Evidence

- `py devscripts/animation_json_check.py`: `clips=116 avatar_profiles_checked=34
  errors=0`.
- `py devscripts/weapon_json_sync.py --check`: `masterlist=1 definitions=34
  errors=0`.
- Dev-loop build 3494 succeeded. `C:\mimita-v9\.dev\builds\3494\mimita.exe
  --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-27-2026/20261027_152015/events.jsonl`.
  The live dev-loop run was `20261027_191214` with client and server ready on
  build 3494, mode 31 (`tdm npc elite`, map `atdm`).
- In the live journal:
  - `actor.loadout.resolved` showed the local player with set `29` and weapons
    `swordsword`, `shotgun`, `revolver`, `chainsaw`, `mace`, `rocket_launcher`,
    `nothing`, `deathball`.
  - `weapon.equip.slot_resolve` on the client resolved set `29` for revolver,
    shotgun, swordsword, chainsaw, and mace.
  - Every TDM NPC `npc.avatar.spawn-selection` reported `candidate_count: 7`,
    `allowed: true`, and a human avatar.
  - `anim.settings.loaded` reported the expected priorities, hurt mode, root
    translation mode, death presentation, and interpolation setting.
  - `anim.state.changed` observed imported dash, downDash, freeze, landing,
    sprint, jump, fall, and small-death clips on actors and NPCs.
  - A teleport/void death selected `mimita_sizing_6_smalldeath_001`.
  - `PERFORMANCE_FRAME` reported p99 and over-budget data. Spikes were dominated
    by `Combat::HitResolve` and `Simulation::SimulateTick`, not steady-state
    animation sampling.
  - No `config.weapon.invalid` or `config.gamemode.invalid` appeared in the
    inspected interval.

## Review and remaining work

- Human visual review must confirm pose quality, hand/weapon attachment, landing
  behavior, hurt tiers, equip clips, small-death presentation, and replay
  playback.
- Hurt-tier and equip state selection are wired but were not directly exercised
  by live damage or a real equip transition in this run.
- Frame-time spikes remain in combat hit resolution and simulation. The new p99
  evidence gives a starting point; the spikes were not fixed in this pass.
- Five boomer animations remain under review for a different pose basis, and
  six monster collections still lack the six canonical parts.
- `death_presentation: "disappear"` is supported but no mode selects it yet.
- The 44 imported clips exist and validate; no new Blender export was run.

Focused review: `docs/skills/spec-behavior-review-v1.md` —
`PASS_WITH_HUMAN_REVIEW`; `docs/skills/logging-checker-v1.md` — bounded owner
events added; `docs/skills/efficiency-checker-v1.md` — no new per-query
allocations in the collision path, but the combat/simulation spikes remain open;
`docs/skills/documentation-checker-v1.md` — animation and TDM specs updated.
