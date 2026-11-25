// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest choppiness evidence review (issue-16500baeb0cc)

# Task

- Task ID: ses_df065a5c1ffe0WV16nzJ46ZIgs
- Session ID: ses_df065a5c1ffe0WV16nzJ46ZIgs
- Owner: build-wait-reviewer
- Focus: performance diagnosis, evidence handoff
- Summary: Agent-parsed run 20261124_212828 client journal and appended measured frame-time, scope, config-poll, Swap, and memory evidence to issue-16500baeb0cc; messaged the root-cause owner with the two cheapest next checks. No source edits.
- Status: in_review
- Date, time, timezone: 2026-11-24T22:52:12+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` |  |  |  | git checkout -- docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md |
| `docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md` |  |  |  | git checkout -- docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md |
| `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` |  |  |  | git checkout -- docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md |
| `docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md` |  |  |  | git checkout -- docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md' |
| `docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md' |
| `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md' |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: build-wait-reviewer
- Focus: performance diagnosis, evidence handoff
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / eb92b02b21e62fd5ec496eee4dcad832f901bb7a / eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-24T19:27:02.742428Z", "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63", "latest_successful_source_generation": 40}, "game_current": {"build_mode": "debug", "code_hash": "cb87962e328830ebd689745d5ba558c502945ad728afc50329d53ad30af9026a", "dll": "build\\game\\generation-627\\game-g627.dll", "generation": 627, "revision": "709286005d1c9eb1d984108245adc6e0f65b6ffab5ed2e20c7117005643f8b0d", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-627\\game-g627.dll", "generation": 627, "published_utc": "2026-11-24T22:33:56.053Z", "revision": "709286005d1c9eb1d984108245adc6e0f65b6ffab5ed2e20c7117005643f8b0d", "status": "ready"}, "progress": {"compiled": 13, "elapsed_s": 47.806, "phase": "build_complete", "result": "success", "skipped": 74, "updated_utc": "2026-11-24T19:26:59.275Z"}}
- Generation: 627
- Artifact: build\game\generation-627\game-g627.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_224314\server-01\events.jsonl
- Session ID: ses_df065a5c1ffe0WV16nzJ46ZIgs

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/analytics.json
 m devscripts/blender-ai
 M docs/issues/README.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
 M docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md
 M docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md
 M docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
?? docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md
?? docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md
?? docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md
```

## Captured diff stat

```text
config/analytics.json                              |  2 +-
 devscripts/blender-ai                              |  0
 docs/issues/README.md                              | 18 +++---
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  8 +++
 ...nt-choppiness-against-the-issue-16500baeb0cc.md | 32 ++++++++++
 ...aces-must-not-become-pass-issue-3c1894ca50ec.md | 16 +++++
 ...e-6-full-frame-stretch-th-issue-e0a6bcbacad0.md | 28 +++++++++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md | 22 +++++++
 ...e-dll-build-at-game-build-issue-a2de003a1900.md | 38 +++++++++++
 ...e-dll-build-at-game-build-issue-ee3e906c8273.md | 32 ++++++++++
 ...e-dll-build-at-game-build-issue-fdf3fb2f88e2.md | 36 +++++++++++
 ...guration-limits-with-warn-issue-9e73745d3154.md | 73 ++++++++++++++++++++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  8 +++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 26 ++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 36 +++++++++++
 15 files changed, 366 insertions(+), 9 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
M	docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md
M	docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md
M	docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-24T19:27:02.742428Z",
    "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63",
    "latest_successful_source_generation": 40
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "cb87962e328830ebd689745d5ba558c502945ad728afc50329d53ad30af9026a",
    "dll": "build\\game\\generation-627\\game-g627.dll",
    "generation": 627,
    "revision": "709286005d1c9eb1d984108245adc6e0f65b6ffab5ed2e20c7117005643f8b0d",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-627\\game-g627.dll",
    "generation": 627,
    "published_utc": "2026-11-24T22:33:56.053Z",
    "revision": "709286005d1c9eb1d984108245adc6e0f65b6ffab5ed2e20c7117005643f8b0d",
    "status": "ready"
  },
  "progress": {
    "compiled": 13,
    "elapsed_s": 47.806,
    "phase": "build_complete",
    "result": "success",
    "skipped": 74,
    "updated_utc": "2026-11-24T19:26:59.275Z"
  }
}
```
