# Vehicle Spec Alignment and Map-Editor Vehicle Placement

- Timestamp: 2026-11-03T22:47:00Z
- Task: bring the vehicle direction into the specification, then implement the
  first local slice: author/place a vehicle in the map editor, spawn it, and
  keep its analytic wheels visible/editable.
- Status: spec updated; local placement/spawn/wheel-editing slice implemented;
  online authority, builder primitives, NPC driving, and arena remain pending.
- Commit: none.

## Pre-existing changes

- The worktree already contained many unrelated modified and untracked files.
  They were preserved and not claimed as this session's work.
- `docs/specs/vehicles/vehicles.md` already had a user journal at the top. It was
  kept verbatim as historical intake.
- No production source or configuration was edited directly.

## Requested behavior

- Make the online weaponized car the first playable direction.
- Reuse existing owners instead of creating a separate car world.
- Keep wheels on the analytic vehicle path, but make them visible and editable
  in the map editor.
- Edit a reusable vehicle template, not a one-off map object.
- Keep NPC driving after online authority.
- Keep Rocket League reference tuning inside a future arena spec.
- Accept driver-only first, with generic occupancy design.
- Implement the recommended phase 0 + phase 1 scope.

## Specification changes

- `docs/specs/vehicles/vehicles.md`
  - Added current-direction and first-playable acceptance sections.
  - Marked aircraft, bikes/skateboards, mounted weapons, arbitrary mesh editing,
    soft body, and advanced destruction as deferred.
  - Added `parts[]` and `attachments[]` to the template model and described
    box/slope/sphere/cylinder primitives, mass/COM contribution, render/collision
    split, and visual-only parts.
  - Stated that wheels stay on the analytic suspension path and are authoring
    attachments, not ordinary body parts.
  - Rewrote the seat/weapon contract around existing weapon muzzle/contact
    ownership.
  - Rewrote the networking contract to name server ownership, seat authority,
    movement bypass while seated, reliable membership events, unreliable state,
    and fail-closed missing templates.
  - Split map-editor placement from builder template editing and documented the
    new editor commands.
  - Updated the implementation phase order.
- New `docs/specs/gamemodes/vehicle-arena.md`: ball, goals, scoring, lifecycle,
  networking, reference-tuning direction, and acceptance.
- `docs/specs/vehicles/vehicles-skateboard.md`: marked deferred.
- `docs/specs/gamemodes/gamemodes.md`: linked Vehicle Arena and added its mode
  data ownership note.
- `docs/specs/moving-physical-objects/moving-physical-objects.md`: stated that
  vehicles and the arena ball use the shared physical owner.
- `docs/specs/entity-editor/entity-editor.md`: added map vehicle placement,
  template-builder ownership, wheel visibility/editing, and `vehicles` JSON.
- `docs/specs/weapons/weapons.md`: added seated weapon use through the normal
  muzzle and contact path; vehicle-mounted weapons deferred.
- `docs/specs/networking/networking.md`: added the vehicle authority and
  replication contract.
- `docs/ROUTER.md`: routed vehicle/arena work to the new arena spec.
- New `docs/features/vehicles/vehicles.md`: feature record, ownership, phases,
  implemented files, evidence, and known gaps.

## Code changes

- `src/gamemode/map-config.{h,cpp}`
  - Added `MapVehicleSpawn` (`id`, `template`, `position`, `yaw`, `enabled`).
  - Added map JSON `vehicles` parse/save, lookup, create/delete, and unique-id
    participation.
  - Extended the map-config selftest to round-trip a vehicle placement.
- `src/mapeditor/map-editor.{h,cpp}`
  - Added `EditorObjectKind::Vehicle`.
  - Added select/move/delete/cut/copy/paste/duplicate support.
  - Added outliner rows, picking, world wire body, and analytic wheel display.
  - Added properties details for template, wheel count, and seat count.
  - Added `addVehicle`, `spawnSelectedVehicle`, and wheel hub/radius/width
    editing with owner-level structured events.
- `src/mapeditor/map-editor-commands.cpp`
  - Added `mapeditor add vehicle [template] [id]`,
    `mapeditor spawn_vehicle [id]`, and
    `mapeditor vehicle_wheel <index> hub|radius|width ...`.
- `src/vehicles/vehicle-config.{h,cpp}`
  - Added mutable template lookup and `saveTemplate`.
  - `saveTemplate` re-reads the existing JSON so unknown sections survive the
    write; comments are not preserved by the JSON writer.
  - Added `vehicle.config.saved` structured logging.
  - Serializes wheels, tire, engine, gearbox, differential, steering, brakes,
    seats, and existing chassis fields.

## Diagnostics

- `map-vehicle.created` at `MapConfigRegistry::createVehicle`.
- `map-editor.vehicle-wheel` for wheel hub/radius/width edits.
- `vehicle.config.saved` for template persistence.
- Existing `vehicle.spawned`, `vehicle.state`, `vehicle.entered`,
  `vehicle.tick.summary`, and `map-editor.autosaved` remain the runtime proof.

## Validation

### Build and focused selftests

- `python devscripts/dev-loop.py --request build --build-mode debug --wait-for-build`
  -> build success (early build).
- Latest dev-loop build after final edits:
  `2026-11-03T22:45:14.448902Z` -> success.
- `mimita.exe --map-config-selftest` -> PASS.
- `mimita.exe --map-entity-selftest` -> PASS.

### Runtime evidence

Run `logs/2026-11-03/20261103_223514/events.jsonl` on the real dev-loop client:

- `map-vehicle.created` with `map=funworld3`, `template=car_sedan`,
  `vehicle_id=vtest`, position and yaw.
- `map-editor.autosaved` wrote `config/maps/funworld3.json`.
- `vehicle_list` produced `vehicle.state` with four `grounded=true` wheels,
  suspension compressions/loads, and `localDriver=false`.
- This proved authored placement -> spawn -> runtime vehicle on the real client.

Wheel editing proof:

- Created a temporary `config/vehicles/car_wheeltest.json` copy.
- `vehicle_reload` -> `mapeditor add vehicle car_wheeltest vwheel` ->
  `mapeditor vehicle_wheel 0 radius 0.42`.
- The temp template was written with wheel 0 radius `0.41999998688697815`.
- The temp template and the funworld3 autosave were removed/reverted after the
  test.

Existing `devscripts/scenarios/vehicle_drive_v1.json`:

- Most assertions passed: spawn, enter, exit, void, local-driver tick summary,
  distance from spawn, mass, engine-on, and tick-summary count.
- The run was interrupted by the dev-loop process-count audit
  (`max_mimita_processes=1`) killing the scenario server/client; the final
  teleport timed out and driver-void assertions did not complete.
- This is recorded as an environment/dev-loop failure, not a passing scenario.

### Agent gate

`python devscripts/quality/agent-gate.py` -> RED.

- Build: PASS.
- Grenade, area-effect, team-brain, objective, NPC navigation, collision, and
  canonical-contact selftests: PASS.
- Weapon and gamemode JSON sync: PASS.
- Lint: FAIL because `duplicate-report.repeated_windows` is `554 > baseline
  551`. The reported windows are in pre-existing network/combat/physics files,
  not in this vehicle change. The baseline was not edited.
- Fast runtime scenarios failed with dev-loop delivery/connection timeouts
  (`grenade_tick_contract_v1`, `npc_wall_containment_v1`,
  `scenario_record_roundtrip_v1`) while the worktree already had stale
  dev-loop processes. They are not claimed as vehicle behavior failures.
- Scenario recording roundtrip check: FAIL for the same no-connection cause.

## Regression review

- No confirmed regression caused by this change.
- The `vehicle_drive_v1` failure is attributed to the dev-loop process conflict
  (delivery timeout after its own processes were killed), not to vehicle physics
  or placement.
- No regression entry appended.

## Human acceptance still required

- Visual inspection of the authored vehicle wire body and wheel cylinders in
  the map editor.
- Human playtest of the map-editor add -> spawn -> F enter -> drive loop on a
  flat area.
- Visual confirmation that wheel radius/width edits change the spawned car.
- Online authority, seated weapon replication, builder primitives, NPC driving,
  and Vehicle Arena are not implemented in this slice.

## Relevant spec TODOs found (not edited)

- `docs/specs/vehicles/vehicles.md:458` (pre-edit numbering): leaving-car
  animation TODO; specify fallback and acceptance.
- `docs/specs/vehicles/vehicles.md:600`: open TODOs; convert to decision log.
- `docs/specs/moving-physical-objects/moving-physical-objects.md:18`: explain
  moving objects; add a vehicle/ball summary.
- `docs/specs/moving-physical-objects/moving-physical-objects.md:31`: map editor
  question; answer placement yes, physics ownership no.
- `docs/specs/entity-editor/entity-editor.md:4`, `:31`, `:36`: spawn
  authoring/path TODOs; align with the shared spawn policy.
- `docs/specs/gamemodes/gamemodes.md:440`: weapon-set explanation TODO; separate
  pass.
- `docs/specs/networking/networking.md:280`: future tick-rate/serverless note;
  out of scope.

## Next step

Phase 3 of the revised plan: server authority, seat occupancy, vehicle input,
seated weapons, and multi-client replication. Phase 4 then adds the primitive
builder; Phase 5 NPC driving; Phase 6 Vehicle Arena.
