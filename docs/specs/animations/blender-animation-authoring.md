# Blender Animation Authoring and Import

Status: **SPECIFICATION — standalone interactive exporter script exists;
Blender execution and runtime acceptance are pending.** The exporter writes the
shared version-2 clip shape. Server/client use of phase markers for gameplay
timing is still unimplemented.

## Goal and ownership

Blender is the authoring tool for poses and clip-local timing. MiMITA imports
those authored clips into the versioned, general animation format defined in
[`animations.md`](animations.md). The game keeps one shared animation format
and playback owner for players, NPCs, and future animatable objects. Consumers
may attach domain meaning to markers, but they do not create parallel clip
formats or duplicate the clip's timing.

For a melee move, the animation asset is the single source of truth for total
duration and windup/active/recovery boundaries. `config/weapons.json` references
the clip and owns combat values (inputs, hitbox shape/size/offset, damage,
knockback, movement impulses, and sound recipe). It must not repeat clip
duration, phase lengths, or per-phase clip IDs. Editing the action's timeline
in Blender and importing it changes both the pose sequence and the ticks in
which the server/client consider the move active.

## Audience and scope boundary

Audience: Blender animators, MiMITA animation/runtime implementers, and people
reviewing exported assets. This document owns the proposed exporter workflow
and Blender-to-file contract. `animations.md` owns the runtime clip schema and
playback; `avatar-editor.md` owns the rig, normalization, and pivot contract.

The standalone interactive exporter is
[`../../../tools/mimita_animation_exporter.py`](../../../tools/mimita_animation_exporter.py).
Run it from a terminal with `py tools/mimita_animation_exporter.py`. It asks
for the `.blend` file, then launches Blender in background mode and prompts for
the rig, Action for each body part,
clip ID/type, pose export mode, GLB rest reference, output path, and optional
avatar `animations.json` profile. The final report prints the exact exported
path. Alternatively, run it directly with Blender's command line and an open
`.blend` file; the same export questions are asked in the terminal. It is not
installed as a Blender add-on and adds no Blender UI panel.

The default Blender executable is
`C:\Program Files\Blender Foundation\Blender 5.0\blender.exe`. If that file is
missing, the script asks for the executable path and shows an example. Every
interactive prompt includes a short
explanation and a concrete example. The selected current rig and part Actions
are defaults when available.

The script accepts canonical part names and Blender's three-digit duplicate
suffixes (for example `head.026`). `Authored keyframes` is the default: it writes
the union of integral transform-key frames from selected Actions and evaluates
all six parts at those frames; MiMITA smoothly interpolates between them.
`Dense every tick` is opt-in and writes Blender's evaluated pose for each 60 Hz
tick. Separate Actions can be selected for individual parts, or `Rest pose` can
be selected for unchanged parts. The exporter rejects missing GLB parts and
invalid/missing Melee markers.

This specification does not claim server/client timeline integration exists.
It does not add another animation format, animator, or
per-actor converter. Skeletal skinning, automatic retargeting, and gameplay
root motion are outside the first supported slice. Blender pose data does not
decide whether a gameplay hit is accepted.

## Proposed asset organization

Use one human-editable file per authored action, grouped by reusable category,
under the shared clip library, for example:

```text
config/animations/library/melee/captainfalcon/punch_a.json
config/animations/library/melee/captainfalcon/falcon_punch.json
config/animations/library/locomotion/run.json
```

Each file uses the general version-2 clip schema; it is not a Melee-only data
shape. This follows the existing shared clip library under
`config/animations/library/`; each file contains the standard `version` and
`clips` map. An avatar profile includes relevant files (or a directory
supported by the current include rules). A future registry/manifest may make
all clips discoverable to server action definitions and other shared consumers.
Weapon actions reference a stable clip ID, not a copied timeline. The shared
server/client registry remains **OPEN** and must be reconciled with current
avatar profile loading before implementation. The standard melee path includes
the avatar/actor identifier so clips stay grouped without changing the shared
clip schema.

An action file contains its stable clip ID, duration in fixed simulation
ticks, all keyed body-part transforms, interpolation, and named timeline
markers. Melee action clips require `phase.windup`, `phase.active`,
`phase.recovery`, and exclusive `action.end` markers, as defined in
[`animations.md`](animations.md). Markers and poses are exported together, so
phase timing cannot drift from the visual action. Other consumers can use
generic event/state markers without changing the base keyframe format.

## Tool shape and user workflow

**Chosen implementation: a standalone interactive Python script.** Blender is
still required to open the `.blend` file and evaluate its Actions, but the user
runs a script rather than installing an add-on. A normal Python launch uses the
default Blender executable path documented above, asks for its path only if
that file is missing, then asks for the source `.blend`. It runs Blender in
background mode, prompts for the export choices, and prints the destination
path.

The script's prompts cover:

| Control | Proposed behavior |
|---|---|
| Rig root | List and select `plrOrigin` or `plrOrigin.<digits>`. |
| Rest reference | Default to `assets/entity/player/default/mimita-char-no-animations-v4.glb`; prompt for the path and validate all six matched parts. |
| Actions | Choose the Blender Action for each limb; allow `Rest pose` for unchanged parts. |
| Part mapping | Match canonical part names, including Blender's `.NNN` duplicate suffix; reject duplicates/missing names. |
| Clip ID | Enter or derive a stable ID such as `punch_a`; validate uniqueness within the library. |
| Clip type | Select generic clip or Melee action clip. Melee requires ordered windup/active/recovery/end markers. |
| Pose export mode | Choose `Authored keyframes` (default, sparse JSON with runtime interpolation) or `Dense every tick` (opt-in evaluated pose at every tick). |
| Timing | Require 60 Blender frames per second for 1 frame = 1 game tick; report frame range, mapped ticks, duration, and marker ticks. |
| Output | Choose the destination under the shared library; print the full path after export. |
| Avatar profile | Optionally choose an avatar `animations.json` to add the clip to its `include` list. |
| Export scope | Export one combined clip from the selected per-part Actions. Batch export is a later convenience. |

For example, `head.026Action` is the Blender Action attached to the head
object; it is only the pose source. The **Clip ID** is the name MiMITA looks up
in the exported JSON. For a punch, enter `punch_a`; it does not need to equal
`head.026Action`. Choose **Melee** for an attack with phase markers, or
**Generic** for an idle/walk clip. Choose **Authored keyframes** to keep only
the keyed poses (for example frames 1, 15, 30, and 45); MiMITA smoothly
interpolates between those keys during playback. Choose **Dense every tick** to
store Blender's evaluated in-between poses explicitly.

A simplified prompt example:

```text
head Action: choose 2, head.026Action [current]
Clip ID [new_animation]: punch_a
Clip type: choose 2, Melee
Pose export mode: choose 1, Authored keyframes
Output JSON path [config/animations/library/melee/captainfalcon/punch_a.json]: [press Enter]
```

The script then reports the total ticks and the exact path it wrote.

Workflow: run the script, choose a rig and each part's Action, inspect the
reference GLB, markers, frame range, pose export mode, and tick conversion;
validate; write one clip JSON; show a success report with the destination and
summary. Errors identify the object,
Action, frame/tick, marker, or transform that failed and leave the existing
destination unchanged. The user can cancel before writing. The tool must not
alter Blender object transforms, rename objects, apply transforms, change the
current Action, or save the `.blend` as a side effect.

### Rig selection and `plrOrigin` space

The first supported slice follows
[`../avatar/avatar-editor.md`](../avatar/avatar-editor.md): one `plrOrigin`
root with the six canonical direct children `head`, `torso`, `leftArm`,
`rightArm`, `leftLeg`, and `rightLeg`. Blender's three-digit duplicate suffix
is accepted on the root and parts when each canonical part remains unique.
The exporter must not infer left/right from screen position, mesh shape, or
color. Other actors may later provide different named parts through the same
generic animation format.

The exporter uses `plrOrigin` to remove actor/world placement while calculating
each sample, but the JSON does **not** contain absolute part transforms relative
to `plrOrigin`. It stores each part's translation and rotation offset from its
normalized GLB rest transform. The runtime applies those offsets to the
normalized rest pose. `plrOrigin` itself is not exported as limb translation or
silently baked into every limb. Gameplay movement continues to move the actor
root; generic root motion needs its own explicit track/decision.

**DECIDED for this avatar:** the rest-pose reference is
`assets/entity/player/default/mimita-char-no-animations-v4.glb`, which contains
the character without animation. The script must resolve this file, match its
canonical rig parts, and use the same normalization/pivot contract as MiMITA.
A Blender reference Action/frame may help align the authored scene to the GLB,
but it does not replace the GLB as the base pose. Export must stop if the
reference asset or part mapping cannot be resolved. All samples use this same
reference, so actor placement is not baked into animation offsets.

Conceptually derive `root_relative_part = inverse(root_world) * part_world`,
then derive the animated local delta against the normalized GLB rest transform.
The canonical frame is Z-up/down, +X forward, and +Y lateral, as specified by
the avatar contract. The first exporter computes `poseDelta =
inverse(restRootRelative) * animatedRootRelative` and serializes translation and
WXYZ rotation. This matches the runtime's `rest * pose` composition when the
Blender rig's part origins correspond to the normalized GLB pivots. The script
does not reproduce geometry-based avatar normalization; align mismatched rig
pivots before export and compare poses in Blender and MiMITA before acceptance.
Absolute transforms expressed directly in `plrOrigin` space are a possible
future format, but are not the current export contract.

## Proposed Blender workflow

1. Animate a named Blender Action on the supported rigid multipart actor rig.
   The first supported slice is object transforms for the established parts;
   skeletal skinning and automatic retargeting are out of scope until selected.
2. Use a clear mapping from Blender objects to canonical runtime part names.
   The screenshot's numbered suffixes illustrate Blender duplicate object
   names. Match canonical names with optional `.NNN` suffixes and reject
   ambiguous/missing parts.
3. Author all required part transforms per pose. The first avatar export
   requires position/translation and rotation for head, torso, both arms, and
   both legs at every exported sample. If a part is intentionally unchanged,
   export its unchanged local transform explicitly so each sample is a
   complete pose. Scale is optional and defaults to one unless the rig contract
   requires it.
4. Add named Blender timeline markers for phase boundaries and events. A Melee
   action uses `phase.windup`, `phase.active`, `phase.recovery`, and
   `action.end`. The script validates marker order and range before writing.
5. Author on a 60 FPS timeline. The first sampled Action frame maps to game
   tick 0, so frames 1–60 map to ticks 0–59. Authored-keyframe mode writes the
   union of keyed integral frames; dense mode writes one evaluated pose per tick.
6. Export to a temporary file, validate the full document, then atomically
   replace the destination only on success. Produce a human-readable report
   with Action name, target path/ID, frame-to-tick mapping, part mapping,
   marker list, duration, basis conversion, and warnings.
7. Preview the imported asset through the shared animation runtime. A valid
   export should be usable by both player and NPC paths without bespoke
   per-actor conversion.

## Time and transform conversion

MiMITA gameplay uses integer 60 Hz simulation ticks. In the authoring workflow,
require the Blender scene to use 60 FPS so each displayed frame is one game
tick. The first Action sample frame maps to game tick 0, independent of the
absolute Blender frame number; e.g. frames 1–60 map to ticks 0–59. Thus a
60-tick clip has 60 sampled poses and the exclusive `action.end` marker at tick
60 (one frame after the final sampled frame). The script reports the mapping;
it never copies Blender's 1-based frame number directly as a game tick. A non-60
FPS scene must produce a clear warning and
must not silently change animation speed. Supporting deterministic resampling
from other frame rates is an **OPEN** extension, not first-version behavior.

**Pose export modes:**

- `Authored keyframes` (**default**): evaluate the whole rig at the union of
  integral transform-key frames across selected Actions and write only those
  poses. Store smooth interpolation as each key's default curve; game runtime
  samples between them without Blender. The clip still records its full
  `durationTicks`, including unkeyed intervals.
- `Dense every tick` (opt-in): evaluate Blender's pose at every 60 Hz frame and
  write each result. This captures Blender's evaluated in-betweens explicitly.
  It is useful when exact per-tick poses are required or to compare runtime
  interpolation with Blender.

Both modes write `durationTicks` equal to the number of sampled frames, with
keyframe ticks `0` through `durationTicks - 1`. Marker frames use the same
frame-to-tick convention; the final exclusive end marker may be at
`durationTicks`.

Blender transforms must become part-local offsets relative to the normalized
rest pose and canonical runtime pivots. Both Blender and MiMITA use Z-up; the
canonical game frame uses +X forward and +Y lateral. Apply the agreed
normalization/pivot conversion once and report it; do not add a guessed root
rotation. Euler XYZ and WXYZ quaternion are both allowed by the shared format;
quaternions use explicit WXYZ order. The exact normalized matrix/pivot delta,
unit handling, parent-transform extraction, and Euler conversion must be
verified against an in-game reference pose before exporter acceptance.

The actor root remains the authoritative movement origin. Limb animation does
not silently move the actor root; gameplay root movement/impulses stay with the
movement/action owner. A future generic root-motion track requires a separate
decision and must not undermine collision authority.

## Proposed shared format example

The following is illustrative version-2 data, not a finalized parser contract:

```json
{
  "version": 2,
  "clips": {
    "punch_a": {
      "durationTicks": 45,
      "loop": false,
      "markers": [
        { "name": "phase.windup", "tick": 0 },
        { "name": "phase.active", "tick": 10 },
        { "name": "phase.recovery", "tick": 25 },
        { "name": "action.end", "tick": 45 }
      ],
      "keyframes": [
        { "tick": 0, "interp": "linear", "parts": {
          "head": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] },
          "torso": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] },
          "leftArm": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] },
          "rightArm": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] },
          "leftLeg": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] },
          "rightLeg": { "translation": [0, 0, 0], "rotationQuaternionWxyz": [1, 0, 0, 0] }
        } }
      ]
    }
  }
}
```

Placeholder transforms above demonstrate shape only and are not usable poses.
Clip `durationTicks` is the single total-length value; no action metadata may
duplicate it. The schema can grow with versioned optional fields and generic
markers while preserving one runtime-readable format.

## Full first-version specification

### Inputs

- An open `.blend` scene (saved or unsaved).
- One selected rig root and its direct canonical part objects.
- One explicitly selected Blender Action.
- The unanimated reference GLB at
  `assets/entity/player/default/mimita-char-no-animations-v4.glb`.
- A clip ID, clip category, named timeline markers, pose export mode
  (`Authored keyframes` or `Dense every tick`), and destination under the shared
  animation library.
- Optional generic event markers; Melee clips require the four phase/end
  markers.

The preview must show these choices before export. Safe defaults may be used
where the project has decided them: 60 Hz target, shared animation JSON,
root-relative local poses, and gameplay-owned actor root. The exporter must not
silently choose an unresolved basis or rest pose.

### Outputs

- One versioned JSON file with the common `clips` structure and one stable clip
  ID in the first supported slice.
- `durationTicks`, playback fields from the shared format, optional generic
  markers, required Melee markers when selected, and either sparse authored
  keyframes or a dense keyframe for every tick from zero through
  `durationTicks - 1`.
- Every exported sample contains complete local translation and rotation for
  every mapped part. Scale is included only if supported and enabled; otherwise
  it is identity.
- A visible report listing the source Action, root, part map, rest reference,
  source frame range/rate, target rate, tick count, marker frame/tick mapping,
  conversion basis, output path, and validation result.

The report is not a second source of truth. Timing and poses live in the
animation JSON. `weapons.json` references the clip ID and stores non-timing
combat data.

### Validation and failure behavior

- Require the selected Action to animate the chosen rig or clearly report
  which required parts have no authored channels. Unchanged parts may be
  emitted with their reference pose only when the user confirms.
- Validate names/mappings, direct hierarchy, finite transforms, supported
  object types, valid rotations, duration, increasing unique ticks, marker
  ordering/range, complete part samples, 60 FPS scene rate, reference GLB
  availability, and schema version.
- Reject zero-tick clips, missing/out-of-order/overlapping Melee markers, empty
  phases, or an `action.end` that is not the exclusive `durationTicks` endpoint.
- Emit finite JSON numbers with explicit component order and units; never emit
  Blender object references or Blender-only runtime data.
- Validate the complete serialized document before replacing the destination.
  Keep the old file intact after any validation or filesystem failure.
- Separate warnings (for example, unusual source FPS) from fatal errors and
  show the exact failing object/frame/tick/marker and reason.

### Acceptance sequence

1. Run the standalone script with a sample `.blend` containing the avatar
   hierarchy; answer the rig, per-part Action, sampling, marker, and output
   prompts.
2. Inspect the exported path and report, then validate against a temporary
   test directory.
3. Validate IDs, JSON shape, part completeness, frame/tick mapping, markers,
   transforms, and atomic replacement behavior.
4. Load the clip through MiMITA's shared library and compare Blender/runtime
   poses at tick zero, marker boundaries, and the final sampled tick. Confirm
   actor world placement was not baked into limb offsets.
5. Connect one move to the clip and verify server/client active tick agreement
   using the same clip revision.
6. Expand to other rigs, batch export, and CLI automation only after the
   six-part flow works reliably.

## Decisions and open items

**DECIDED direction:** Blender-authored clips import into the general
versioned animation format; action clips carry their own duration and semantic
phase boundaries; gameplay uses those same fixed-tick boundaries; weapon JSON
references the clip and retains non-timing combat configuration; local part
transforms, not client-only root movement, drive the visible rig and attached
limb hitboxes.

**DECIDED:** standalone interactive Python script (using Blender to evaluate
the scene); map the rig parts using the prompt workflow; use the no-animation
player GLB as the rest-pose reference; author phase markers on the Blender
timeline; export authored keyframes only by default and let the game interpolate
between them; offer dense per-tick evaluated poses as an opt-in export mode.

**PROPOSED:** one file per action under
`config/animations/library/<category>/<avatar>/<clip>.json` (for example
`melee/captainfalcon/punch_a.json`); a reusable Python export core; one selected
Action per part in v1; atomic validation/write; shared registry available to
player, NPC, server actions, and future object consumers. `Dense every tick` is
the opt-in mode and stores Blender's evaluated pose on each 60 Hz tick.

**OPEN:** runtime normalization reuse; shared server/client clip discovery;
server/client use of phase markers for combat timing; Blender-version support;
visual parity across supported pivots; non-60 FPS resampling; whether live edits
replace a running action's revision or apply on its next start; scale policy;
future armature/skinning and root-motion support. A graphical Blender add-on,
batch export, and no-Blender retargeting remain possible later extensions.

## Acceptance outline

- Export one idle and one attack action for an existing rig, with an auditable
  mapping for every required part and marker.
- Confirm a 60-frame, 60 fps source maps deterministically to ticks 0–59 and a
  duration of 60, with endpoint marker at 60.
- Confirm duration and all phase boundaries come only from the exported clip;
  deliberately conflicting values cannot be supplied by `weapons.json`.
- Confirm server and two clients resolve the same clip revision and active
  interval, including a real swept-limb contact scenario.
- Compare Euler and quaternion exports of the same pose in-game and verify the
  reported basis/pivot conversion.
- Confirm invalid or incomplete exports leave the previous valid asset intact
  and report the failure clearly.

The Captain Falcon `punch_a` clip was exported from `mimita sizing 6.blend` as
a 60-tick, two-keyframe JSON and included in the Captain Falcon animation
profile. That source file currently has no phase markers, so the exported clip
has no authoritative melee timeline. Complete server/client timeline
integration is also not implemented.
