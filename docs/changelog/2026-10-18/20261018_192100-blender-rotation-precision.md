# Blender animation rotation and export precision

- Timestamp: 2026-10-18T19:21:00Z (2026-10-18 15:21:00 EDT)
- Branch: `20261011zt`
- HEAD before work: `3bf384e4`
- Status: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Clarify why the Blender punch can look different in-game; make the exporter write
readable two-decimal transforms without tiny scientific-notation values; update
the punch asset; and align implementation with the written transform contract.

The shared animation loader reads quaternion components in WXYZ order, normalizes
them, interpolates with shortest-arc SLERP, and the pose owner applies the
result as `restM * poseM`. The exporter computes a rest-relative matrix delta.
However, the exporter imports the default rest GLB and does not reproduce or
verify the player loader's canonicalization against the avatar model that will
play the clip. A difference in GLB basis, runtime normalization, root, or part
pivot can therefore produce a different apparent rotation in-game. The
available screenshots do not establish which frame/pivot differs, so no guessed
runtime basis rotation or Euler-order change was added. This session does not
claim that the in-game pose mismatch is fixed; the spec now marks runtime bind
parity as unresolved and requires it before visual acceptance.

## Changed files and exact edits

### `tools/mimita_animation_exporter.py` — `finite_vec`, line 160

Before:
```python
return [round(float(v), 8) for v in values]
```
After:
```python
rounded = [round(float(v), 2) for v in values]
return [0.0 if value == 0.0 else value for value in rounded]
```

The exporter keeps finite-value validation, rounds serialized transform vectors
to two decimal places, and canonicalizes rounded positive/negative zero to
`0.0`. The completion report now states precision, the rest-relative WXYZ export,
and the unverified runtime bind-frame assumption (lines 408–412).

### `docs/specs/animations/blender-animation-authoring.md` — transform contract, lines 180–220 and 292–311

Before:
```text
The first exporter computes poseDelta = inverse(restRootRelative) * animatedRootRelative ...
The script does not reproduce geometry-based avatar normalization ...
```
After: a complete `Exact transform contract and known limitation` section now
states the four matrices and multiplication order, canonical runtime-bind
requirement, currently unverified normalization/pivot assumption, and the
rule against guessing a 90-degree correction. The output section defines meter
units, WXYZ quaternion order, two-decimal serialization, zero cleanup, and the
quantization/review tradeoff.

### `docs/specs/animations/animations.md` — conversion and numeric format, lines 458–460 and 512–519

Before:
```text
Validate B against the normalized avatar frame before accepting an exporter.
```
After:
```text
Validate B against the normalized avatar frame before accepting an exporter. The exporter must
compare its rest matrix and pivot with the exact runtime-normalized model;
importing the default reference GLB in Blender does not establish parity.
```

Added the shared two-decimal, near-zero output rule and explicit statement that
precision does not establish basis/pivot correctness.

### `config/animations/library/melee/captainfalcon/punch_a.json` — all transform vectors

Before (head tick 0):
```json
"translation": [0.58908653, 3e-08, -0.19297302],
"rotationQuaternionWxyz": [0.99463761, -4e-08, 0.10342155, 3e-08]
```
After:
```json
"translation": [0.59, 0.0, -0.19],
"rotationQuaternionWxyz": [0.99, 0.0, 0.1, 0.0]
```

All exported numeric transform values in the file were rounded to two decimal
places; near-zero values are `0.0`, and no exponent notation remains. Runtime
normalizes quaternion input. Translation rounding is one-centimeter precision.

### `docs/changelog/2026-10-18/20261018_192100-blender-rotation-precision.md`

New file containing this session record.

## Validation and evidence

- `python -m py_compile tools/mimita_animation_exporter.py`: passed.
- Parsed `punch_a.json`; confirmed 84 float values, at most two decimal places,
  no scientific notation, and two keyframes: passed.
- `python devscripts/dev-loop.py --request build --wait-for-build`: success,
  published build 2602 at `.dev/builds/2602/mimita.exe`.
- The existing dev-loop status showed a live NPC-navigation client/server on
  build 2601. The build request did not terminate or replace those processes.
  No Melee runtime action was triggered and no new gameplay journal was
  collected; runtime/visual acceptance remains pending.
- `git diff --check` reported trailing whitespace at
  `docs/specs/gamemodes/supersmashbrosmelee.md:865`, which was already modified
  before this work and was preserved untouched.
- Router-referenced `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md` are absent from this checkout.

## Instructions and focused reviews

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
`docs/specs/animations/blender-animation-authoring.md`,
`docs/specs/avatar/avatar-editor.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/skills/spec-behavior-review-v1.md`, and
`docs/skills/asset-checker-v1.md`. The specification review found an unclear
and unimplemented bind-frame parity requirement. Asset review confirmed the
shared library path and Captain Falcon include. Focused review result:
`PASS_WITH_HUMAN_REVIEW`.

## Pre-existing work preserved

`docs/specs/gamemodes/supersmashbrosmelee.md` was already modified before this
session and remains unchanged by this work.
