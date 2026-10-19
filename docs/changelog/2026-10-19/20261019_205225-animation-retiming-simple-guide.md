# Simple Melee animation timing guide

- Local time: 2026-10-19 16:52 EDT (UTC-04:00)
- UTC: 2026-10-19 20:52 UTC
- Branch/starting commit: `20261011zt` / `35c9b12a`
- Request: explain the already specified marker-retiming behavior and how to edit it in beginner-friendly terms.

## Change

- Updated `docs/specs/animations/animations.md:319-361` with a flipbook analogy, a step-by-step edit path for `kick_a.json`, the current kick marker values, exporter preserve/reset guidance, and a plain explanation of how moving the active/recovery markers changes visible speed and gameplay timing.
- Before: the spec explained the mapping using technical language: “each interval between adjacent phase markers is mapped linearly from its runtime `[tick]` interval to its authored `[sourceTick]` interval.”
- After: it adds “Think of `sourceTick` as the page in Blender's flipbook where a pose was drawn. Think of `tick` as when the game should show that page.”
- The guide states that editing `tick` changes playback timing while `sourceTick` stays fixed; playback markers must stay ordered, and `durationTicks` must match `action.end.tick`.

## Review and validation

- Followed `docs/ROUTER.md` to the animation specification and applied `docs/skills/spec-behavior-review-v1.md`.
- Confirmed the example against the active `kick_a.json`: active is playback tick 2/source tick 20, recovery is tick 40, end/duration is 59. The guide explains that the active pose span is consequently spread over 38 runtime ticks, and that moving recovery to 15 makes the active span 13 runtime ticks.
- `git diff --check -- docs/specs/animations/animations.md` passed; Git emitted only its LF-to-CRLF working-copy notice.
- Documentation-only edit; no build or runtime run was needed. The prior gameplay visual acceptance remains separate.
