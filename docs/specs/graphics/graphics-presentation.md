# Graphics and Gameplay Presentation

This specification defines how gameplay visuals and camera presentation are
chosen, combined, reloaded, and verified. It is for gameplay, graphics, and
configuration implementers.

It does not define authoritative damage, hit registration, movement, or match
rules. A presentation setting may hide or show feedback, but it must not change
the result of gameplay. It does not replace the global video and accessibility
settings that players control for their own machine.

## Intent

The game should make its visible feedback understandable and configurable.
Designers should be able to edit several presentation choices for a mode in
that mode's existing JSON file, save once, and have the complete valid set take
effect together while playing. In particular, `config/gamemodes/juggernaut.json`
must describe and control the Juggernaut experience accurately. The same
contract applies to Counter-Strike and every other mode that supplies
presentation overrides.

Options must have a clear owner and observable result. A setting present in
JSON is not considered supported merely because it parses: its value must reach
the effect, HUD, camera, or renderer that produces the visible result.

## Presentation settings

Mode presentation policy belongs in the mode's existing `presentation`
object. A mode may omit any setting to inherit the normal actor or user
configuration. An explicit value must override that inherited value for the
scope described below.

The shared mode-level presentation contract covers these independently
controllable choices:

| Setting | Meaning |
|---|---|
| `damage_numbers` | Show damage values after confirmed damage. |
| `hit_effects` | Show effects on a damaged actor. |
| `world_impact_effects` | Show effects where a shot or projectile hits the world. |
| `hit_markers` | Show the attacker's hit confirmation marker. |
| `hit_sounds` | Play the attacker's hit confirmation sound. |
| `hit_flinch` | Apply the configured camera response when the local actor is hit. |
| `blood` | Allow blood effects. |
| `killfeed` | Show killfeed events. |
| `ragdolls` | Show ragdoll presentation. |
| `enemy_healthbars` | Show health bars for hostile actors. |
| `player_outlines` | Show player team/enemy outlines. |
| `friendly_outline_color`, `enemy_outline_color` | Set the respective outline colors when outlines are enabled. |

Each switch is independent: turning one off must not silently disable another.
For example, disabling damage numbers must not disable hit effects, and
disabling hit flinch must not disable damage or health changes.

Numeric presentation controls should be added to the same mode profile when a
mode needs to tune an effect's amount. Examples include a bounded dynamic-light
intensity/radius scale and camera aim-FOV multiplier, duration, and easing.
These controls require documented units, valid ranges, defaults, and the
existing rendering or camera owner that consumes them. Do not invent a second
lighting or camera implementation just to add a mode override.

The ordinary camera FOV remains role/actor-preset policy when it differs by
role, as in Juggernaut's Fighter and Juggernaut roles. A mode may define a
camera-wide or aim-FOV override only where its intended scope is explicit.
An aim-FOV change must leave the input condition, base FOV, multiplier,
transition duration, and easing understandable and inspectable.

Global graphics quality, display resolution, and player accessibility choices
remain local to the player. A mode can request a gameplay presentation policy
but must not silently change global resolution, turn VSync on, or lower/raise
the user's general quality settings. Mode effects that consume dynamic lights
must respect the user's global lighting limits.

## Ownership and combination

Use existing owners and configuration files. The mode owns match-wide feedback
policy; role and actor-preset files own role-specific appearance and baseline
camera policy; player video/accessibility settings own local quality and
accessibility. The effective setting for each option must be resolved once and
reported with its source. Explicit mode values take precedence only for the
options that the mode schema supports. Missing values inherit from the next
applicable owner rather than becoming an accidental `false` or zero.

When an actor preset and a mode both specify the same setting, the mode's
explicit match-wide value wins for that setting. Role-specific values, such as
the two roles' ordinary camera FOV, stay role-specific unless the mode has an
explicit override for that camera behavior. Leaving or changing a mode must
restore the previous local settings and clear the prior mode's overrides.

Do not copy the complete actor preset into every gamemode file. Add a field to
the mode schema only when the mode needs to control that behavior across its
participants. Reuse the existing effect/camera/render owner and document the
resolver order where a field has more than one source.

## Editing and live reload

The desired workflow is:

1. Edit several related settings in the active mode file, for example
   `config/gamemodes/juggernaut.json`.
2. Save once while the game is running.
3. The mode's configuration owner reads and validates a complete candidate.
4. If valid, it installs the candidate as one new presentation generation at a
   safe client presentation boundary. All changed options become effective
   together; no frame or event uses a mixture of old and new values.
5. If invalid or incomplete, retain the complete last-known-good generation,
   report the rejected file/field, and try again after a later file change.

Presentation changes should not require rebuilding or restarting the game.
For an active match, client-only visual changes may apply on the next rendered
frame. Any value that affects shared simulation, match authority, or network
agreement is outside this presentation reload contract and must use its own
documented safe boundary.

Expose an inspection path that reports the active mode, source file, generation
or content hash, and effective value/source for every supported setting. A
reload success message alone is not proof that the visual consumer applied the
new value.

## Current evidence and known gaps

`GamemodeRegistry::pollReload()` watches mode files, parses into a candidate,
and retains the previous valid mode when JSON parsing fails. This is useful
existing infrastructure for bulk edits. It does not by itself prove that every
presentation consumer receives the new values together or expose an effective
profile generation.

The mode parser in `src/gamemode/gamemode.cpp` reads the presentation switches
listed above, but the client currently applies only a subset of those mode
values directly. `src/network/community-match-client.cpp` applies mode-level
player-outline and hit-flinch overrides. Damage numbers, actor hit effects,
world impact effects, blood, hit markers, and hit sounds are also controlled by
actor-preset application in `src/combat/actor-preset-weapons.cpp`; their
mode-level parsed values do not currently all flow to those consumers. The
current Juggernaut JSON sets `damage_numbers`, `hit_effects`,
`world_impact_effects`, and `hit_markers`, but this source path does not prove
that each of those values controls the corresponding visible effect.

Camera FOV is split across actor presets and gamemode camera policy. The
Juggernaut actor presets set different forced FOV values for Fighters and
Juggernauts. Mode aim-FOV configuration is parsed for modes that define
`camera.aim_fov`; Juggernaut does not currently define one. Dynamic light
settings are loaded by `src/render/dynamic-light-config.cpp` from the global
`config/lighting.json` owner. There is no demonstrated Juggernaut-specific
dynamic-light amount override in the current consumer path.

These are source-level findings, not live-runtime verification. The required
implementation work is to route each supported mode value through its existing
consumer, resolve mode/actor/user precedence, and make the effective profile
inspectable. Counter-Strike's current JSON is a useful comparison case, but its
presence or successful parsing does not prove that all modes or all options
work.

## Validation and acceptance

Source and configuration checks must show that:

- every documented field has one parser, one owner, a default/inheritance
  rule, validation, an application boundary, and a visible consumer;
- each mode can independently turn every documented feedback option on and
  off, and numeric controls stay inside documented bounds;
- saving multiple valid edits to one mode profile changes all corresponding
  effective values together without a restart or build;
- a malformed edit preserves the entire prior profile rather than applying
  only some fields;
- leaving the mode restores the previous local settings;
- the inspection output identifies the source and effective value, including
  explicit mode overrides and inherited values.

Runtime acceptance must use the dev loop's freshly identified executable,
capture its `EVENTS_JSONL_PATH`, exercise actual hits, misses, world impacts,
camera input, and mode exit in Juggernaut, then repeat the relevant controls in
Counter-Strike. Inspect the live journal for the mode/config generation and
effective values; visually check the effects in-game. Build success, parsed
JSON, or a diagnostic line alone does not prove visible behavior. Human visual
acceptance remains separate from source and build evidence.

## Decisions still needed

- Which numeric dynamic-light controls should be available per mode, and
  should the mode specify a multiplier, a direct intensity/radius, or a named
  preset? The existing global lighting limits must remain authoritative.
- Does Juggernaut need a mode-wide aim-FOV policy, or should its Fighter and
  Juggernaut actor presets continue to own all camera FOV behavior?
- Which additional numeric camera/effect amounts belong in the first shared
  mode schema? Add them only after naming their existing owner and intended
  visual result.

Until these questions are answered, keep numeric field names/ranges as
proposals. Do not claim that they already exist or choose final Juggernaut
values by inference.

## Original user notes (2026-10-20; context, not a complete schema)

```text
2026 10 20 1955

presnetiatn like

damage n umbers on or off
dmg effect on or off
hit flinch on or off
how much dnamic lighitng
how much fov
how much  etc etc
this and that

this is  mor focused on juggernaut bc juggernaut is having some visual issues it a lot of things gogin on and i dont udnerstand reallt taht well
soooooooooooo

need to maek sure that 

C:\mimita-v9\config\gamemodes\counterstrike.json
this is good and works i think
but the issue is like 
everuthing else needs to work too
not just this 
```
