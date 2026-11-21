# Bounded agent-wake roadmap priority routing

## Task

- Task ID: `work-574223deadca`
- Session ID: `session-3e7d4837c56c`
- Owner: `build-wait-reviewer`
- Focus: bounded test activation woken by `wake-80fc87f29a24`
- Summary: recovered the truncated wake prompt, reviewed high-priority context
  `ctx-dcb8efb0ade0` and its linked issue, routed the new backlog against
  `ROADMAP.md` phases, acknowledged the context, and reproduced the wake
  truncation bug
- Status: complete, no source edits, no build request, no runtime proof claimed
- Date, time, timezone: 2026-11-21T00:31:22-05:00 (2026-11-21T05:31:22Z)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `d651e133`
- Final commit: uncommitted

## Pre-existing changes

- The worktree was already modified by other agents (config, devscripts,
  `docs/issues`, `docs/regressions`, and prior changelog files).
- This session did not modify or revert any of them.

## Requested behavior

The delivered first user line was only
`You were woken by MiMITA agent-wake. This is a bounded test activation.`
The real assignment was in the wake request
`.dev/agent-wake/requests/wake-80fc87f29a24.json` and its prompt file: review
new high-priority user context `ctx-dcb8efb0ade0`, review its linked issue, then
acknowledge it after routing or repairing ownership.

## Specification alignment

- Read `AGENTS.md`, `docs/specs/dev-loop/agent-wake.md`, `ROADMAP.md`, and the
  linked issue record.
- A woken agent may investigate and produce evidence but does not inherit
  build, merge, or policy authority.

## Findings

- `ctx-dcb8efb0ade0` is linked `primary` to `issue-4392f56334ef` (review and
  sort the new autonomy/vehicle/platform backlog). Routing was already correct,
  so no ownership repair was made and `build-wait-reviewer` took no ownership.
- `ROADMAP.md` "Current priority — 2026-11-17" is already the authoritative
  priority method, so no second ranking system is needed. The issue should
  attach roadmap phase/order metadata to new program issues.
- Proposed routing: `issue-c8290acca248` (live multi-client release gate) is
  phases 1-2 and the regression enforcement instrument, with a first proof of
  pinned two-client melee claim/accept/damage/kill plus one rejected broken
  path; `issue-0539d967d696` (Caravan/vehicle end-to-end) is phase 4 after the
  shared format, with a first slice of car physics/free-play and human+NPC
  driving in one authored map; `issue-5534074d9309` (VR, cross-platform,
  distributed nodes) is phase 5 long-horizon and stays low until Cutover-1 and
  performance are proven. Claim order: `c8290` -> `0539d` -> `5534`.
- `issue-4392f56334ef` remains unowned and has no roadmap-phase field; an owner
  must triage it and add the phase tag its expected behavior requires.
- Wake delivery reproduced `issue-62ab3daa615d` again:
  `MIMITA_WAKE_ID=wake-80fc87f29a24`, `MIMITA_WAKE_ISSUE` empty,
  `MIMITA_AGENT_ID=build-wait-reviewer`; only the one-line banner reached the
  model while the prompt file held the full requested work. The request `issue`
  field is empty for context wakes, so the linked issue was reachable only
  through the context.

## Validation

- `agent-coord sync`, `context list --high`, `issue show`, `issue brief`, and
  `dev-loop.py --status` (lifecycle `game_building`, no run active).
- Coordination writes: one `issue suggest` and one `issue review`
  (`roadmap-phases-routed`) on `issue-4392f56334ef`, one `context ack`
  `ctx-dcb8efb0ade0`, one `issue review` (`reproduced`) on
  `issue-62ab3daa615d`, and `activity set`.

## Not proven

No gameplay, build, or runtime behavior was changed or proven. The priority
routing is a review contribution only; the release gate, vehicle, and
long-horizon issues still require their own owners and pinned live proof.

## Automatic evidence

- `.dev/agent-wake/requests/wake-80fc87f29a24.json`
- `.dev/agent-wake/logs/2026-11-21T03-26-09.523Z-build-wait-reviewer.prompt.txt`
- `logs/2026-11-21/20261121_022840/dev-loop/events.jsonl`
- `docs/issues/improvement/20261121-review-and-sort-the-new-autonomy-vehicle-and-pla-issue-4392f56334ef.md`
- `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md`
