# Fullscreen shadow cadence flicker

- Recorded: 2026-11-04T00:13:49Z (2026-11-03 20:13:49 EDT)
- Status: ATTEMPTED FIX (2) — source cadence restored; mode 6 visual review pending
- Audience: MiMITA graphics and performance maintainers
- Purpose: track repeated world flicker reported while using fullscreen mode 6.
- This record does not claim that a build or source correction proves the visual issue is resolved.

## Expected behavior

Mode 6 renders the complete game frame at 1024x768 and stretches it to fill a
borderless display. Moving world geometry and its shadows should appear stable
from frame to frame. The graphics contract is in
`docs/specs/graphics/graphics-resolution.md`, under “Fullscreen modes” and
“Stretched fullscreen.”

## Occurrence 1 — 2026-10-24

The user reported choppy fullscreen drawing and wrong depth/shadow presentation.
The history investigation found that a performance optimization had changed
shadow-map refresh from every rendered frame to alternating frames. The
correction selected mode 1 (full-rate refresh), documented in
`docs/changelog/2026-10-24/20261024_181414-full-rate-shadow-mode.md` and
`docs/gold/20261024_191700-fullscreen-shadow-small-fix.md`. That correction was
built, but visual acceptance was left pending.

## Occurrence 2 — 2026-11-03

- Actual: the user reports the same flickering-world effect in fullscreen mode
  6, for the second occurrence, and identifies the earlier performance work
  that reduced shadow rendering as the likely cause.
- Cause found in source: the current working-copy diff in
  `src/engine/engine-tick-render.cpp`, `engineTickRender()`, selected
  `kShadowUpdateMode = 2`. Mode 2 updates the shadow depth map only on
  alternating rendered frames; the renderer reuses the previous map on the
  intervening frame. The shadow map feeds scene shadowing, so its stale contents
  can make shadows lag moving content and plausibly produce the reported
  flicker. This is consistent with the earlier diagnosis, though the report was
  not reproduced during this session.
- Wrong code: `constexpr int kShadowUpdateMode = 2;` with a comment selecting
  alternating-frame refresh for performance.
- Corrected code: `constexpr int kShadowUpdateMode = 1;`, with a comment stating
  full-rate refresh avoids stale shadow depth between moving frames.
- Fix: restore the Oct. 24 full-rate selection. The alternating mode remains in
  the existing conditional for later performance comparisons after visual
  stability is confirmed.
- Proof: source inspection confirmed mode 2 was selected in the current working
  copy and that mode 2 skips every other shadow-map render. The Oct. 24 record
  documents the prior full-rate correction. Dev-loop build published
  `.dev/builds/2026-11-04T00-15-03.911565Z/mimita.exe` (build ID
  `2026-11-04T00:15:03.911565Z:3251d56b48e890950423f4d08416d5a099e70871baf81d469f24dbf1ea6cc37d`); its `--versioninfo` reported
  `EVENTS_JSONL_PATH=logs/11-03-2026/20261103_191516/events.jsonl`. The build
  and identity probe confirm the corrected source was published. No live mode 6
  run or visual comparison was performed, so resolution is not yet proven.

## Evidence and follow-up

- Specification: `docs/specs/graphics/graphics-resolution.md`.
- Relevant owner: `src/engine/engine-tick-render.cpp`, `engineTickRender()`,
  shadow update cadence block.
- Prior correction: `docs/changelog/2026-10-24/20261024_181414-full-rate-shadow-mode.md`.
- Current session record: `docs/changelog/2026-11-04/20261104_001349-fullscreen-shadow-cadence.md`.
- Human review still needed: run the exact newly published executable in mode 6
  on a moving scene and confirm world and shadow stability. A build alone is not
  visual acceptance.
