# Captain Falcon landing animation

- Status: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-18T20:21:43Z (2026-10-18 16:21:43 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4`
- Scope: export the Blender `land20261018` action through `tools/mimita_animation_exporter.py` and select it for Captain Falcon's landing state.

## Final changes

1. `assets/avatars/captainfalcon/animations.json`
   - `states.land` changed from `"land": "land"` to `"land": "captainfalcon_land"`.
   - Removed the old inline `clips.land` entry. It was an unused duplicate after the new library clip became the state target.
   - Added `"config/animations/library/locomotion/captainfalcon/land.json"` to `include`.
   - This file already contained pre-existing walk/run, punch, and kick work from earlier tasks. Those changes were preserved.
2. `config/animations/library/locomotion/captainfalcon/land.json` (new)
   - Exported clip ID `captainfalcon_land`, six canonical parts, normalized WXYZ quaternion rotations, and two keyed poses at ticks 0 and 6.
   - `durationTicks` is 7 and `loop` is false. The runtime selects landing for 0.12 seconds, about seven fixed ticks, so the exported 60-frame Blender action was fitted to that existing state window; no gameplay timing code changed.
   - Removed the Blender timeline's phase markers because this is a generic landing clip.

## Source and export evidence

- Blender source: `C:\important\quiet\blender-projects\mimita sizing 6.blend`.
- Selected rig: `plrOrigin.035`, collection `land20261018`; its six child Actions are `head.034Action`, `leftArm.035Action`, `leftLeg.030Action`, `rightArm.035Action`, `rightLeg.030Action`, and `torso.032Action`.
- Blender reported 60 FPS and each Action spans frames 1–60. The exporter reported “Export complete,” 60 source ticks, two authored poses, and the output JSON path above. The root Action was cleared in Blender memory for the export because the tool rejects unsupported root motion; the source `.blend` was not saved or changed.
- Exporter reference rest model: `assets/entity/player/default/mimita-char-no-animations-v4.glb`.

## Validation and runtime evidence

- Both JSON files parse. The profile's land state matches the clip ID, its include path exists, the included file defines that ID, the clip has key ticks `0,6`, duration 7, and `loop:false`.
- `git diff --check` passed for the changed profile; Git emitted only its existing LF-to-CRLF normalization notice. New JSON content was parsed and inspected directly.
- Existing live dev-loop client was preserved (build 2607, PID 2268; journal `logs/2026-10-18/20261018_200125/events.jsonl`). No executable was rebuilt or restarted for this JSON-only change.
- The live journal records `anim.set.reloaded`, revision 6, `valid:true`, with `config/animations/library/locomotion/captainfalcon/land.json` in `include_paths` at tick 2973. It later records `anim.state.changed` from `fall` to `captainfalcon_land` at tick 5550, then to `jump` at tick 5558; other repeated land transitions returned to walk. This proves profile loading and live state selection in the existing client. The journal does not show a rendered pose, so visual quality remains unverified.
- Human review still needed: confirm the landing motion looks right in game.

## Documents and focused review

- Read: `docs/ROUTER.md`, `docs/specs/animations/animations.md`, `docs/specs/animations/blender-animation-authoring.md`, `docs/specs/avatar/avatar-editor.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/asset-management/asset-management.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, and `docs/features/live-code-development/live-code-development.md`.
- Focused reviews: `docs/skills/spec-behavior-review-v1.md` — PASS; the new clip is assigned to the documented land state. `docs/skills/asset-checker-v1.md` — PASS; the shared clip path is included and loaded by the avatar profile.
- No regression was confirmed. No source code or gameplay timing was changed.

## Preserved pre-existing worktree changes

The worktree already had unrelated edits before this task, including `config/weapons.json`, melee implementation files, several specs and regressions, the animation exporter, Captain Falcon punch/kick assets, Captain Falcon walk/run profile changes, and earlier changelogs. They were not reverted or folded into this change. The profile file was already dirty from those earlier animation requests; this task only changed the land mapping, removed the now-unused inline land clip, and appended the land include.
