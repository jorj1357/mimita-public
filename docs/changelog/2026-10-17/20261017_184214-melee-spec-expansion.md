# Melee match and animation specification expansion

- UTC: 2026-10-17T18:42:14Z
- Local time: 2026-10-17 14:42:14 EDT
- Branch: `20261011zt`
- HEAD observed: `bc4f69817358bcfcc2d4fafabafe7e410402e0a0` (`better dev loop yay`)
- Scope: specification and documentation only. No gameplay source code, build, tests, or live runtime were changed.
- Instructions followed: `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.

## Repository state and ownership

A user-created commit advanced the shared branch to `bc4f6981` while this work was active; I did not create that commit. `config/accounts/default.json` was already modified outside this task and was preserved. No unrelated change was included in this documentation update.

## Specification changes

### `docs/specs/gamemodes/supersmashbrosmelee.md:9-54, 125-139, 157-179, 181-227, 241-299, 310-541`

Expanded the Melee contract with always-active move set, camera-forward captured at attack start, movement-only WASD, immediate cancellation, momentum retention on moves that suppress steering, per-move hit volumes attached to animated body parts, percent/knockback, immediate prediction, and low-control hit reaction visuals. Added confirmed controls and rules: four lives, void-only death, 0 percent after respawn, jump scaling over 1–10 ticks, double jump, fast-fall/crouch, Q shield, and E grab. Added `Melee` mode flow (15-second/900-tick intermission, map selection, frozen 3/2/1 countdown, GO release), one-human-versus-one-NPC Captain Falcon first slice, shared player/NPC flat-top body collision, and ten acceptance scenarios. Open values remain explicitly marked, including move roster/timings, hitbox shape and repetition, percent/knockback tuning, spawn and blast-zone details, shield recovery action, grab limits, countdown duration, map picker, and Ctrl+attack downward-attack mapping.

Before (section 32):

> **DECIDED for the first version:** Each fighter starts with four lives. A fighter loses one life only by being knocked out of the stage and dying in the void beyond its blast zone. When lives remain, the fighter respawns; losing the fourth life eliminates them. Ordinary health damage, hazards, or another cause must not kill a fighter in this version. **OPEN:** respawn delay and location, spawn protection, and whether percent resets on respawn.

After (section 32):

> **DECIDED for the first version:** Each fighter starts with four lives. A fighter loses one life only by being knocked out of the stage and dying in the void beyond its blast zone. When lives remain, the fighter respawns at 0 percent; losing the fourth life eliminates them. Ordinary health damage, hazards, or another cause must not kill a fighter in this version. **OPEN:** respawn delay and location, spawn protection.

Before (section 41):

> **PROPOSED:** Trigger sounds and effects from deterministic move events or tick markers so predicted and authoritative playback can be reconciled and duplicate effects avoided.

After (section 41):

> **DECIDED:** Every animation used by a Melee move has an immediate start sound at clip tick 0 and may have additional sound events at named ticks. Starting another animation on the same actor immediately stops its currently playing Melee animation sound and starts the new one; repeating a sound restarts it from the beginning.

Before (section 51 facing):

> Directional input may choose a move variant relative to the captured camera-forward basis, but the exact W/A/S/D mapping remains **OPEN**.

After (section 51 facing):

> **DECIDED:** W/A/S/D is movement-only and never selects an attack variant or changes attack direction in this version.

New sections 52–54 had no prior text. They define match lifecycle, shared actor-body collision/predicted knockback goals, and acceptance scenarios. For the hitbox contract, the old requirement that the rig part itself be physical was replaced by:

> The hitbox is a separate move-owned gameplay volume, not the limb mesh or actor body collider. It may extend past the visible limb for reliable reach, while remaining anchored to and moving with that limb; it cannot independently aim, teleport, or scale with frame rate.

### `docs/specs/animations/animations.md:225-375, 425-440`

Added the `audio` clip field and tick-0/additional-tick sound event format, including retrigger/cutoff, cancellation, prediction deduplication, and a per-actor Melee animation sound channel. Expanded Blender-to-runtime conversion: sample at simulation ticks, extract local part transforms, apply one explicit basis conversion, normalize Euler/quaternion input into normalized local quaternion channels, validate, then SLERP. Euler and quaternion exports must produce equivalent matrices in conversion fixtures. Added the Melee action override path so a replacement move cancels the current clip at once and suppresses later markers from the canceled clip.

Before (clip fields):

> | `keyframes` | array | required | Ordered keyframes. |

After:

> | `keyframes` | array | required | Ordered keyframes. |
> | `audio` | object | optional | Tick-addressed animation sounds; schema below. |

New section “Blender and file-to-runtime conversion pipeline” had no prior text. Its coordinate basis `B` is intentionally still an implementation prerequisite; the documents do not currently establish the exact Blender/game axes. No coordinate convention was invented.

### `docs/specs/animations/blender-animation-authoring.md:16-68`

Updated Blender export to accept either Euler XYZ degrees or WXYZ quaternions, convert once into normalized game-local space, report its basis policy, and export immediate plus tick-addressed sounds. Added equivalent-rotation and tick-mapping acceptance checks.

Before:

> Export the action as data that converts into the existing `animations.json` clip/keyframe model, sampled or authored at integer simulation ticks.

After:

> Export the action into the version-2 `animations.json` clip/keyframe model, sampled or authored at integer simulation ticks. A part keyframe may encode orientation as Euler XYZ degrees or a WXYZ quaternion according to `animations.md`; the shared loader converts either encoding to the normalized quaternion runtime representation.

## Validation and remaining work

- `git diff --check`: passed. Git reported expected LF-to-CRLF normalization notices for animation Markdown files.
- Confirmed the linked animation and Blender-authoring documents exist.
- Repository TODO scan found the existing dated note at
  `docs/specs/gamemodes/supersmashbrosmelee.md:1`: `2026 10 08 1228 jorj todo explainbetter`.
  It was left intact as historical user-authored material; a future cleanup
  could label and move it under a clearly named historical-notes heading.
- No tests or build were run because this task changed specifications only; no gameplay implementation was attempted.
- Code inspection recorded in `animations.md`: the current parser still reads the legacy Euler `rotation` array and current animation code interpolates Euler vectors. The desired quaternion runtime conversion and Melee behavior are not thereby implemented.
- Before implementation, verify Blender/game axis mapping against the normalized model frame, choose the exporter and tick-sampling rule, and resolve the open decisions listed above. Later acceptance requires the real runtime scenarios, live journal evidence, and human gameplay/visual review.
