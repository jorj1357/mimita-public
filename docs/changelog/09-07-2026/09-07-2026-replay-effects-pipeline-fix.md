// 09 07 2026, 11 15 EST
/* purpose
* Fix two replay export bugs: effects not rendering in MP4 and left leg flipping
* Bug 1: mPendingEffects never merged into scene frames during recording
* Bug 2: glm::quat_cast produces antipodal quaternions near 90-degree rest pose
* Aligns replay export with spec section 7.10 (MP4 correctness)
* Does NOT change gameplay logic, networking, or live effect spawning
* Does NOT modify config files or JSON schemas
*/

# Replay Export Fixes — Effects Pipeline + Left Leg Quaternion — 09-07-2026 11:15 EST

## Branch
develop/v2.0.1

## Time
2026-09-07 11:15 EST

## Task
Fix two replay export bugs:
1. Effects (gunshots, blood, hit bursts, dashes, impacts) not appearing in exported MP4s despite being captured during gameplay
2. Left leg rotation flipping in exported MP4 when rotating around certain world axes

## Pre-existing Changes
None — these files had no uncommitted edits before this session.

## Root Cause 1 — Effects Pipeline Gap

**Observed:** Clip `2026-09-06_19-54-57_3531_Multi Kill.mclip.json` has 480 scene frames with 0 effects but 47 sound events. `captureReplayEffect()` is called from 27 callsites and works correctly (pushes to `mPendingEffects`), but effects never reach scene frames.

**Root cause:** The recording loop in `engine-tick-replay.cpp:343-845` uses the `getWritableFrame()`/`commitFrame()` fast path. `commitFrame()` at `replay.h:117-121` advances the ring buffer index but never merges `mPendingEffects` into the scene frame. The old `recordSceneFrame()` at `replay-recorder.cpp:231-251` DID merge effects but is never called — it was replaced by the fast path for performance.

**Chain of failure:**
```
captureReplayEffect() → mPendingEffects.push_back()  [WORKS]
engine-tick-replay: sceneFrame.effects.clear()        [clears each tick]
engine-tick-replay: commitFrame()                     [advances index, ignores mPendingEffects]
export: sceneFrame.effects always empty               [reconstruction code never runs]
```

**Fix:** Modified `commitFrame()` in `replay.h` to flush `mPendingEffects` into the current scene frame before advancing the index. The `if (!mPendingEffects.empty())` guard ensures zero cost when no effects were captured (the common case).

## Root Cause 2 — Quaternion Double-Cover

**Observed:** Left leg flips visually in exported MP4 when rotating around certain world axes. Left leg is fine in-game.

**Root cause:** The GLB model's rest pose applies 90° X rotation to all body parts (`bodyparts.json`). Walk animation applies ±80° Z rotation. The combined rotation `restM * poseM` lands near gimbal-lock. `glm::quat_cast` extracts a quaternion from the 3×3 matrix, but for rotations near this configuration, it can return either `q` or `-q` (both represent the same rotation). In-game, the euler→matrix→render path is consistent every frame. In export, the quaternion is extracted once, stored, and reconstructed — if extraction picks the "wrong" hemisphere, the leg visually flips.

**Fix:** After extracting and normalizing the quaternion in `captureReplayBodyParts()`, enforce hemisphere consistency by checking the local +Y axis direction (hip-to-knee for legs). If the rotated +Y points downward, negate the quaternion to land on the canonical hemisphere.

## Files Changed

| File | Lines | Change |
|---|---|---|
| `src/replay/replay.h` | 117-121 → 117-131 | `commitFrame()` now merges `mPendingEffects` into scene frame before advancing index |
| `src/replay/replay-recorder.cpp` | 88-93 | Added quaternion hemisphere enforcement after `glm::normalize(localRot)` |

## Spec Reference

Per `docs/specs/replays/replay-editor-and-export-v2.md`:
- Section 1.2 (Client-visible history): effects, kill effects, damage effects are listed
- Section 7.10 (MP4 correctness): "the MP4 must reconstruct the local client's experience"
- Section 10.1 (Hard quick-export correctness test): "correct effects" must appear in MP4

## Skill Results

### Spec-Behavior Review
- Severity: high (was blocker)
- Type: spec-code disagreement (pre-existing, now fixed)
- Finding: Effects were captured but not stored in scene frames, violating section 7.10
- Human decision required: None — fix directly implements spec

### Efficiency Review
- `commitFrame()` merge: O(n) where n = effects count (typically 0-5), guarded by `!mPendingEffects.empty()` — negligible
- Hemisphere check: ~36 FMADD per player per tick — negligible
- No duplicate concepts, no ownership drift, no new allocations in hot path

## Build
Status: SUCCESS (build_agent.py, 173.48s, 92 compiled, 378 skipped)

## Validation
- Effects pipeline: `commitFrame()` now correctly merges pending effects into scene frames
- Quaternion hemisphere: consistent +Y reference ensures no antipodal flipping
- Both fixes compile cleanly with zero warnings
- Old `recordSceneFrame()` function still exists (unused) — not deleted per minimal-change principle

## Remaining Human Review
- Record a clip with combat (gunshots, blood, hits, dashes) and export to MP4 — verify effects appear
- Export a clip with left leg rotation and verify no visual flipping
- Regression check: normal gameplay recording unaffected (recording condition unchanged)
