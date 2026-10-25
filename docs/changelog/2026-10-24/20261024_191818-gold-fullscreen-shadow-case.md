# Gold reference for a focused fullscreen rendering correction

- Result: PASS
- Timestamp: 2026-10-24 19:18:18 EDT (2026-10-24T23:18:18Z)
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7`
- Scope: add a reusable gold document explaining the fullscreen shadow update diagnosis, small correction, build evidence, and remaining visual review.

## Change

Added `docs/gold/20261024_191700-fullscreen-shadow-small-fix.md`.
The document records the user's symptom and clarification, the October 22
every-other-frame shadow update change, the user's requested mode meanings and
mode-1 selection, the concise correction, build 3102, and the fact that visual
acceptance is still pending. It preserves the user's “1 plan prompt then 1
little edit” characterization without claiming that a build proved the visual
issue is resolved.

The reference links to the existing source/build changelog, issue note,
fullscreen specification, and performance specification. All referenced
files exist. It includes an audience, purpose, and explicit evidence boundary.

## Review and validation

- Read `docs/ROUTER.md`, the existing gold examples, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Focused skill: `docs/skills/documentation-checker-v1.md` — PASS for the new
  document's purpose, audience, evidence boundary, and reference links.
- The router's `docs/doc-review-09-03-2026.md` target is absent in this
  checkout. No `docs/gold/README.md` is present; the router already identifies
  `docs/gold/` as the home for evidence and changelog examples.
- `git diff --check`: PASS, with line-ending normalization notices for
  unrelated pre-existing files.
- No code, build, tests, or runtime behavior changed in this documentation-only
  follow-up. The prior build/runtime evidence is linked and remains unchanged.
- The repository-wide TODO scan found existing unrelated TODO notes; none are
  in the new gold document, and none were edited as part of this request.
- Pre-existing working-tree changes and the prior shadow correction were
  preserved.
