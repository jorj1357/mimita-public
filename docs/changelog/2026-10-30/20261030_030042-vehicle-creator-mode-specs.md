# Vehicle, Creator, and Mode Specification Pass

- Timestamp: 2026-10-30T03:00:42Z (2026-10-29 23:00:42 EDT)
- Result: documentation update; implementation and runtime acceptance not
  performed.
- Commit: none.
- Scope: specifications and developer documentation only. No source code or
  active configuration was changed.

## User intent interpreted

- Keep the existing MiMITA physics world and improve it as a shared system.
  External rigid-body libraries are not part of the current plan; reconsider
  only after a measured missing capability and bounded comparison.
- Use per-mode vehicle controls. Keep the ordinary car profile manual by
  default and give skateboard/BMX a shared arcade profile with W/S drive and
  brake, A/D steer and lean, and charged Space jump.
- Build vehicle/game modes on shared physics: Skatepark, Need for Madness, and
  cops vs. robbers. Keep unsettled police win conditions as explicit decisions.
- Expand the in-game map editor around reusable Blender-exported objects,
  transforms, mass/material, collision, and shared attachments.
- Make destruction default on with a mode opt-out; preserve damage through
  Survive the Disasters rounds and restore the map between matches.
- Keep casual mode points local and unranked first: 100 points per survived
  disaster and 1,000 points for a full Zombie Tower completion. Future upgrade
  prices of 10/50 points remain examples until each catalog is designed.
- Keep replay, video, and future audio creation inside the MiMITA application.
  Preserve user-directed sharing, clear free-effect/cost information, and
  avoid default upload reminders or automatic posting.
- Treat cross-machine hour-long NPC replay accuracy below 0.00001 world units
  as a documented research goal, not current implementation work.

## Files created

- docs/specs/creator-tools/video-editor.md — in-app editor scope, project
  contents, timeline behavior, replay/media integration, performance,
  accessibility, and ClipForge audit/porting boundaries.
- docs/specs/gamemodes/need-for-madness.md — one-map arcade race slice,
  checkpoints/hoops, tricks-to-power, NPC drivers, and shared vehicle damage.
- devscripts/README.md — current script groupings and placement guidance;
  no scripts were moved.

## Files revised

- docs/ROUTER.md — added routes for vehicle controls, creator tools,
  distribution, speech, and shared volume/collision work.
- docs/architecture/vision-mentality/vision.md — added the one-application
  creator direction and no-pressure sharing principles.
- docs/architecture/accessibility/accessibility.md — added creator-tool
  input, selection, performance, and portability requirements.
- docs/architecture/collision/collision.md — documented reusable box, sphere,
  capsule, overlap, ray, and sweep query goals plus a shared soft-fit contact
  policy.
- docs/operations/asset-management/asset-management.md — defined a reusable
  object-library package under assets/objects/library/<asset-id>/.
- docs/specs/vehicles/vehicles.md — documented the in-house physics decision,
  manual default, mode-selected control profiles, trick-vehicle behavior, and
  a future Fixed/Weld constraint.
- docs/specs/vehicles/vehicles-skateboard.md — replaced the wishlist with
  skateboard/BMX controls, behavior goals, and acceptance.
- docs/specs/entity-editor/entity-editor.md — added asset placement,
  transform, physical-property, and constraint-authoring requirements.
- docs/specs/gamemodes/gamemodes.md — clarified the generic data-driven mode
  goal and added shared vehicle, destruction, and collision policy fields.
- docs/specs/gamemodes/skatepark.md — defined the low-pressure tuning and
  practice mode.
- docs/specs/gamemodes/survive-the-disasters.md — defined randomized rounds,
  timing targets, map reset, local points, and a staged mod-pack/Lua direction.
- docs/specs/gamemodes/vehicles/police-chase.md — clarified police/fugitive
  objectives and recorded undecided win/loadout rules.
- docs/specs/gamemodes/swat.md and docs/specs/gamemodes/bodycam.md — separated
  tactical match rules from reusable camera presentation and kept scenario
  choices open.
- docs/specs/gamemodes/zombie-tower/zombie-tower.md — added the future
  1,000-point local completion reward without changing the first-playable run.
- docs/specs/destructible-world/destructible-world.md — added mode inheritance,
  the desired default switch, map restoration ownership, and corrected the
  config path.
- docs/specs/data-saving/data-saving.md — added local, unverified,
  mode-specific points as a carve-out from account-level database statistics.
- docs/specs/replay/replay.md — organized recording, editing, fidelity,
  tool-assisted playback, export, and performance goals; kept prior reports
  explicitly unverified.
- docs/specs/distribution/distribution.md — replaced raw notes with
  local-first creation and user-directed sharing principles.
- docs/specs/live-voice-generation/live-voice-generation.md — specified
  editable in-game character TTS and optional above-head text.
- docs/specs/ingame-chat/voice-chat.md — separated player voice chat from
  generated dialogue and added safety/accessibility requirements.
- docs/specs/non-game-specs/DAW/mimita-daw.md — recorded integrated music
  creation and live collaboration as staged future work.

## Review findings and unresolved implementation gaps

- docs/specs/gamemodes/survive-the-disasters.md now describes the requested
  disaster-round redesign. config/gamemodes/survive_disasters.json still
  describes random weapons and last-actor-standing; no runtime/config change
  was made.
- The active config/destructible-world.json has cut, fracture, and physical
  settings but no world.enabled switch. The new desired default and
  features.destructible_world mode override are not implemented.
- The gamemode schema does not yet consume the newly documented
  vehicle.control_profile or shared actor-contact profile.
- The current ConstraintType source supports Point, Hinge, and Slider. The
  documented Fixed/Weld constraint remains a feature requirement, not working
  code.
- The editor does not yet provide the proposed asset library/assembly workflow.
  No asset package was created.
- ClipForge was not integrated. Its README and requirements were inspected, but
  the full folder was not audited or ported.
- Existing replay code is present, but export performance, visual fidelity,
  multiplayer fidelity, and first-time usability were not tested.
- The router references docs/doc-review-09-03-2026.md; that file is absent in
  this checkout.

## Documentation findings

- docs/specs/gamemodes/gamemodes.md:417 still has “Todo explain this 9 2
  2026 0848” under “Weapon set.” Suggested expansion: specify how a selected
  weapon set maps to hotbar slots and mode inventory.
- docs/specs/gamemodes/zombie-tower/zombie-tower.md:1637 still asks “todo
  explain better enshittification ?”. Suggested expansion: link the project
  principles and explain how rewards and upgrades avoid coercive engagement.
- docs/specs/gamemodes/zombie-tower/zombie-tower.md:4574 still says the replay
  section needs expansion. Suggested expansion: replace it with a short
  mode-specific list of entities/events that the shared replay contract must
  preserve.
- docs/specs/gamemodes/zombie-tower/zombie-tower.md:5775 still says “todo -
  explain zombie tower”. Suggested expansion: make it a concise mode summary
  or remove it if the structured first-playable contract supersedes it.
- docs/specs/moving-physical-objects/moving-physical-objects.md:18 still asks
  to explain the physical-object vision. Suggested expansion: summarize shared
  bodies, constraints, interactions, and how map-authored objects use them.

## Validation and acceptance

- Documentation checker and specification behavior review: performed for the
  edited documents. Product choices remain marked as open where the user had
  not settled them.
- Relative Markdown links in reviewed files: all resolved.
- git diff --check for the documentation paths in this work: passed; Git
  reported line-ending normalization warnings on several files. A whole-tree
  check also reports trailing whitespace in the unrelated pre-existing
  config/weapons/swordsword/config.json changes; those were left untouched.
- Build, tests, runtime, multiplayer, and human visual acceptance: not run;
  this was a documentation-only request.
- The worktree contained unrelated user changes before and during this task;
  they were preserved.
