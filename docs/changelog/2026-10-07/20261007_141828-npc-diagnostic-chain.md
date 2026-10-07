# NPC diagnostic chain

## Scope

Added bounded StructuredLogger diagnostics for the current NPC investigation. Gameplay behavior was not changed.

## Diagnostics

- `npc.utility-decision`: logs utility goal/action transitions with target and objective context.
- `npc.pursuit-state`: logs visible versus remembered target transitions, confidence, memory age, and last-known position.
- `npc.grenade-availability`: logs each NPC life's loadout, weapon-definition presence, runtime presence, and ammo.
- `npc.grenade-equipped`: logs successful grenade-launcher weapon switches.
- `npc.objective-context`: now includes bomb carrier, site index, distance, interaction range, and eligibility reason.
- Existing movement snapshots, nav failures, wall escapes, and stuck episodes remain bounded and enabled.

## Configuration

Enabled the `grenade_launcher` logger category at `important` for this focused investigation. Other categories retain the October 6 profile.

## Evidence

- Build: `mimita-20261007Tnpc-diag-v2.exe`, build success, 74 compiled units.
- Version info: build commit `64b085c`, executable build timestamp `Oct 7 2026 14:15:43`.
- Generic headless run: `logs/10-07-2026/20261007_141722/events.jsonl`.
- Counter-Strike mode run: `logs/10-07-2026/20261007_141801/events.jsonl`.
- Counter-Strike run emitted 218 structured records, including 88 movement snapshots, 18 stuck episodes, 8 utility decisions, 8 pursuit states, 8 objective contexts, and 8 grenade-availability records.

## Findings

- The headless run spawned NPCs with `team=-1`; therefore it did not create an active CT/T bomb round and could not prove planting or defusing.
- The grenade-availability records show the Counter-Strike NPC loadout contains `revolver`, `shotgun`, and `rocket_launcher`, but not `grenade_launcher`. The grenade weapon definition is registered, so the current gap is upstream loadout/equip wiring rather than registration.
- The navigation records show repeated stuck episodes near both spawn areas, with low or zero wall-avoidance counts in some episodes. This is useful evidence for the next navigation owner investigation, but is not yet a behavior fix.
