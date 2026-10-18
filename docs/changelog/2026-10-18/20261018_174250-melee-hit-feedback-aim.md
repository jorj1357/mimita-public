# Melee confirmed hit feedback, hitbox tuning, and aim mode

- Local time: 2026-10-18 13:42 EDT; recorded at 2026-10-18T17:42:50Z.
- Branch: `20261011zt`; no commit created.
- Preserved existing edits and the active client process. Did not relaunch the client or start a replacement server.

## Specification and configuration

- Added `docs/specs/gamemodes/supersmashbrosmelee.md:719-723` with confirmed-hit impact/audio feedback requirements and the Melee RMB aim policy. RMB remains the secondary move input.
- Added `config/gamemodes/melee.json:17-22`:

```json
"camera": {
  "aim_fov": {
    "enabled": true,
    "input": "none",
    "multiplier": 1.0
  }
}
```

- `config/weapons.json` already had per-move `hitboxes`, sphere `size`, limb attachment, `local_offset`, active tick window, and optional `per_tick` overrides. `reloadBuiltinWeaponsIfChanged()` polls it for both the weapon system and server. In-progress actions keep their starting revision; changed values apply to the next action.

## Implementation

- `src/network/server-melee-actions.cpp:313-356, 456-458, 552-554, 717, 868` now queues one reliable `PACKET_SHOT_EVENT` after each confirmed player/NPC hit. It carries the confirmed point and direction, with the Melee marker and no health confirmation or knockback. Both server hitbox paths now use the same 0.01 m minimum as the local visual, replacing the old 0.2 m clamp.
- `src/network/packets.h:300` adds `SHOT_EFFECT_MELEE_CONFIRM` so confirmed Melee impacts can be shown to the local attacker despite ordinary locally predicted gun feedback being suppressed.
- `src/network/multiplayer-shots.cpp:166-177` leaves the gun-shot serial sequence unchanged for Melee feedback; reliable event deduplication handles the impact event.
- `src/engine/engine-tick-net.cpp:754-757, 798-808, 892-935` displays confirmed impact effects to the attacker and observers, skips damage numbers and gunshot replay labeling, and uses the `captain_falcon_moveset` hit sound configuration at 55% base volume.
- `src/engine/engine-tick-camera.cpp:1053-1057` honors the mode's aim-FOV input; `none` leaves RMB from narrowing the camera FOV. The existing Melee-authored-pose path already suppresses physical arm aiming.
- Added `docs/regressions/2026-10-18/melee-hit-feedback-and-hitbox-accuracy-REG.md` with the confirmed feedback gap and remaining hitbox-position mismatch.

## Validation and remaining evidence

- Parsed `config/gamemodes/melee.json` successfully and ran `git diff --check` on the changed code/config files. The Melee specification has older trailing whitespace in informal notes; those notes were not changed.
- Canonical dev-loop build 2597 compiled four touched C++ translation units; build 2598 compiled the replay follow-up; build 2599 compiled the final audio-config change. Build 2599 is current and published at `.dev/builds/2599`. Its `--versioninfo` check passed; `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_134239/events.jsonl`.
- The managed client is still running build 2594 and its managed server is reported dead. No live hit-feedback, hot-edit, RMB, multiplayer, or visual/audio acceptance was performed on build 2599.
- Known unresolved accuracy gap: the local Melee visual uses sampled animated limb transforms, while authoritative server contact estimates hitbox centers from actor-root offsets. Per-move size editing is live and exact down to the shared 0.01 m floor, but size changes alone cannot prove visible-contact parity.
