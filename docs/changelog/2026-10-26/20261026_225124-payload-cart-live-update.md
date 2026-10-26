# Payload cart live-update issue intake

- **Time:** 2026-10-26T22:51:24Z (18:51:24 EDT, America/New_York).
- **Branch:** `20261011zt`.
- **HEAD at session:** `c393337d` — `reporetdly working image decal shared shader state`.
- **Result:** PASS_WITH_HUMAN_REVIEW. Documented the desired live-update behavior and its runtime evidence; no game code was changed.

## Change

Updated `docs/specs/open-gameplay-issues.md` under “Cart placement question”
(new subsection “Payload authoring should update the active match live” at
line 622).

The added requirement says that valid cart-origin and route changes made while
a Payload match is running should create/update the cart for clients without
restarting the match. It cites run journal
`logs/2026-10-26/20261026_223138/events.jsonl`: the `payloadsmall1` server
reported `payload.config-invalid` at startup, and the session had no
`payload.cart-spawned` event after later map-editor saves. It also keeps the
effect of editing the route during cart movement as an explicit open rule, to
avoid silently resetting progress or teleporting the cart.

## Review and validation

- Read `docs/ROUTER.md`, the Payload and Entity Editor specifications,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`, and
  `docs/architecture/terminal-commands/terminal-commands.md` in the preceding
  investigation turn; reviewed the relevant issue-index section again here.
- Focused documentation review: PASS. The report distinguishes the observed
  startup failure from the desired live behavior and leaves route-progress
  semantics open.
- `git diff --check` was run for the edited issue index and this changelog.
  No build or new test was run; this is a documentation-only change.
- The issue index was already modified and the worktree contains unrelated
  pre-existing source/config edits and untracked files. Those changes were
  preserved.
- Human visual confirmation that the cart appears after live authoring remains
  required; this session did not change or validate runtime behavior.
