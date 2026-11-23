# Build/test wait review: game-dll-build game_build (issue-d668de6bfc5e)

Date: 2026-11-22 ~19:18Z
Agent: build-wait-reviewer
Wake: wake-fe718d06a423
Issue: issue-d668de6bfc5e (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, build-script, or test edits. No
compiler, `build.py`, module worker, or second dev-loop invoked. One final
changelog for this session; the only repository writes are the coordination
store's issue documents and this note.

## What was reviewed

- Wait `game-dll-build:2026-11-22T18:05:34.493Z`: kind `game-dll-build`, stage
  `game_build`, elapsed 60.302s vs 60.0s budget, source_generation 7,
  request_ids ["auto-7"], reason revision
  `007813969191c69908bf8c8bcc054a067e50e7fb90607ab3524d7ebf91ff6e6e`.
- Journal `logs/2026-11-22/20261122_174002/dev-loop/events.jsonl`: one
  `dev-loop.wait.over_budget` (18:06:37.952Z, created=true) and one
  `dev-loop.wait.reviewer-wake` (18:06:37.951Z) for this wait_id; issue
  `occurrences=[]` beforehand.
- Underlying attempt `.dev/build-timings/game-dll-20261122T180535335873Z-773b0b.json`:
  started 18:05:35.335Z, completed 18:08:52.214Z, generation 508, success=true,
  total 196.865s. Changed `src/network/server-npcs.cpp` (note 17d49b9187b4,
  horde-behavior-evidence) compiled 100.531s; changed_source_preflight 100.608s;
  link.lld_linker 58.423s. The build was actively progressing and succeeded.
- Wake dispatch (`wake-fe718d06a423`): queued 18:06:35.961Z; codex
  claimed/launched 18:06:37.632Z/18:06:37.819Z then exited; wake.retry to
  opencode 18:25:25.086Z; opencode claimed/launched 18:35:56.541Z/18:35:56.591Z
  (this session). Queued exactly once, but dispatch took ~29m20s.

## Verdict

`reviewed-with-findings` on issue-d668de6bfc5e. The wait record and its
one-issue/one-wake mechanics are correct; the sample was a real, productive
~197s game-DLL build, not a stall. related_build was corrected to
`game-dll-20261122T180535335873Z-773b0b` and related_run to `20261122_174002`.

Evidence gap (reported, not fixed): the wait evidence `build_id`
`2026-11-22T17:39:49.763492Z` and `generation` 507 are the prior published
build, not the in-flight attempt 773b0b / generation 508, which appears only in
build-timings.

## Cross-links

- issue-84c584957e65: appended occurrence for the ~29m20s late reviewer wake.
- Systemic owners unchanged: issue-9a4503f6ed68 (over-budget wait policy),
  issue-136d4553ff92 (stage accounting). 118 identical game-DLL wait issues
  exist, 9 created after 17:00 today.

## Claims

- Source evidence: review-only (dev-loop journal, build timings, agent-wake events).
- Build evidence: attempt game-dll-20261122T180535335873Z-773b0b, generation 508,
  196.865s, success.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Limitations

No fix or closure claimed. The per-wait_id issue flood and the 60s budget remain
a policy decision; left unowned.
