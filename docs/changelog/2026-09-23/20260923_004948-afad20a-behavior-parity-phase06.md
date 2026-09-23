# afad20a behavior parity — Phase 6 (shared selector + C++/JSON parity)

Date: 2026-09-23
Status: hot-code built; deterministic parity tests pass (0 mismatches); human
live acceptance pending

Related specification: `docs/specs/movement/movement.md`,
`docs/architecture/collision/collision.md`,
`docs/features/live-code-development/live-code-development.md`

Predecessors:
`docs/changelog/2026-09-22/20260922_235824-afad20a-movement-phase01.md`,
`docs/changelog/2026-09-23/20260923_001103-afad20a-collision-phase04.md`,
`docs/changelog/2026-09-23/20260923_001934-afad20a-animation-phase05.md`

## Scope of this session

Phase 6: give the C++/JSON behavior selector one owner and add a fixed-tick
parity comparison for movement, collision, and animation, so switching the
active source is evidence-based. The C++ implementation remains the compiled
fallback and rollback path; the JSON source is active for all three domains.

## Work performed

Shared selector owner:

- Added `src/hot-reload/hot-behavior-source.h`: one `MimitaBehavior::Source`
  vocabulary (`cpp` / `json`) and `movementSource()`, `collisionSource()`,
  `animationSource()` readers, each parsing the owning config's
  `behaviorSource` key. Movement (`hot-movement-presets.h`) and collision
  (`collision-package-solver.cpp`) now resolve through this owner instead of
  each parsing the string themselves.

Fixed-tick parity harness:

- Added `src/hot-reload/hot-behavior-parity.h` and
  `src/hot-reload/modules/behavior-parity-selftest.cpp`:
  - Movement: compares the compiled preset tuning against the JSON preset
    field-by-field, then runs a 60-tick collision-free movement trace through
    the shared hot functions with each tuning and compares final root
    position/velocity.
  - Collision: compares the compiled fallback `CollisionBehaviorV1` against the
    JSON-loaded response/ground/bounce policy.
  - Animation: compares `afad20aSampleClip` (C++) against `sampleClip` (JSON)
    for idle/walk/return_to_idle at fixed times.
  - Report-only: divergences are printed and summarized; a candidate is not
    rejected for known tuning differences, but non-finite output fails.
- Wired into the DLL candidate self-test (`src/effects/effect-part.cpp`) so the
  comparison runs before activation; the summary is included in the self-test
  message and printed as `[PARITY] ...` lines.
- Registered the two new headers in `src/hot-reload/hot-modules.json` for
  change detection.

Parity fixes surfaced by the harness:

- `src/hot-reload/hot-movement-presets.h`: aligned the compiled `source` preset
  to `config/movement/movement-source.json` (the afad20a source values):
  `walkMode` source, ground acceleration 20, friction 3.25, stopspeed 1.0, air
  acceleration 12, air max wishspeed 2, air speed gain 2, gravity 40, jump
  15.1, max fall 175, jump buffer 0.2, coyote 0, dash impulse 10/10, down-dash
  -50, speed limit enabled/fixed 50. This is the C++ rollback path **and** the
  tuning NPCs use directly (`actor-movement-system.cpp` reads
  `getMovementPreset(...).tuning`), so it also removes the NPC/player tuning
  split.
- `src/hot-reload/hot-animation-clips.h`: the JSON `sampleClip` one-shot clamp
  now clamps to `durationTicks - 1` like the afad20a C++ sampler, removing an
  animation parity divergence on one-shot clips.

## Evidence

Source evidence:

- Selectors: `config/movement.json`, `config/collision.json`,
  `config/animations.json` each carry `behaviorSource`; movement additionally
  carries `preset`, animation additionally carries `locomotionSource`.
- Parity harness: `src/hot-reload/modules/behavior-parity-selftest.cpp`.

Build evidence (separate from runtime evidence):

- Hot DLL: `python build_game_dll.py` -> `DLL build success`
  (`build/mimita-game.dll`, 82 sources). No cold rebuild was required; only hot
  headers, a hot module, a manifest entry, and config were touched.

Test evidence (deterministic, headless):

- `--live-code-selftest`: PASS. The candidate self-test reports
  `[PARITY] sources movement=json collision=json animation=json mismatches=0
  maxDev=0.0000`.
- `--afad20a-reference-selftest`, `--afad20a-parity-selftest`,
  `--movement-selftest`, `--movement-parity-selftest`, `--collision-selftest`:
  PASS.

Runtime evidence:

- Not performed. No running game session was driven and no in-game trace was
  captured in this session.

Human acceptance:

- Pending. The NPC/player tuning change (source preset alignment) and the
  one-tick one-shot clamp change still require a human playtest.

## Limits

- The parity harness is report-only, not a hard activation gate. Known tuning
  differences in the non-active presets (default/heavy/retrograd) are reported
  by the existing `movementPresetSelfTest` and are not treated as fatal.
- The harness covers collision policy values, not a full C++-vs-JSON collision
  trace (there is one collision solver; the selector chooses its response data).
- The harness has no limb/weapon or ground-settling C++/JSON variant because
  those are owned by the single collision package; the package self-test covers
  them.
- `behaviorSource: "cpp"` still switches tool phase sets / `weaponArms` /
  aimbody to their compiled paths; a full C++ mode is not yet decoupled.
- Animation uses `behaviorSource` for the data features and `locomotionSource`
  for the sampler; the two are not unified into a single animation selector.

## Pre-existing changes preserved (not part of this session)

Unrelated working-tree edits left untouched: `config/collision.json` behavior
keys, `collision-policy.cpp`, `collision-abi.h`, and the untracked
regression/changelog records.
