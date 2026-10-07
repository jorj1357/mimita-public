# Juggernaut endless rounds and team outline correction

- Scope: make Juggernaut rounds repeat indefinitely, expose the round lifecycle in the GUI, and correct friendly/enemy outline resolution.
- Added an explicit endless-rounds rule. The server now treats a mode with endless_rounds: true as a valid round mode, never declares the match complete from round wins, and advances results into the next countdown.
- Added Juggernaut GUI layout content for intermission, 3-2-1-GO, round number, round-over text, score, and timer.
- Fixed outline team resolution to use the replicated community-match local team instead of the unsynchronized local Player field. Same-team remote actors resolve green; every opposing team resolves red.
- Fixed role resolution so the presentation-only `juggernaut_arcade` preset cannot replace explicit team roles. Juggernaut actors now retain `juggernaut_mode_juggernaut`, including its authoritative `5000` health, heavy movement, behavior profile, and loadout.
- Enabled the teammate outline layer and set its default color to green. Juggernaut mode still overrides opposing actors to red.
- Added a visible Fighter weapon picker during intermission/countdown/GO. Keys 1-4 choose Revolver, Shotgun, Spy Knife, or Rocket Launcher through the existing equip command path; the selection is locked for that life.
- Fixed the renderer to use the replicated actor team for both outline-layer selection and outline color, preventing stale local `matchTeam` data from making teammates red.
- Added the host-only `juggernaut_skip` command. It forces the active Juggernaut round to a draw, marks participating actors dead, shows the round result, and lets the normal next-round flow respawn everyone into the next weapon pick and countdown. Non-host use is forwarded as a future vote request but is explicitly not applied yet.
- Validation: Juggernaut and GUI JSON parsed successfully; explicit endless-round and HUD assertions passed; all touched translation units compiled. The gamemode self-test passed. Final linking was blocked by an already-running `mimita.exe` locking the executable, so no fresh executable or connected-client visual acceptance is claimed here. The actor-preset self-test still fails on pre-existing Counter-Strike preset expectations (reported revolver damage 35 versus expected 100), unrelated to the new Juggernaut preset.

## Weapon selection and role-slot diagnostics (2026-10-06)

- Root cause confirmed for Fighter weapon failure: the selected Fighter weapon was committed during intermission, then the client cleared/reconciled weapon state when the round-begin packet arrived without reapplying that selection after spawn installation.
- Root cause confirmed for Rocket Launcher `SLOT MISMATCH`: the client sent the Fighter role's logical slot 4, while server equip and attack validation resolved logical slots through the global default weapon set. The server now resolves logical/native slots through each actor's authoritative `weaponSetId` for objective rounds, including Spy Knife claim validation.
- Fighter selection is preserved through countdown/spawn and reapplied after authoritative spawn weapon runtimes are installed. Bounded structured events now cover `weapon.selection.requested`, `weapon.selection.applied`, `weapon.selection.apply_failed`, `weapon.equip.accepted`, `weapon.equip.rejected`, and `weapon.attack.slot_mismatch` in the canonical per-run `events.jsonl` journal.
- Validation: the timestamped executable `mimita-20261006T2218-weaponfix.exe` built successfully after the source fix. `--versioninfo` created and reported `logs/10-06-2026/20261006_221752/events.jsonl`; the journal contained the expected logger/versioninfo lifecycle events. A connected two-client Juggernaut weapon acceptance run was not performed in this pass, so gameplay acceptance remains open.
- Large Machine Gun follow-up design: add a config-driven hitscan weapon with a unique native slot, `fire_delay` around `0.16`, damage `20`, `magazineSize` `150`, reserve `9999`, `victim_knockback_per_damage` tuned initially around `2.0`, and the existing `assets/sound/weapon/machinegun/{machinegunequip,machinegunshoot,machinegunreload}.wav` sounds; add it to the Juggernaut weapon set only after the slot and balance values are reviewed.

## Quick local Juggernaut alias (2026-10-06)

- Added lowercase `rt1` to `config/command-aliases.json`. It expands through the existing `server_start_preset` command rather than creating a second startup path.
- Added `juggernaut_1` to `config/server-presets.json`: Juggernaut mode, host joins automatically, zero startup NPCs, automatic map rotation disabled, and Discord notification disabled.
- Balance and NPC-navigation values were intentionally not changed in this pass. The current role file resolves `juggernaut_mode_juggernaut` to 2000 health in `config/roles.json`; the perception and wall-navigation controls are in `config/behavior-profiles.json` and `config/npc-difficulty.json`, so those should be tuned in a separate measured pass.
- Validation: both edited JSON files parsed successfully; alias expansion remains case-sensitive, so the command is exactly `rt1`.

## Idempotent equip and Large Machine Gun (2026-10-06)

- Numbered `equipslotN` commands no longer toggle the currently equipped weapon off. Re-selecting the same slot is now idempotent; `equipslot0` remains the explicit unequip action. This fixes selecting Revolver again for a later Juggernaut round.
- Added `large_machine_gun` to `config/weapons.json` using the shared hitscan/automatic weapon path. It reuses the Revolver model, doubles the viewmodel scale, applies a dark-blue tint, uses the existing machine-gun equip/fire/reload sounds, has 150 magazine rounds and 9999 reserve rounds, 0.16 seconds between shots, high recoil, and heavy victim knockback.
- Replaced the Juggernaut role's default Big Shotgun with the Large Machine Gun and updated the role weapon set. The LMG uses 35 base damage, 1.85x headshots, and the existing distance falloff system with a 250 m start and 0.714 minimum fraction. This produces approximately 35 body / 65 head at zero distance and 25 body / 46 head at 250 m; exact 25/40 long-range head damage would require a separate per-body-part falloff model.
- Validation: weapon, weapon-set, and role JSON parsed successfully; machine-gun sound assets exist; `mimita-20261006T-lmg-juggernaut.exe` linked successfully; `--versioninfo` reported `logs/10-06-2026/20261006_222741/events.jsonl`. Full connected multiplayer and visual acceptance remain open.
- Final code rebuild after the equip-command description correction: `mimita-20261006T-lmg-juggernaut-v2.exe` linked successfully, with `src/terminal/weapon-commands.cpp` compiled. Its `--versioninfo` journal is `logs/10-06-2026/20261006_222829/events.jsonl`.

## NPC LMG audio, sustained fire, and guaranteed tracers (2026-10-06)

- The Large Machine Gun now uses the exact tracked assets `assets/sound/weapon/machinegun/machinegunequip.wav`, `machinegunshoot.wav`, and `machinegunreload.wav`. Its shoot sound is marked retriggerable so rapid automatic shots can replay the firing sound without waiting for the previous instance to finish.
- Added a dedicated `NETWORK_WEAPON_LARGE_MACHINE_GUN` mapping and remote-shot sound selection. This fixes the previous `NONE` mapping that caused the NPC firing broadcast to be skipped, and ensures remote clients receive the LMG tracer/effect shot event and machine-gun audio.
- Added a visible blue LMG tracer profile with an enabled flag, thicker start, longer lifetime, and full starting alpha. NPC hit/tracer presentation remains client-visible through the existing `SHOT_EFFECT_TRACER` event path.
- Increased the `juggernaut_limited` fire cadence multiplier to `3.125`. With the LMG's `0.16` second fire delay, a Juggernaut sustains automatic fire whenever its target and line-of-sight conditions allow it, rather than taking long pauses; this is the practical implementation of the requested long full-auto strings, not a random 90% burst roll.
- Validation: `mimita-20261006T-lmg-npc-tracers.exe` linked successfully; `--versioninfo` reported `logs/10-06-2026/20261006_223548/events.jsonl`; the journal contains the expected logger start, versioninfo, and stop events. Full connected multiplayer acceptance of NPC fire, audio retriggering, and on-screen tracer visibility remains to be performed.
