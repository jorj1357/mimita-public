# Ground walking collision stability and contact diagnostics

- Recorded: `2026-10-24T17:52:54Z` (display: `2026-10-24 13:52:54 EDT`, `America/New_York`)
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7`
- Result: `INCOMPLETE_RUNTIME_VALIDATION`

## Request

Reduce ground-walk snagging at triangle seams and corners while retaining
triangle-versus-triangle actor collision. Keep collision effects visible, and
record which surface was contacted and where the player was in `events.jsonl`.
The requested runtime acceptance is a mode 1 Sandbox walk for 600 server
simulation ticks, with a nearly straight path and no unintended wall hits.

## Source changes

- `src/physics/movement/actor-triangle-solver.cpp` filters opposing contacts
  reported across one world face, collapses connected same-part edge contacts
  only when one surface does not block the intended move, keeps true corners,
  preserves triangle geometry during grounded response, and includes a bounded
  list of contact part/triangle/entity/position/distance/normal/penetration in
  the existing once-per-second important-level collision summary.
- `src/physics/movement/physics-collision.h` records whether the near-feet
  support recovery normal was reoriented toward the actor.
- `config/debuglogger.json` uses the original `important` collision level;
  per-tick trace output was disabled after it produced a 19 MB journal during
  the incomplete test run.
- `docs/architecture/collision/collision.md` documents the contact filters,
  grounded response, and bounded `collision.solve.summary` fields.

## Build and runtime evidence

- Dev-loop published build `3089` successfully from source generation `22`.
  Exact executable: `.dev/builds/3089/mimita.exe`; its `--versioninfo` output
  recorded `logs/10-24-2026/20261024_134954/events.jsonl`.
- The last completed valid mode 1, 600-server-tick walk was build `3081`.
  The path moved forward `71.152 m`, sideways `0.685 m` (about `0.963%` of
  forward distance), and vertically `-24.727 m`; one rollback occurred at a
  cliff edge. This does not meet the requested `0.1%` sideways target.
- The build `3087` walk did not complete: client events stopped at movement
  tick `1937` before the 600-tick end event. The solver's last recorded frame
  completed in under `1 ms`; dev-loop later found the client no longer ready.
- Build `3089` could not enter runtime validation. Dev-loop reported open
  Windows Firewall consent dialogs and did not respond to them automatically.
- The captured build `3038` journal contains one rollback with its leading map
  contacts at most `1.76 m` from the player origin. That evidence does not
  confirm a contact from across the map; the new bounded contact fields were
  not present in that older journal.

## Acceptance still required

Clear the Windows Firewall dialogs and rerun the exact build through dev-loop
in mode 1 Sandbox. Verify that the client is ready, then execute
`agent.move 1 0 600`; inspect `events.jsonl` for the start/end server ticks,
path deviation, and logged contact identities/positions. Repeat after any
further source change. Human confirmation that ground walking feels straight
and does not catch on seams remains open.

## Follow-up after firewall prompt was resolved

- Recorded: `2026-10-24T19:30:28Z` (display: `2026-10-24 15:30:28 EDT`, `America/New_York`)
- Result: `INCOMPLETE_RUNTIME_VALIDATION`

### Additional source and documentation changes

- `src/physics/movement/actor-triangle-solver.cpp:625-648`: before this follow-up, near-feet recovery only had the earlier feature-normal orientation handling. It now sets `supportNormal` from the map face, orients it toward the actor, then uses a vertical support direction before positional recovery. This retains triangle contact and penetration depth while avoiding sideways depenetration from rounded feature or cross-slope normals.
- `src/physics/movement/actor-triangle-solver.cpp:943-980`: before, the bounded summary did not include a bounded contact list or explicit solve correction vector. It now logs up to three part/triangle/entity contact records, world-space points, distance from player origin, depenetration/surface/response normals, penetration and TOI, plus player position and position correction.
- `docs/architecture/collision/collision.md:165-185`: before, the new support-normal behavior and bounded contact detail were not documented. It now records the behavior and event fields.

### Build evidence

- Dev-loop build `3090` succeeded from source generation `23`; exact executable `.dev/builds/3090/mimita.exe`.
- Dev-loop build `3091` succeeded from source generation `24`; exact executable `.dev/builds/3091/mimita.exe`. Its `--versioninfo` journal path was `logs/10-24-2026/20261024_152211/events.jsonl`.
- A subsequent dev-loop request reported build `3093` as `NOTHING_CHANGED`, same executable identity/hash as `3091`; build `3093` was not launched, so runtime results below are from `3091` only.

### Runtime evidence

- Build `3091` launched in mode 1 Sandbox, map `funworld3`, loopback; run `20261024_192228`, client PID `34712`, server PID `4808`, firewall status `FIREWALL_CLEAR`. Live journal: `logs/2026-10-24/20261024_192228/events.jsonl`.
- The queued `agent.move 1 0 600` completed 600 client movement-simulation ticks. Applied world input was approximately `(-0.000000044, -1)`. Start position was `[73.63928, 20.78121, 355.56213]`; end of the held move was `[75.36949, -48.70431, 329.17355]`. Delta was `+1.730 m` lateral, `-69.486 m` forward, and `-26.389 m` vertical, or `2.49%` lateral/forward. A later snapshot was grounded at `[75.37127, -48.95879, 327.86194]` after encountering a wall. This misses the requested `0.1%` target and is not a clean seam test: the route crosses a large height drop and reaches a real wall.
- The journal’s sampled collision contacts are nearby actor parts and world triangles (feet/body contacts around 1.6–2.0 m from the player origin, as expected for a body-origin measurement). No far-across-map contact was confirmed. No last-valid rollback occurred during the 600-tick segment; a later rollback occurred during teleport setup, outside that segment.
- A second attempt to measure a flat lane was invalid because the aiming setup introduced about 3% lateral input. A temporary mode-profile map change did not apply because the active dev-loop had cached `funworld3`; the profile was restored byte-for-byte. No clean flat-ground 600-tick acceptance run is available from this configuration.
- Local runtime now starts without a firewall dialog (`FIREWALL_CLEAR`). Mode 1 uses loopback. This does not require a firewall exception. Repeated prompts for public hosting can result from per-build executable paths; any persistent public-host rule should be narrowly scoped to the exact executable/profile/ports and created through an explicit admin-approved setup, not automated dialog dismissal.

### Acceptance status

Source and build evidence show the support recovery and collision-source diagnostics are present. Runtime evidence does not establish the requested near-perfect straight walk because the available map path drops substantially and hits a wall. The `99.9%` forward target, seam/corner feel, and absence of perceived snagging remain unaccepted and need a fresh straight, level lane through the live dev-loop. Preserve the earlier historical build notes above; the firewall blocker was resolved for this run.
