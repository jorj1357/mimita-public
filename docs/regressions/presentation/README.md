# Presentation regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 1
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| A presentation effect that is switched at runtime differs between the world, UI, and output passes. | Name the pass that owns the effect, prove the pass with a deterministic pattern, and check the journal for generation swaps between switch and capture. |
| A pixel probe measures natural colour instead of the effect. | Use an achromatic deterministic pattern and a channel-edge metric; assert the effect-off mode at exactly zero. |

---

## Entry 1 — Bloodtest chromatic-aberration lens modes

- Recorded: 2026-11-26
- Status: FIXED IN SOURCE AND RUNTIME — modes 1–5 pass the atomic pixel probe on generation 738, fullscreen 6 stretch and animation mode 2 tether pass their verdicts; human visual tuning pending
- Summary: Five exclusive CA presentation modes (mixed, world-only, normal, world+UI, bodycam-special), plus the 1024x768 whole-frame stretched pipeline, a live CA toggle, no-limit lens config, and animation mode 2's soft limb tether. The record lists the confirmed regressions found while separating them and the pixel/journal proof for each mode.
- Detail: [bloodtest-chromatic-aberration-modes-REG-2026-11-26.md](bloodtest-chromatic-aberration-modes-REG-2026-11-26.md)
- Runtime: run `20261126_231556`, generation 738
- Screenshots: `build/presentation-probes/proof{2,5on,5off}-*.png`
