# Counter-Strike Checkpoint 7 — bomb sites, plant, defuse, explosion

Date: 2026-10-02
EST timestamp: 2026-10-02 23:05:12 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure plant/defuse/site-rule evidence are proven. Site
positions are unauthored and planting is disabled until a human verifies them
with `site_debug`; live plant/defuse/explosion is NOT visually verified.

## Scope

Checkpoint 7 of `docs/specs/20261002plan.md`: editable bomb sites and the
plant/defuse/explosion round rules (Stages 11, 12).

## Pre-existing / external edits (not mine)

Same runtime-written user settings as prior checkpoints. Untouched.

## Files changed

### `src/gamemode/map-config.h` / `.cpp` (new)

- `BombSite` (id, position, radius, visible_debug, hasPosition).
- `MapObjectiveConfig` (mapId, bombSites, plant/defuse/explosion seconds).
- `MapConfigRegistry`: `load(mapId)`, `pollReload`, `current`, `findSite`,
  `siteIndexAt(position)`, `setSitePosition`, `setSiteVisibility`, `save`,
  `pathForMap`. Accepts nested `objectives.bomb_sites` or flat `bomb_sites`.
- `mapConfigSelfTest`.

### `config/maps/dust2cyberiav3.json` (new)

Sites A/B with radius 4 and `visible_debug` false, no `position` (unauthored),
and bomb timers plant 3 / defuse 5 / explosion 40. Comment explains the
verify-and-save workflow.

### `src/game/objective-state.h` / `.cpp`

`ObjectiveInstance` gained plant/defuse progress fields, planner/defuser ids,
planted site id, and `isPlanted()`. Added `advanceObjectiveProgress`
(fixed-tick, interruptible) and `objectiveSecondsToTicks`. Extended
`objectiveSelfTest` with progress tests.

### `src/network/server-gamemode.cpp`

- `assignObjectiveCarrier` loads map timers/progress and resets per round.
- `serverObjectiveTick`: plant inside a site (interrupt on leaving), explode at
  the deadline, defuse by a defender in interaction range (interrupt on
  leaving); emits `objective.plant-start`/`planted`/`defuse-start`/`defused`/
  `exploded`.
- `checkObjectiveRoundEnd`: planted bomb survives a team wipe; timeout awards
  defenders when nothing was planted.
- `broadcastDuelState` replicates progress, progress kind, explosion timer, and
  site id.

### `src/network/packets.h`

`DuelStatePacket` gained `objectiveProgress`, `objectiveTimerLeft`,
`objectiveProgressKind`, and `objectiveSite[8]`.

### `src/network/community-match-client.h` / `.cpp`

`ReplicatedObjective` gained `site`, `progress`, `timerLeft`, `progressKind`;
mirrored in `onState`.

### `src/engine/engine-tick-ui-overlays.cpp`

HUD now shows "Planting Bomb...", "Defusing Bomb...", "BOMB PLANTED Ns", and
"Pick up Bomb"/"BOMB DROPPED".

### `src/game/gamemode-manager.cpp`

Site debug zones rendered as wire spheres when a site's `visible_debug` is set.

### `src/terminal/debug-commands.cpp`

Added `site_debug show|hide|select <id>|move [x y z]|print|save`.

### `src/game/game-cli.cpp`

Added `--map-config-selftest`.

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 7.

## Reasoning

Sites are generic per-map data queried by id or world position so future
objective kinds can reuse them. Plant/defuse use one pure fixed-tick,
interruptible progress helper shared by runtime and test. Round outcomes now
follow the plan: explosion awards the planters, defuse awards the defenders, and
a timeout without a plant awards the defenders. Positions are intentionally
left unauthored because they must be verified against the loaded map, not
guessed.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 11, 12; Checkpoint 7).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.
- Skill: `docs/skills/terminal-command-checker-v1.md` — `site_debug` registered
  with usage/description/category and clear failure messages.

## Validation

Build: `BUILD SUCCESS`.

Runtime (`mimita.exe`):

```text
[MAP CONFIG SELFTEST] loaded=yes sites=2 ... PASS
[OBJECTIVE SELFTEST] progress=ok PASS
[NPC UTILITY SELFTEST] PASS
[NPC NAV REQUEST SELFTEST] PASS
[NPC PERCEPTION SELFTEST] PASS
[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

## Human review still needed

- Inside a live CS session: `site_debug show`, stand at each bombsite, run
  `site_debug move A`/`B`, `site_debug print`, `site_debug save`, then confirm
  the bomb can only be planted at a site, planting interrupts on leaving,
  defusing interrupts out of range, explosion awards Terrorists, defuse awards
  Counter-Terrorists, and a no-plant timeout awards Counter-Terrorists.
- Confirm site debug spheres appear with `site_debug show`.

## Explicitly not done yet

The explicit `F` interact wire path (plant/defuse are proximity/state driven),
grenades/area effects (Checkpoint 8), and TeamBrain objective play
(Checkpoint 9).
