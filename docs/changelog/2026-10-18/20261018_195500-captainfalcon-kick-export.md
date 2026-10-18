# Captain Falcon kick animation export

- Timestamp: 2026-10-18T23:55:00Z (2026-10-18 19:55:00 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Outcome

Exported the kick action from `C:\important\quiet\blender-projects\mimita sizing 6.blend` with `tools/mimita_animation_exporter.py` to `config/animations/library/melee/captainfalcon/kick_a.json`. It contains version 2, clip ID `kick_a`, duration 59 ticks, five authored pose keys at ticks 0 through 58, all six canonical body parts, and melee markers at windup 0, active 20, recovery 40, and exclusive end 59.

## Why the user's export failed

The saved scene is 60 FPS and has the six-part `plrOrigin.029` rig used by the `kick20261018` collection. However, its `action.end` marker is on Blender frame 59. The exporter maps frames 1–59 to 59 samples (ticks 0–58), so the required exclusive end marker must be on frame 60 (tick 59). After correcting this in memory, the exporter then rejected the assigned `plrOrigin.028Action.001` on the root because root Actions are not supported.

For this export only, the script was run through Blender 5.0 with the end marker moved to frame 60 and the root Action unassigned in memory. The `.blend` file was not saved or modified. The root transform itself remained intact, including the calibrated Blender-to-game basis. The exported file remains pose data; this does not connect it to a gameplay kick action or prove runtime playback.

## File change

### `config/animations/library/melee/captainfalcon/kick_a.json` (new, lines 1–474)

- Before: no file existed.
- After, lines 1–10:

```json
{
  "version": 2,
  "clips": {
    "kick_a": {
      "durationTicks": 59,
      "loop": false,
      "keyframes": [
```

The complete addition was written by the Blender exporter; each sampled key has translation and WXYZ quaternion deltas for head, torso, both arms, and both legs.

## Validation and review

- Blender 5.0 opened the specified `.blend`; scene FPS is 60, the kick collection resolves to `plrOrigin.029`, and all six part Actions were assigned from the scene.
- The exporter completed and wrote the exact path above.
- Parsed the exported JSON and confirmed one version-2 clip, five keys, tick range 0–58, valid 59-tick duration, and required ordered melee markers ending at tick 59.
- No build or game runtime playback was run. Human visual acceptance in MiMITA remains needed.
- Focused skills: `docs/skills/asset-checker-v1.md` — PASS for path and JSON output; `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; no runtime or gameplay connection was claimed.
- Relevant specs: `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`.

## Preserved pre-existing work

The worktree already had unrelated modified configuration/spec files and existing changelogs, including earlier changes to the exporter and `punch_a.json`. Those edits were preserved and are not part of this export.
