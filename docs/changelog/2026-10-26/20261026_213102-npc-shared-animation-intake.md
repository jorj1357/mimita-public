# NPC shared animation intake

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-26T21:31:02Z (2026-10-26 17:31:02 EDT)
- Branch: `20261011zt`
- Base commit: `c393337d60749c30399f669604658404d968479e`; no commit created.
- Pre-existing edits: `config/analytics.json`, `config/audio/music-settings.json`, and `config/collision.json` remain modified from earlier work and were not changed in this session. Prior replay intake edits and evidence files remain untouched.

## Changed

- `docs/specs/open-gameplay-issues.md`, New intake near line 46: recorded request to try shared authored NPC animations covering locomotion, crouching, jumping/air/fall/hover/landing, aim in/out, and graded hurt reactions; included AimBody/whole-body aiming composition and hitbox predictability questions.
- `docs/specs/open-gameplay-issues.md`, priority rank 5 near line 1033: expanded the NPC combat/movement row to include expressive shared animations and their open integration questions.

## Exact content changes

- Added: `NPCs need a more expressive shared animation set` with the requested state examples and a next step to check whether the existing state machine and profiles can express the idea.
- Clarification: the existing animation specification already calls for one shared runtime and reusable clips across players and NPCs, so this intake explicitly does not ask for a second NPC-only path or establish a confirmed defect.
- Updated priority rank 5 from combat tuning, weapon rotation, and movement sequences to also include animation-driven movement and presentation, AimBody/whole-body aim composition, and physical hitbox consistency.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/animations/animations.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, and `docs/skills/spec-behavior-review-v1.md`.
- `docs/skills/documentation-checker-v1.md`: PASS. The issue is framed as an experiment/design request and linked to existing owners; no separate NPC animation implementation is proposed.
- `docs/doc-review-09-03-2026.md` is referenced by the router but absent from this checkout.
- `git diff --check`: PASS for whitespace. Git reported a line-ending notice for the pre-existing `config/collision.json` modification; it was not changed.
- No source code, animation asset, or configuration changed. No build or runtime animation/hitbox validation was performed. Human review/playtesting remains needed for clip choices, AimBody blending, and the relationship between visible poses and hit registration.
