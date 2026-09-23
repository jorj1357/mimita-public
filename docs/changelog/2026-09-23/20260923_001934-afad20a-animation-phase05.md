# afad20a animation restoration — Phase 5 (locomotion sampling + parity)

Date: 2026-09-23
Status: hot-code built; deterministic tests pass; human live acceptance pending

Related specification: `docs/specs/movement/movement.md`,
`docs/features/live-code-development/live-code-development.md`

Predecessors:
`docs/changelog/2026-09-22/20260922_235824-afad20a-movement-phase01.md`,
`docs/changelog/2026-09-23/20260923_001103-afad20a-collision-phase04.md`

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Scope of this session

Phase 5: restore the `afad20a` animation contract behind the hot animation
boundary and add an explicit C++/JSON locomotion selector. Per the approved
decision, `afad20a` is the C++ source while the existing data-driven path stays
selectable. This session covers the locomotion state/sampling contract and
parity; the dash/freeze pose-overlay timer mechanism is documented as remaining.

## What already existed (not re-implemented)

The hot animation subsystem already contained most of the `afad20a` machinery:
`animation-policy.cpp` owns the idle/walk/return_to_idle + dash/down-dash/freeze
state machine, `pose-generation.cpp` owns the pose build order (clip overlay,
weapon arms, aimbody, afad springs, blend) and publishes via `skeleton.apply`,
and `hot-animation-clips.h` loads the `afad20a` `layers.animations` keyframes.
The gap was that the C++ source (`behaviorSource: "cpp"`) bypassed those
keyframes and used the compiled sinusoidal procedural poses instead.

## Work performed

- `src/hot-reload/hot-animation-clips.h`:
  - Added `afad20aSampleClip`: the exact `afad20a` sampling contract — fixed
    60 Hz tick clock scaled by `walkFrequency * walkFrequencyMultiplier`,
    `fmod` loop wrap, `durationTicks - 1` one-shot clamp, and per-part euler
    interpolation, including the wrapped prev/next tie branch.
  - Added `afad20aSourceCpp()` reading an explicit `locomotionSource` key
    (default `"cpp"`) so the C++/JSON selection is independent of the
    data-feature `behaviorSource` key.
  - Added `afad20aWalkSpeedScale()` reading `walkFrequency` /
    `walkFrequencyMultiplier` from `config/animations.json` (default 0.3).
  - Made `jsonClipApplied` and `actionClip` source-agnostic: both now consult
    the cached `afad20a` keyframes regardless of `behaviorSource`, so the
    return_to_idle transition and afad springs apply to the C++ path too.
  - `evaluateAction` now routes idle/equipped-idle/walk/return_to_idle through
    `afad20aSampleClip` for the C++ source and `sampleClip` for the JSON source.
- `config/animations.json`: added `"locomotionSource": "cpp"` (kept
  `behaviorSource: "json"` so the data-driven extras — tool phase sets,
  weapon arms, aimbody — stay enabled).
- `src/hot-reload/modules/presentation/animation-selftest.cpp`: added a
  deterministic self-test for `afad20aSampleClip` — 60 Hz tick timing, loop
  wrap, one-shot hold, determinism, and C++/JSON sampling parity.

## Evidence

Source evidence:

- `afad20a` sampling contract reconstructed from
  `git show afad20a:src/entities/player-animation.cpp` (`interpolateAnimClip`:
  `tickTime = timeSeconds * 60 * speedScale`, `fmod` loop, `durationTicks - 1`
  one-shot clamp, `glm::mix` on euler degrees).
- `afad20a` clip data is the existing `config/animations.json`
  `layers.animations` block (idle/walk/return_to_idle) loaded by
  `loadJsonClipCache`.

Build evidence (separate from runtime evidence):

- Hot DLL: `python build_game_dll.py` -> `DLL build success`
  (`build/mimita-game.dll`). No cold rebuild was required; only hot headers,
  a hot module, and a config file changed.

Test evidence (deterministic, headless):

- `--live-code-selftest`: PASS, including the DLL candidate self-test with the
  new `afad20a` sampling/parity assertions.
- `--afad20a-parity-selftest`, `--movement-selftest`: PASS.

Runtime evidence:

- Not performed. No running game session was driven and no in-game trace was
  captured in this session.

Human acceptance:

- Pending. The `afad20a` idle/walk/return-to-idle feel and the animation
  transitions still require a human playtest.

## Limits

- This session did not change the visible result: the C++ `afad20a` sampler and
  the JSON `sampleClip` path sample the same cached frames and the parity
  self-test asserts they agree. The value is the explicit selector and the
  C++ implementation of the `afad20a` contract.
- The `afad20a` dash/freeze **pose-overlay timers** (blend-in/hold/blend-out
  with `snapIn`, applied over a base clip) are not restored. Dash/down-dash/
  freeze still resolve through the existing action-clip mechanism
  (`HOT_ACTION_DASH` / `HOT_ACTION_FREEZE`).
- The `afad20a` per-part `springVec3` (stiffness 90/16 translation, 80/14
  rotation) is approximated by `applyAfadSpring` (exponential ease), not the
  exact semi-implicit spring.
- `behaviorSource: "cpp"` still disables the JSON-only extras (tool phase sets,
  `weaponArms`, aimbody). Decoupling those so a full C++ mode is viable is
  remaining work; `locomotionSource` was introduced to avoid that coupling for
  locomotion.
- Animation still runs after collision (POST_MOVEMENT/RENDER) and the stored
  pose is republished before collision; this matches the `afad20a` ordering but
  was not re-verified against the render/collision transform identity.
- No `afad20a` animation fixtures were recorded; the self-test uses a synthetic
  clip rather than captured old poses.

## Pre-existing changes preserved (not part of this session)

Unrelated working-tree edits left untouched: `config/collision.json` behavior
keys, `collision-policy.cpp`, `collision-abi.h`,
`collision-package-solver.cpp`, and the untracked regression/changelog records.
