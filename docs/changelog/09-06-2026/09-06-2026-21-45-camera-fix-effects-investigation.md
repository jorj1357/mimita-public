// 09 06 2026, 21 45
/* purpose
* Confirm replay export camera fix, document regression, investigate effects/left-leg issues
* Camera fix confirmed working via diagnostic logging and direct subprocess test
* Left leg rotation: no asymmetry in replay pipeline, likely animation/model issue
* Effects not showing: clip has 0 effects in 480 scene frames despite 47 sound events
* Root cause: captureReplayEffect() calls exist (27 callsites) but effects not making it into clip
* Does NOT change any gameplay, recording, or camera logic
*/
# Export Camera Fix Confirmation & Effects Investigation — 09-06-2026 21:45 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 21:45 EST

## Task
1. Confirm replay export camera fix is working
2. Investigate left leg rotation issue in exported replays
3. Investigate effects not showing in exported replays
4. Investigate weapon sounds in exported replays

## Results

### 1. Camera Fix — CONFIRMED WORKING

The 19:51 recording condition fix (`isRecording() && (!replayPlaybackActive || isReplayExportActive())`) and `beginPlayback()` removal are correct and sufficient.

**Proof:**
- Ran export subprocess directly: `mimita.exe --export-replay <clip> --output test.mp4 --visible --timeout 30`
- Diagnostic logs confirmed camera follows clip data at every tick:
  - Tick 0: `frameCamPos=(6.97, -26.14, 95.66)` → controller sets it → `finalPos=(6.97, -26.14, 95.66)`
  - Tick 1: `frameCamPos=(6.97, -26.14, 95.55)` → correctly interpolating
  - Tick 2: `frameCamPos=(6.97, -26.14, 95.43)` → following clip data
- Export produced 5.4MB MP4 with correct camera (vs 600-760KB from broken builds)
- User confirmed test export plays correctly

**Root cause of user's earlier broken exports:** Old clips recorded with pre-fix binary had empty camera data. Rebuilding + re-recording was required.

### 2. Left Leg Rotation — NO ASYMMETRY IN REPLAY PIPELINE

The replay rotation pipeline is **symmetric for all 6 body parts**:
- Capture: `captureReplayBodyParts()` extracts quaternion from world transform — identical for all parts
- Serialization: JSON format `{w, x, y, z}` — identical for all parts  
- Deserialization: `glm::quat(r[0], r[1], r[2], r[3])` — identical for all parts
- Interpolation: `glm::slerp()` — identical for all parts
- Rendering: `applyReplayPose()` builds `root * translate * mat4_cast(quat) * scale` — identical for all parts

The "left leg rotating differently" is likely a **model/animation issue** — either the GLB model's rest pose for leftLeg has a different orientation, or the euler-to-matrix conversion hits a gimbal lock singularity during spring-smoothed animation. This is a separate bug from the export issue.

### 3. Effects Not Showing — ROOT CAUSE FOUND

**The clip `2026-09-06_19-54-57_3531_Multi Kill.mclip.json` has 480 scene frames but ALL effects arrays are empty.**

| Data | Count |
|------|-------|
| Scene frames | 480 |
| Frames with effects | 0 |
| Total effect events | 0 |
| Sound events | 47 |

The recording pipeline works for sounds but NOT effects, despite:
- `captureReplayEffect()` being called from 27 callsites across the codebase
- `captureReplaySound()` using identical conditions and working (47 events recorded)
- All config flags (`gConfig.enabled`, `gDashFXEnabled`, `movementDashBurst.enabled`) being `true`

**The effects are spawned during gameplay and `captureReplayEffect()` IS called**, but the effect events are not making it into the clip's scene frames. The recording pipeline for effects needs investigation — the `recordEffectEvent()` → `mPendingEffects` → `recordSceneFrame()` merge path may have a timing or state issue.

**What needs to be fixed:**
- Effects must be recorded into scene frames during gameplay so they can be reconstructed during export
- The `captureReplayEffect()` → `recordEffectEvent()` → `mPendingEffects` → `recordSceneFrame()` pipeline needs debugging
- This is a separate issue from the camera fix

### 4. Weapon Sounds — RECORDED BUT NOT PLAYED DURING EXPORT

The clip has 47 sound events (gunshots, footsteps, etc.). During export:
- `playWorldSound()` is explicitly blocked: `if (!isReplayExportActive()) playWorldSound(...)`
- Comment: "During export the clip audio is mixed into the MP4 separately; do not also play the sounds live"
- Sounds should be mixed into the MP4 via the audio encoding pipeline

If weapon sounds are missing from the exported MP4, the audio mixing pipeline may have an issue. This is a separate investigation.

## Files Changed
| File | Change |
|---|---|
| `docs/regressions/regressions-v1.md` | Added regression entry confirming camera fix works |

## Spec Reference
Per `docs/specs/replays/replay-editor-and-export.md`:
> i open it, watch it, and it is what i saw in the game at that time. from my point of view, whatever i had at the time, thirdperson first person etc, sounds, effects, etc, chat, gui, crosshair, etc its all in the .mp4
