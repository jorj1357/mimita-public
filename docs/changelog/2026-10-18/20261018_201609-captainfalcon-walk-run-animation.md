# Captain Falcon walk and run animation

- Timestamp: 2026-10-18T20:16:09Z (2026-10-18 16:16:09 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Outcome

Exported the `walk20261018` Actions from `C:\important\quiet\blender-projects\mimita sizing 6.blend` using `tools/mimita_animation_exporter.py`. The result is one 60-tick, five-pose loop named `captainfalcon_walk`. Captain Falcon's avatar profile includes it and routes both its walk and run state to this same clip. The old inline run cycle was removed. The profile's other locomotion mappings and other clips remain in place.

The selected rig was `plrOrigin.030`, its six direct child Actions were current, and scene rate was 60 FPS. The source scene also assigned a root Action; because the exporter does not export root motion, that Action was unassigned in memory only during export. The `.blend` source was not saved or changed. The screenshot's phase markers are irrelevant to locomotion and were removed from the generic walk clip output. The clip is set to loop; the exported first/last sampled pose values differ by at most 0.02 after serialization.

## File changes

### `config/animations/library/locomotion/captainfalcon/walk.json` (new, lines 1–436)

- Before: no file existed.
- After: exporter-created JSON starts:

```json
{
  "version": 2,
  "clips": {
    "captainfalcon_walk": {
      "durationTicks": 60,
      "loop": true,
      "keyframes": [
```

It has five authored keyframes at ticks 0, 14, 29, 44, and 59; each key contains all six canonical parts with rest-relative transforms. It has no Melee phase markers.

### `assets/avatars/captainfalcon/animations.json`, lines 6–7, removed old `clips.run`, and lines 366–370

- Before:

```json
    "locomotion": { "walk": "walk", "fly": "walk", "crawl": "walk", "swim": "walk" },
    "run": "run",
```

- After:

```json
    "locomotion": { "walk": "captainfalcon_walk", "fly": "walk", "crawl": "walk", "swim": "walk" },
    "run": "captainfalcon_walk",
```

The old inline `clips.run` definition was removed because no state uses it after this mapping. The existing `walk` clip remains for fly/crawl/swim states.

The profile include list before this task ended with:

```json
    "config/animations/library/melee/captainfalcon/kick_a.json"
```

After export it ends with:

```json
    "config/animations/library/melee/captainfalcon/kick_a.json",
    "config/animations/library/locomotion/captainfalcon/walk.json"
```

### Dev Loop runtime observation

The existing live run was reused without restarting or terminating its client. Its journal `logs/2026-10-18/20261018_200125/events.jsonl` recorded `anim.set.reloaded` for `assets/avatars/captainfalcon/animations.json`, revision 3, `valid=true`, and an include path for the new locomotion file (tick 42363, 2026-10-18T20:14:18.922Z). This proves the live registry reloaded the profile and include. Later sampled `anim.melee.authored_pose` records showed `clip="idle"`, because the player was idle; they do not prove walk or run playback.

A bounded `agent.move 1 0 120` was queued through the existing dev loop, but it returned `delivered=false completed=false`; no movement scenario was proven. The live run remains untouched and running. No build or human visual acceptance occurred.

## Validation

- Blender 5.0 exporter completed successfully for the screenshot rig at 60 FPS, 60 ticks, and five authored poses.
- Parsed the new clip and profile JSON; confirmed `captainfalcon_walk` exists in the included file and both `states.locomotion.walk` and `states.run` reference it.
- Confirmed the obsolete inline run clip is absent and the profile's remaining clip/state data parses.
- `git diff --check` passed for the edited profile and new clip file.
- Focused review: `docs/skills/asset-checker-v1.md` — PASS for exported path/type and profile include; `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/workflows/runtime-scenario-validation.md` — profile hot-reload observed, motion scenario not completed, visible behavior unverified.
- Existing unrelated worktree edits were preserved.
