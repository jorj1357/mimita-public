// 09 06 2026, 00 00
/* purpose
* define the JSON configuration architecture for MiMITA
* identify commonly used configuration families and their owners
* make the vast majority of gameplay and presentation behavior editable
* this file DOES NOT claim that every listed file is hot-reloaded today
* this file DOES NOT replace feature specifications
* this file DOES NOT permit unsafe live changes to a running network match
*/

# JSON Configuration Architecture

MiMITA should be configuration-led. The vast majority of values that control
gameplay, presentation, accessibility, testing, and networking should live in
named JSON files rather than being compiled into C++ code.

The goal is to change behavior by editing configuration and reloading it, not
by rebuilding the executable for every tuning change. A value is a good JSON
candidate when it is a policy, constant, balance value, layout value, visual
choice, timing value, or test setting rather than engine plumbing.

This is a target architecture. The repository currently contains many JSON
files, but not every file is proven to be hot-reloaded. Each file must be
verified against its loader and reload path before being described as live.

## Reload contract

Every configuration file should have:

1. one obvious owner and one schema;
2. a safe default when the file is missing or invalid;
3. validation with a warning that names the file and rejected field;
4. atomic replacement of the last valid configuration;
5. a reload signal or timestamp check that does not require a process restart;
6. a documented application boundary: immediately, on respawn, at round start,
   or on the next connection;
7. a terminal command that can reload, print, validate, or inspect it.

Presentation JSON intended for live tuning must be polled while the owning
runtime is active and must not depend on a rebuild, restart, avatar swap, or a
global gameplay-config reload toggle. Validate a whole candidate first, then
install it together at its documented safe boundary. If a save is incomplete
or invalid, keep the last valid values and retry when the file changes again.
The feature specification sets the required polling delay and exact boundary.

Invalid JSON must not partially replace a working configuration. Match-critical
values should be staged and applied at a safe boundary instead of changing
halfway through a tick.

## Gamemode masterlist and canonical mode files (2026-10-25)

`config/gamemodes/masterlist/gamemode-masterlist.json` is the single contract
for every file in `config/gamemodes/*.json`. It defines the supported keys, the
canonical field order, types, ranges, enums, nullable fields, and defaults.

### Completeness and ordering

Every gamemode file is a complete canonical document:

- It contains every top-level and nested field listed under `settings` in the
  masterlist, in the masterlist's field order.
- Optional feature blocks are still present. They use an explicit
  `enabled: false`, so the file is structurally complete without changing
  runtime behavior.
- `null` is the explicit “inherit the shared value” marker for presentation
  and trail fields. `null` is accepted only where the masterlist marks the
  field `nullable`.
- Per-mode values are preserved. Synchronization adds missing fields and
  reorders keys; it does not substitute generic values for existing values.

`devscripts/gamemode_json_sync.py` owns the canonicalization rule:

```text
python devscripts/gamemode_json_sync.py --check
```

`--check` reports missing fields, unknown fields, and field-order mismatches
without writing. It is the completeness/order gate for contributors and for
`config_selftest.py`.

```text
python devscripts/gamemode_json_sync.py --write
```

`--write` is the explicit one-time or intentional migration. It reads each
mode, preserves existing values, fills missing fields from the masterlist,
writes `null` where the contract means inherit, and emits a top comment pointing
at the masterlist. Ordinary loading and ordinary `--check` never rewrite a
mode file.

### Runtime validation

The runtime loader in `src/gamemode/gamemode.cpp` validates every mode before
installing it:

- Missing any masterlist field is an error. Required-field checks are retained
  for documentation, but the complete-file rule is enforced for all fields.
- Unknown top-level or nested fields are errors.
- Wrong JSON types, out-of-range numbers, and values outside an `enum` are
  errors.
- `null` is accepted only for fields the masterlist marks `nullable`.
- A mode’s declared `id` must match its filename stem.
- Duplicate mode ids are rejected during directory loading.
- A malformed file discovered during initial loading does not register a default
  or partially initialized mode.
- During hot reload, an invalid replacement is rejected and the last known-good
  mode stays active.

Validation errors are written to the current run’s `events.jsonl` as
`config.gamemode.invalid`, with the file path and exact setting path. The same
error is shown through the existing notification path as an in-game
`CONFIG ERROR` tip, rate-limited so a repeated invalid file cannot flood the
player.

`python devscripts/config_selftest.py` runs the same masterlist/schema checks
offline. Its gamemode section also runs the canonical `--check`, so a mode that
is missing a field, has an unknown field, or is out of order is reported before
it reaches a live match.

### Behavior-preserving defaults

The masterlist distinguishes three cases:

- A concrete default is written when the runtime loader applies that value
  regardless of whether the key was present.
- `null` is written for a presentation/trail override that should inherit
  `config/playervisuals.json` or another shared owner.
- `enabled: false` is written for an optional policy block that the mode does
  not use.

This is why adding every field to every mode does not silently enable payload
rules, NPC targeting, avatar policy, or presentation overrides. For example,
`payload.enabled: false` means the loader leaves `GamemodePayload.configured`
false, exactly like an omitted block.

### What still requires code changes

Completeness and validation guarantee structure, types, ranges, and order. They
do not make an unread field take effect:

1. Add a setting to the masterlist first, including its type, range, default,
   nullability, and description.
2. Make the owning loader consume it. A field that is only listed in the
   masterlist but ignored by the loader is a contract violation.
3. Add it to the runtime validation path and the offline validator through the
   same masterlist.
4. Run `python devscripts/gamemode_json_sync.py --write` to update every mode,
   then `--check`.
5. Run `python devscripts/config_selftest.py`, the focused gamemode selftest, and
   a live server/client scenario when the value affects gameplay or
   presentation.

### Related owners

- Masterlist and validator: `config/gamemodes/masterlist/gamemode-masterlist.json`,
  `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode.h`.
- Offline canonicalizer: `devscripts/gamemode_json_sync.py` and
  `devscripts/config_selftest.py`.
- Runtime presentation application: `src/network/community-match-client.cpp`.
- Shared presentation values: `config/playervisuals.json`, `config/hitfx.json`,
  `src/config/player-visuals-config.cpp`.
- Runtime diagnostics: `config.gamemode.invalid`,
  `player_visuals.config.rejected`, `player_visuals.outline`, and
  `player_visuals.trail` in the current `events.jsonl`.

## Gamemode runtime configuration resolution (2026-10-29)

`GamemodeRegistry` loads and validates mode data. It is not the runtime
application owner. One resolver produces the effective per-match values and is
the only place that applies or clears them.

### One resolver, one snapshot

`GamemodeRuntimeResolver` produces an immutable `GamemodeRuntimeSnapshot` for
the active mode. The snapshot contains effective values, not raw JSON:

- mode id, teams, roster, round/objective rules;
- movement preset and its precedence result;
- actor preset, camera FOV, first-person policy;
- healthbar policy;
- presentation policy (hit effects, outlines, trails, blood, hit flinch, victim
  blood feedback);
- death policy;
- NPC policy (behavior profile, targeting, travel, avatar);
- aim policy (`aim.mode`, `aim.body_mode`, `aim.body_enabled`).

Both the client and server consume that one resolver. The client applies it at
the community-match state boundary and clears it when the match state is reset.
The server applies it when a mode starts and refreshes it only when
`GamemodeRegistry::pollReload()` installs a new valid mode revision. Feature
systems must not keep their own per-mode enable flags or re-read the registry
every tick when the snapshot already carries the resolved value.

### Resolution order

Resolve in this order and document the winner where a field has more than one
source:

```text
validated per-mode JSON value
> actor-preset or replicated refinement
> base config JSON value
> compiled default
```

`null` or an absent nullable field means “inherit the next source”. A concrete
`true`/`false` or number in the mode JSON wins. This matches the existing
presentation/trail contract and keeps `config/gameplay.json`,
`config/aimbody.json`, `config/impact_decals.json`, and `config/hitfx.json` as
the shared base values rather than alternate mode stores.

### Per-mode aim and victim-feedback overrides

The gamemode masterlist owns the schema for these nullable fields; each mode
file owns its values:

- `aim.mode` — replaces `config/gameplay.json` `aim_mode`
  (`crosshair`, `world_hit`, `farpoint`, `camforward`, `physical`).
- `aim.body_mode` — replaces `config/aimbody.json` `mode`
  (`default`, `smooth`, `physical`, `hybrid`).
- `aim.body_enabled` — replaces `config/aimbody.json` `enabled`.
- `presentation.blood_client_feedback` — replaces
  `config/impact_decals.json` `blood.clientFeedback.enabled`. It is separate
  from `presentation.blood`, which remains the all-blood switch.
- `presentation.hit_flinch` — already exists and remains the hit-flinch switch.

These values are client-side presentation/firing policy. The dedicated server
does not load `gameplay.json`, `aimbody.json`, `impact_decals.json`, or
`hitfx.json`; if a server-side consumer ever needs the same aim value, it must
resolve it through this same resolver instead of assuming the client base file.

### Sandbox configuration sources

Sandbox is an ordinary mode file loaded by `GamemodeRegistry`. It currently
reads the raw shared JSON files for gameplay, aimbody, impact decals, hitfx,
and movement, plus the non-null presentation values in
`config/gamemodes/sandbox.json`. There is no merged Sandbox preset and no
Sandbox-specific override block.

This is the observed current behavior, not a settled design decision. Whether
Sandbox should continue to expose the raw user JSON values or should gain an
explicit configurable preset is tracked as an open decision in
`docs/specs/open-gameplay-issues.md`.

### Consolidation status

The resolver facade lands first. It replaces the scattered apply blocks in
`src/network/community-match-client.cpp` and the three server mode-start paths.
Deleting the per-singleton mode flags (`CamConfig`, `HealthbarConfig`,
`PlayerVisualsConfig`, `RagdollDeathConfig`, `ImpactDecalsConfig`,
`KillfeedManager`, `ActorPresetWeapons`) and the remaining per-tick registry
re-reads is a follow-up after runtime evidence proves the resolved values.

### Known duplicate owners (cleanup backlog)

| Area | Current owners | Cleanup |
|---|---|---|
| Client apply | `CommunityMatchClient::onState` / `reset` | Folded into the resolver; keep the client as the call boundary only. |
| Server apply | `serverCommunityStartMatch`, `serverStartMode`, `serverBombTagStartMatch` | One resolver apply path. |
| Server reads | `ServerGamemodeState` plus per-tick `GamemodeRegistry::get` re-reads | Refresh the snapshot only on registry revision change. |
| Hit/visual policy | `GamemodePresentation`, `ActorPresetWeapons` cache, `HitEffects`, `WeaponHitFxConfig`, `ActorPresetPresentation`, `PlayerVisualsConfig` | One resolved policy handed to each consumer. |
| Hit flinch | `CamConfig::mHitFlinchOverride` | Resolver owns the value; remove the singleton flag. |
| Blood | `presentation.blood`, top-level `blood_enabled`, `ImpactDecalsConfig` | Keep one canonical mode field; migrate the legacy alias. |
| Healthbars | `HealthbarConfig` mode flags | Three flags have no reader; implement or delete. |
| Movement | mode `movement_preset`, actor preset, role preset | Choose one precedence; client and server currently disagree. |
| Camera | registry fields, packet fields, actor preset | One resolved camera policy. |
| Death | `death_presentation`, `ragdoll_enabled`, `blood_enabled` | One death policy plus a separate blood policy. |
| Whole-file backup | `SettingsBackup` in the mode reset path | Delete; runtime overrides are memory-only. |
| Mode ids | `bombtag` vs `bomb_tag` | Normalize to the registry id. |
| `onlinemodes.json` | `score_limit`, `teams` | Delete; `goal_value` and mode `teams` are authoritative. |
| Dead mode fields | `spawn_groups`, `teams[].spawn_group`, objective timing, `payload.class_pool`, `npc_travel.explore_bias`, unused `features.*`, `spawn_tracer_seconds`, `allow_rematch` | Delete after updating the masterlist and self-tests. |

## Configuration families

These are the hot-reload families that are already heavily used or should be
made hot-reloadable over time.

| Family | Current files in this repository | Values that belong here |
|---|---|---|
| Movement | `config/movement.json`, `config/movement/` | acceleration, speed, gravity, jump, dash, air control, friction, movement mode |
| Collision and physics | `config/collision.json`, `config/collision-lod.json`, `config/weaponcollisions.json` | body dimensions, layers, tolerances, solver limits, weapon-world collision |
| Networking | `config/networkingconfig.json`, `config/network/`, `config/networking/` | tick and snapshot policy, interpolation, retry timing, timeouts, packet limits, ICE behavior |
| Gameplay | `config/gameplay.json`, `config/gameplay/`, `config/size_scaling.json` | damage, health, respawn, void bounds, scaling, knockback, interaction rules |
| Weapons | `config/weapons/<id>/config.json`, `config/weapons/masterlist/weapon-masterlist.json`, `config/weaponsets.json` | definitions, cooldowns, projectiles, ammo, slots, and network-owned weapon sets |
| Weapon presentation | `config/weapon_hitfx.json`, `config/weapon-cool-shot-line.json`, `config/weapon-tracers.json` | tracers, shot lines, impact visuals, timing, colors, visibility |
| Effects | `config/hitfx.json`, `config/impact_decals.json`, `config/effects/` | particles, decals, lifetimes, sizes, colors, sound references |
| Game modes | `config/gamemodes/`, `config/gamemode-good-maps.json`, `config/onlinemodes.json` | goals, timers, countdowns, maps, queues, team and gamemode rules |
| GUI and HUD | `config/gui/`, `config/killfeed.json`, `config/healthbar.json`, `config/rewards-hud.json`, `config/chatgui.json` | text, fonts, colors, positions, sizes, visibility, layout, notifications |
| Input and profiles | `config/current-profile.json`, `config/profiles.json`, future input/accessibility files | bindings, sensitivity, remapping, color-blind modes, text scale, contrast |
| Video and lighting | `config/video-settings.json`, `config/shadows.json`, `config/lighting.json`, `config/postfx.json`, `config/skybox.json` | resolution, quality, shadows, lighting, post-processing, frame limits |
| Audio | `config/audio/`, `config/notifications.json`, `config/tipsconfig.json` | volumes, routing, subtitles, notification behavior, music |

| Avatars and actors | `assets/avatars/<id>/avatar.json`, `config/animations/library/`, `config/animations/masterlist/animation-masterlist.json`, `config/bodyparts.json`, `config/player-procedural.json`, `config/npc-avatar.json`, `config/npc-difficulty.json`, `config/npcpresets/` | rig, animation state mapping, shared clips, body layout, cosmetics, NPC difficulty, actor presentation, procedural values |
| Authored animation | `assets/avatars/<avatar>/animations.json`, files included from `config/animations/` | clip keyframes, states, and authored pose data; live presentation tuning |
| Replay | `config/replay/`, `config/replayexport.json` | recording, playback, export, camera, event and effect capture |
| Map editor | `config/mapeditor.json` | editor font scales, outliner/search/scrollbar/properties geometry, handle sizes and thickness, block wireframe alpha, focus distance, camera speed, label mode, autosave delay |
| Debugging and testing | `config/debug/`, `config/debuglogger.json`, `config/hotreload.json`, `config/analytics.json` | debug categories, logging, profiling, reload diagnostics, test switches |

### Weapon definition completeness

`config/weapons/masterlist/weapon-masterlist.json` defines the complete
top-level field set for every `config/weapons/<id>/config.json`. Every file
keeps every listed key; an unused optional top-level field is explicitly
`null`. The masterlist currently treats nested objects such as `components`,
`sound`, and `viewmodel` as capability-specific objects, so it does not yet
require every nested key in every weapon.

When adding a new optional top-level field, add its type and nullability to the
masterlist, then run `py devscripts/weapon_json_sync.py --sync` to insert it
into each weapon file. Run `py devscripts/weapon_json_sync.py --check` to catch
missing or unknown fields. `py devscripts/config_selftest.py` runs this
completeness check too.

The inventory is a starting map, not proof that every file currently has the
same loader or reload behavior. Check the owning loader before changing one.

## Presets without a JSON explosion

An actor movement preset is one named data object, not one special code path.
The actor stores an ID such as `juggernaut`, `runner`, or `tf2`. The shared
movement system resolves that ID to one `MovementConfig` and uses the same
movement functions for every actor.

Presets may be authored in JSON or in hot C++ defaults. JSON is useful for
value tables; hot C++ is useful for formulas and behavior. They must not create
separate implementations. Resolve in this order: validated live override,
selected JSON preset, selected hot-C++ preset, canonical default.

The resolved preset ID, source, version, and hash must be written to JSONL.

## What should remain compiled

Keep platform integration, memory ownership, serialization machinery, security
boundaries, packet framing, cryptographic operations, and low-level rendering
plumbing in code. Their policies and tunable inputs should still be exposed as
configuration where safe.

Never put secrets, passwords, private keys, or account credentials in shipped,
committed, or reported JSON.

## Hot-reload priority

Implement and verify reload support in this order:

1. GUI, HUD, text, colors, fonts, and layout;
2. effects, audio routing, crosshair, shadows, lighting, and post-processing;
3. movement, weapon balance, damage, collision tolerances, and game-mode values;
4. NPC difficulty, avatar presentation, replay, and profiles;
5. networking timing and transport policy at safe session boundaries.

Movement constants and networking values should be editable, but their scope
must be explicit. A local presentation value may apply immediately; a shared
match or protocol value should be negotiated, applied on restart, or applied at
the next round so clients and servers cannot silently disagree.

## Verification

For every new or migrated JSON value, document the loader, owner, schema,
default, reload trigger, application boundary, validation command, and expected
diagnostic. A focused terminal test should prove the resulting state without
requiring a visual test for every edit.

For authored animation JSON, the owning feature contract is in
`docs/specs/animations/animations.md`: poll while actors update, apply valid
clip edits on the next fixed tick, preserve the last valid set on a bad save,
and make already-running actors use the updated set without a rebuild.
