# afad20a limb collision gold reference + grounded down-dash proof

Date: 2026-09-23
Status: gold document written; grounded down-dash repeat proven deterministically;
animation 1:1 restoration still open

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Work performed

- Wrote the gold/reference document
  `docs/gold/2026-09-23-afad20a-limb-collision-method.md`:
  - what the `afad20a` per-limb producer actually did
    (`computeBodyPartCenter`, one world-space sphere per
    `Player::physicalBody.parts`, `previousWorldTransform` sweep, no extra size
    scale, `p.pos += correction`);
  - how the break was localized from the live journal
    (`limbSrc=body.parts limbCols=6 limbHits=0`);
  - the root cause (root-relative capability + hot re-composition + size scale +
    the `-0.138` feet offset);
  - the fix (world-space capability + `space` flag + exact afad20a formula);
  - the reusable verification method (`limbSrc`/`limbCols`/`limbHits`/`limb0`)
    and the lesson: add the counter that separates "not submitted" from
    "submitted but misplaced".
- Added a deterministic test to `src/physics/movement/movement-selftest.cpp`:
  on a flat floor, press Q for one tick and release for nine, 20 times, and
  count the `HOT_FIRED_DOWN_DASH` facts the hot movement publishes. It also
  counts grounded ticks. This proves the `afad20a` grounded down-dash repeat.

## Evidence

Build evidence:

- Hot DLL `DLL build success`; cold build `Status: SUCCESS`
  (`mimita-20260922T213948.exe`).

Test evidence (`--movement-selftest`):

```text
[ok] grounded down-dash: stays grounded
[ok] grounded down-dash: fires on each fresh press
```

`--afad20a-parity-selftest`, `--movement-parity-selftest`,
`--live-code-selftest`, `--collision-selftest`: PASS.

Journal evidence for the limb break (old code, generation 6):

```text
movement.collision ... limbSrc=body.parts limbCols=6 limbHits=0 grounded=1 worldContact=1
```

Human acceptance:

- Arm collisions: reported working by the user after the world-space fix.
- Grounded down-dash repeat: deterministic test passes; live re-check pending.

## Limits / still open

- **Down-dash / dash live feel**: the deterministic test proves the grounded
  repeat, but the live session that reported "no repeat" was still on the older
  EXE/generation. A live re-check with the new EXE is needed; the journal showed
  the failing Q presses were airborne (`grounded=0`), which correctly do not
  reset the ability.
- **Animation 1:1 with afad20a** is not done. Currently:
  - idle/walk/return_to_idle sample the `afad20a` JSON keyframes via
    `afad20aSampleClip` (C++) or `sampleClip` (JSON), selectable with
    `locomotionSource` / `behaviorSource`;
  - dash and freeze are still full-body **action clips**, not the `afad20a`
    **pose overlays** (blend-in / hold / blend-out with `snapIn`);
  - the per-part spring is an exponential ease, not the exact `afad20a`
    `springVec3` (translation 90/16, rotation 80/14);
  - the procedural idle sway (arms/legs/torso/head) is not restored.
  These are the next slice.
