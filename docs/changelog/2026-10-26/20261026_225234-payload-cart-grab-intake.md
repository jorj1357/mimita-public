# Payload cart grab issue intake

- **Time:** 2026-10-26T22:52:34Z (18:52:34 EDT, America/New_York).
- **Branch:** `20261011zt`.
- **HEAD at session:** `c393337d` — `reporetdly working image decal shared shader state`.
- **Result:** PASS_WITH_HUMAN_REVIEW. Added the cart-grab report and linked existing ragdoll requirements; no code or configuration was changed.

## Change

Updated `docs/specs/open-gameplay-issues.md` under the Payload cart live-update
intake (new “Cannot grab and ride the Payload cart while ragdolled” section at
line 639).

Recorded the player's request to ragdoll, grab the moving cart, and be carried
by it. Linked `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` RAG-027 and
RAG-028, which already require moving physical objects to be grabbable and
grabbed points on moving entities to follow them. The intake requires testing
after the cart is visible and tracing target eligibility, acceptance,
attachment, cart motion, and actor carry, so the prior missing-cart issue is
not mistaken for a separate grab failure.

## Review and validation

- Read `docs/skills/spec-behavior-review-v1.md`, the relevant Payload spec
  excerpts, `config/gamemodes/payload.json`, and the ragdoll grab requirements.
- Focused spec comparison: PASS. Existing requirements cover the intended
  behavior; user report remains unreproduced.
- `git diff --check` was run for the issue index and this changelog. No build
  or runtime test was run; this was a documentation intake.
- Existing unrelated worktree changes were preserved.
