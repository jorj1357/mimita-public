# Fun Counter Strike HUD visibility and Deathball damage

Time created: 2026-11-01T01:52:53Z

Status: IMPLEMENTED IN SOURCE; BUILD 13 SUCCEEDED; RUNTIME JOURNAL CONFIRMS
HUD MODE ID AND DEATHBALL DAMAGE; HUMAN VISUAL REVIEW PENDING

Branch: `20261011zt`
Base commit: `f757b936 disable destructible map world by default and document re-enable path`
Pre-existing edits: the worktree already contained many modified and untracked
files from earlier sessions. This session only changed the files listed below.

## Goal

Fix the two user-visible failures:

1. Fun Counter Strike showed no objective/round HUD in game.
2. Deathball passed through actors but applied no damage or hit feedback.

Also log the deferred weapon-slot issue: loadouts with more than ten logical
weapons cannot be reached easily with the direct number keys.

## Root causes

### HUD: replicated gamemode id was truncated

`DuelStatePacket::matchMode` was `char[16]` (`src/network/packets.h:1183`).
`"funcounterstrike"` is 16 characters plus the null terminator, so
`strncpy(..., sizeof - 1)` in `src/network/server-gamemode.cpp:1603` produced
`"funcounterstrik"`.

Evidence: `logs/2026-11-01/20261101_010944/events.jsonl`,
`network.mode.start` from the client contained `"mode":"funcounterstrik"`.

The client then called
`GuiLayoutManager::getGamemodeLayout("funcounterstrik")`, found no section in
`config/gui/gamemode-meta-gui.json`, and rendered no HUD elements.

### Deathball: world pass-through consumed the actor contact

`simulateProjectileTick` chose the earliest collision candidate; at equal time
world geometry won over actor capsules (`src/combat/projectile-simulation.cpp`,
candidate ordering). The Deathball radius is 5 m, so its sphere continuously
overlaps the floor. On nearly every tick the world candidate won at `t=0`, the
`WorldPassThrough` branch ran, and the actor capsule candidate was never
processed.

Evidence: run `20261101_010944` recorded `deathball.spawn`,
`deathball.wall-pass`, and zero `deathball.contact`.

### Deathball: server knockback was always upward

`src/network/server-projectiles.cpp` computed:

```cpp
glm::vec3 push = targetCenter - victimPos; // always (0,0,0.9)
```

This normalized to straight up instead of outward from the ball.

## Changed files and exact changes

### `src/network/packets.h`

- `PROTOCOL_VERSION` 46 -> 47 with a protocol-history comment.
- Old: `char matchMode[16] = {};`
- New: `char matchMode[32] = {};` with a comment explaining that a 16-byte field
  silently truncated `funcounterstrike` into a different mode id.

### `src/network/server-gamemode.cpp`

- In `broadcastDuelState`, added a rate-limited `network.mode.truncated` error
  event if a future gamemode id ever exceeds `DuelStatePacket::matchMode`.
- The id is still copied with `strncpy(..., sizeof - 1)`, but the field now fits
  every shipped and configured mode id.

### `src/gui/gui-layout.cpp`

- `getGamemodeLayout` now returns an empty layout for an empty mode request
  without logging it.
- Added a one-per-load `hud.layout.loaded` GUI event with the mode, section,
  load result, element count, and element ids. This makes an empty or missing
  HUD section observable in the journal.
- Evidence from the fixed build:
  `mode:"funcounterstrike"`, `section:"funcounterstrike"`, `loaded:true`,
  `element_count:12`, including `phaseText`, `scoreText`, `roundText`,
  `matchTime`, `objectiveStatus`, `objectiveHint`, `objectivePrompt`,
  `objectiveResultText`, `roundOverText`, and `countdownText`.

### `src/combat/projectile-simulation.h`

- Added `bool passedThroughWorld = false` to `ProjectileStepResult`.
- It is independent of `type`, so a pass-through projectile can contact an actor
  and world geometry on the same step and both owners must run.

### `src/combat/projectile-simulation.cpp`

- Tracks `worldContactThisStep` and the earliest actor capsule candidate.
- When `passThroughWorld` and `passThroughActors` are both true and the world
  candidate would otherwise hide an actor, the actor candidate becomes the
  primary result. `passedThroughWorld` is still set so wall lifetime decay is
  applied.
- World-only pass-through still sets `result.passedThroughWorld = true`.

### `src/network/server-projectiles.cpp`

- Wall lifetime decay now runs on `step.passedThroughWorld` instead of only
  `step.type == WorldPassThrough`. This allows a wall-decay tick and an actor
  contact tick to coexist.
- Deathball knockback direction corrected to outward:
  - Old: `push = targetCenter - victimPos;`
  - New: `push = victimPos - center;`
- Damage distance uses the victim origin, matching the local Deathball path.
- The authoritative contact point comes from
  `nearestServerActorBodyPoint(...)`, so hit markers, impact effects, and damage
  numbers appear on the victim instead of the ball center.
- Contact reach/tolerance now match the actual body contact point.
- Damage source changed from `RocketExplosion` to `PhysicalContact`; Deathball
  is not an explosion.
- `deathball.contact` now records `contact_point` and
  `passed_through_world`.

### `src/network/multiplayer-projectiles.cpp`

- Remote projectile age decay now uses `step.passedThroughWorld`, matching the
  server and preserving wall decay when the result is an actor pass-through.

### Documentation

- `docs/specs/weapons/deathball.md`: world pass-through must not mask actor
  contact; the contact point is on the victim; feedback uses the shared
  confirmed-damage/NPC-damage paths.
- `docs/specs/networking/contact-authority.md`: the Deathball contract now
  states that world pass-through and actor contact are independent and that
  knockback points outward.
- `docs/specs/open-gameplay-issues.md`: added a newest-first intake entry for
  the deferred slot issue. Weapon set 30 binds `darkbang` at logical slot 11 and
  `bomb` at logical slot 12, beyond the direct 1-0 key range. No binding or
  inventory change was made.

### `config/playervisuals.json` (follow-up request)

The player asked for enemy and friendly outlines to be enabled and thicker.
The shared player-visuals config is the owner of outline appearance; gamemode
`player_outlines` / `friendly_outlines` remain the per-mode enable policy.

- Enemy outline:
  - Old: `"enabled": false, "thickness": 1.0, "alpha": 1.0, "color": [255, 255, 255]`
  - New: `"enabled": true, "thickness": 4.0, "alpha": 1.0, "color": [255, 0, 0]`
- Teammate outline:
  - Old: `"enabled": false, "thickness": 1.0, "alpha": 0.5, "color": [0, 255, 0]`
  - New: `"enabled": true, "thickness": 4.0, "alpha": 1.0, "color": [0, 255, 0]`

The renderer converts thickness to a world-space shell with
`thickness * 0.015`, clamped to `0.01..0.15`. Thickness `4.0` is a 4x increase
from the old `1.0` and produces a `0.06` world-space outline.

Validation: `config/playervisuals.json` parses as valid JSON. The file is
hot-reloadable and needs no build. Modes with `player_outlines: false` (for
example `counterstrike` and `juggernaut`) still suppress the configured team
outlines by design.

### Follow-up: every avatar must receive an outline

The player reported that the girl body and the normal zombie body had no
outline even though other actors did. Three rendering gaps were found and
fixed:

- `src/npc/npc.cpp` / `src/npc/npc.h`: local NPCs were rendered through
  `renderPlayer(...)`, which treats the actor as the local player and therefore
  uses the disabled `self` outline. `NpcSystem::render` now accepts the local
  team and calls `renderNetworkPlayer(npc.body, camera, npc.id, false,
  localTeam, npc.body.matchTeam)`.
- `src/engine/engine-tick-render.cpp`: computes `localOutlineTeam` once and
  passes it to `npcSystem.render(...)` so local NPCs get the same
  teammate/enemy policy as remote actors.
- `src/entities/player-render.cpp`: the single-mesh `renderMesh` fallback and
  the capsule fallback previously ignored `outlinePass` entirely. Both now set
  `uOutlinePass`, `uOutlineThickness`, and the outline color, and cull front
  faces for the outline pass. `computeActorVisualBounds` now derives bounds from
  the fallback `renderMesh` batches when an avatar has no physical body parts;
  otherwise the outline guard rejected those actors every frame.
- `src/render/render-player.cpp`: an actor with unknown team identity now uses
  the configured enemy outline layer instead of losing the outline entirely,
  while `player_outlines` and `friendly_outlines` still gate the layer.

Runtime evidence, run `20261101_031926`, build 34:

- `player_visuals.outline` recorded `drawn:true`, `thickness:4.0`,
  `world_thickness:0.06`, `reason:"drawn:4.000000"` for both teammates
  (`alpha:1.0`, green) and enemies (`alpha:1.0`, red).
- The earlier stale run had logged `thickness:1.0`, confirming the new config is
  active.

The girl/monster fallback path and local NPC path are source-complete and the
build passes, but human visual confirmation on those exact avatars is still
required.

## Validation

### Build

- Dev-loop build `13` succeeded, then build `15` succeeded with the same
  session changes. Build `15` is the last successful build before unrelated
  concurrent edits appeared in `src/entities/player.h`, `src/entities/player.cpp`,
  `src/network/server-npcs.cpp`, and `src/sim/simulate-tick.cpp`.
- Exact executable: `.dev/builds/0015/mimita.exe`.
- `--versioninfo`:
  `EVENTS_JSONL_PATH=logs/10-31-2026/20261031_215912/events.jsonl`.
- The build timestamp reported by `--versioninfo` is `21:46:08`; the published
  executable file timestamp is later than all source edits in this session.
- After build `15`, the dev loop began rebuild attempts for the unrelated
  concurrent source edits above. Those attempts reported a `class Player`
  redefinition and are not part of this session's changes.

### Focused self-tests (build 15)

- `--canonical-contact-selftest`: PASS.
- `--objective-selftest`: PASS.
- `--map-config-selftest`: PASS (2 sites checked).
- `--counterstrike-acceptance-selftest`: round+weapons, objective, map-config,
  grenade, area-effect, team-brain, npc-perception, npc-utility, npc-grenade,
  and npc-nav-request PASS. The pre-existing `spawn-tags` failure remains; it is
  unrelated to this change.

### HUD runtime evidence (build 13)

Run `20261101_015151`, mode `funcounterstrike`, map `dust2cyberiav4`:

- client `network.mode.start` reported `mode:"funcounterstrike"`;
- client `hud.layout.loaded` reported the `funcounterstrike` section with 12
  elements and `loaded:true`;
- round start/countdown/go/active events were present;
- the server reported `round_end_tick:6480` for a 90-second active round.

This proves the layout lookup and round state are no longer blocked by id
truncation. It is not visual acceptance of readability or overlap.

### Deathball runtime evidence (build 8 behavior)

Run `20261101_014026`, sandbox mode, one player against one server NPC:

- `deathball.spawn` for projectile 13.
- Repeated `deathball.contact` events with `passed_through_world:true`.
- Example contact: damage 8, `distance_fraction:0.175`, `distance_m:0.876`,
  health 76 -> 68, `contact_point` on the NPC, outward knockback
  `[0.7665, 0.2004, -0.1108]`.
- `server.damage.applied` recorded the expected per-tick sequence
  `2,3,5,6,7,8,9,8,7,5,3,2,1` and dropped the NPC from 99 to 33 health.
- The result is player -> NPC. NPC-origin and player -> player runs were not
  completed in this session. The server branch resolves the attacker as NPC when
  `projectile.ownerNpcId != 0`, but the actor/weapon matrix in
  `docs/workflows/runtime-scenario-validation.md` is still required.

An initial `deathball_contact_probe_v1` scenario used `npc_spawn_legacy`, which
is intentionally dropped while multiplayer is active, so it did not spawn a
server NPC and its assertion failed. The scenario file was removed. The successful
evidence came from the live run using `npc_spawn 1`, `agent.aim`, and
`agent.shoot`.

Build 8 contains the gameplay changes. Build 13 contains the same gameplay
changes plus the final journal guard and HUD log cleanup; build 13 compiled and
passed the focused self-tests.

## Documents and skills read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/weapons/deathball.md`
- `docs/specs/gamemodes/funcounterstrike.md`
- `docs/specs/gamemodes/objective-hud.md`
- `docs/specs/gui/gui.md` (routing context)
- `docs/specs/networking/contact-authority.md`
- `docs/specs/networking/networking.md` (routing context)
- `docs/specs/open-gameplay-issues.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`

## Skill results

`docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.

Findings fixed:

- Spec-code disagreement: Deathball must damage intersecting actors, but world
  pass-through masked actor contact.
- Spec-code disagreement: Deathball knockback must push outward, but the server
  pushed upward.
- Spec-code disagreement: Fun Counter Strike requires an objective/round HUD,
  but the truncated mode id selected an empty layout.

`docs/skills/logging-checker-v1.md`: PASS.

- `hud.layout.loaded` is logged once per mode layout load and includes the
  element list.
- `network.mode.truncated` is rate-limited to one event per oversized mode id.
- Existing `deathball.contact` remains rate-limited to every 10 ticks or a kill.
- No secrets or client-only values are logged.

## Human review still required

- Visually confirm the Fun Counter Strike HUD on the live client: phase, round,
  90-second timer, bomb carried/dropped/planted state, team hint, plant/defuse
  prompt, result text, and readability at the current resolution.
- Confirm hit markers and damage numbers are visible when Deathball hits an NPC
  and when an NPC hits the local player.
- Complete the actor/weapon matrix for Deathball: player -> NPC (proven),
  NPC -> player, NPC -> NPC, player -> player.
- Confirm outward knockback feel and the measured 10 m visual diameter.

## Documentation notes (not edited)

- `docs/specs/gamemodes/objective-hud.md:1` still contains the TODO to combine
  this file with the GUI spec. Suggested fix: merge the general objective HUD
  contract into `docs/specs/gui/gui.md` or remove the draft header.
- `docs/ROUTER.md:105` points to `docs/regressions/regressions-v1.md`, but the
  current file is `docs/regressions/archive-ignore/regressions-v1.md`. The route
  is stale and was not changed in this session.
