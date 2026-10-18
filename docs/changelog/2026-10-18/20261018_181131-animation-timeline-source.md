# Animation timeline as the timing source

- Local time: 2026-10-18 14:11:31 EDT (America/New_York).
- UTC: 2026-10-18T18:11:31Z.
- Branch: `20261011zt`.
- HEAD before this documentation change: `f2e91866`.
- Result: `PASS_WITH_HUMAN_REVIEW`.

## Request and decision

Update the animation/Blender/Melee/weapon specifications so one authored
animation controls total action length and windup/active/recovery windows. The
action in `weapons.json` references one clip but cannot override timing. The
broader goal is a Blender authoring/import path into one expandable animation
format shared by game consumers.

The specs now define integer clip markers, half-open phase intervals, and an
exclusive end marker equal to `durationTicks`. Example: duration 45 and marker
ticks 0/10/25/45 produce 10 windup, 15 active, and 20 recovery ticks; poses are
sampled at ticks 0–44. Weapon JSON retains input/combat/hitbox/sound values but
cannot duplicate duration, phase lengths, or per-phase clip IDs.

## Findings and implementation boundary

- The prior animation spec assigned phase durations to combat action data and
  normalized separate phase clips to those durations. The prior weapon and
  Melee specs also placed phase tick values in `weapons.json`. This conflicted
  with the user's requested single source of truth.
- Source review confirms current code is not migrated: the weapon melee
  configuration parses phase `ticks` and `clipId`; the client retimes phase
  clips using phase duration; server action state uses configured phase
  durations. The authored-marker contract in this changelog is a desired spec,
  not an implementation claim.
- No Blender exporter was found. The Blender document now marks exporter and
  shared server/client timeline integration unimplemented and proposes a
  validated per-action file workflow under `config/animations/...` using the
  versioned shared clip format.
- Open decisions remain for asset discovery/manifest, Blender basis/pivot and
  time conversion details, and whether a running action pins its clip revision
  or adopts a live edit. The latter must be consistent on server and clients.

## Files changed

1. `docs/specs/animations/animations.md`, lines 242–289, 313–321, 333–351.
   - Before (line 283): “Combat action data owns the server-tick duration of
     each phase.”
   - After (line 335): “One action references one authored clip. Its
     `durationTicks` and phase markers in that clip define the complete action
     duration and the exact server-tick windup, active, and recovery windows.”
   - Added general clip markers, phase boundary validation, replicated clip
     revision requirement, live-edit open decision, and fixed-tick playback.

2. `docs/specs/animations/blender-animation-authoring.md`, lines 1–181.
   - Before (line 1): “# Blender Animation Authoring and Export” and a short
     proposal focused on exporting existing animation clips.
   - After (line 1): “# Blender Animation Authoring and Import”. The document
     now defines the user goal, one-file-per-action proposal, Blender rig part
     mapping, complete body-part poses, 60 Hz sampling/resampling, marker
     export, transform/basis constraints, versioned shared format example,
     proposal/open decisions, and future acceptance criteria.

3. `docs/specs/weapons/weapons.md`, lines 291–300, 320–325, 362–374.
   - Before (line 295): “recovery tick counts, per-phase animation/pose IDs” in
     the `weapons.json` ownership list.
   - After (line 295): “one stable animation clip ID”. The text now states
     animation asset timing is authoritative, shows `animation_clip_id`, and
     removes phase timing/clip fields from the illustrative weapon shape.

4. `docs/specs/gamemodes/supersmashbrosmelee.md`, lines 121–135, 253–263,
   291, 331–356, 382–394, 484–490, 623–629, 697–705.
   - Before (line 255): “Animation duration need not equal action duration,
     but the relationship must be explicit and must not affect combat timing.”
   - After (line 255): “The clip's `durationTicks` and `phase.*` markers define
     its total duration and windup/active/recovery windows with half-open
     intervals.”
   - Aligned move roster/configuration requirements, replication identity and
     revision, active hitbox timing, phase-tuning register, authored pose
     behavior, and live-edit wording.

5. `docs/changelog/2026-10-18/20261018_181131-animation-timeline-source.md`.
   - New file; there was no prior text. It records the exact request, decision,
     source/spec disagreement, remaining open items, validation, and review
     status.

## Instructions and focused review

- Followed `AGENTS.md` and `docs/ROUTER.md`; consulted the routed animation,
  Blender authoring, weapons, Melee, networking, and task completion material.
- Focused skill: `docs/skills/spec-behavior-review-v1.md` — `PASS_WITH_HUMAN_REVIEW`.
  Desired behavior is now explicit and the code/spec mismatch is recorded.
  Runtime migration and human gameplay/visual review remain outstanding.
- Time naming standard: `docs/architecture/time-and-formatting/time-and-formatting.md`.
- No skills or memory files were modified.

## Validation and review still needed

- `git diff --check` passed for the documentation edits. No tests or build were
  run because this session changed specifications only.
- Source evidence is from code/config inspection, not runtime testing. The game
  still follows the old `weapons.json` phase-duration path until implementation
  is migrated. Do not claim animation-owned active windows work in a running
  game yet.
- Human review is still needed for the marker schema, asset discovery shape,
  live-edit revision policy, and Blender basis/time mapping before exporter and
  runtime implementation are finalized.
- Pre-existing unrelated dirty files were present and left untouched, including
  Captain Falcon animation/config data and gameplay source/config edits.
