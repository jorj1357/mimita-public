# Git index corruption regression record

- Recorded at: 2026-11-07T16:19:47Z (2026-11-07 11:19:47 EST)
- Branch: `20261011zt`
- HEAD: `839322c0` (`inf world things`), already pushed to `origin/20261011zt` before this documentation-only update.
- Pre-existing worktree edits were left untouched; this task changed only the regression index, its new detail record, and this changelog.

## Change

Added Entry 23 to `docs/regressions/dev-loop/README.md` and changed the count from `Entries: 22` to `Entries: 23`. The entry links to `docs/regressions/dev-loop/git-index-corrupt-interrupted-session-REG-2026-11-07.md`.

The detail record documents Git's observed `bad signature 0x00000000` failure, expected and actual behavior, recovery evidence, and follow-up steps. It explicitly records the user's belief that the PC may have been shut down during AI work as an unconfirmed hypothesis. The `.git/index` last-write timestamp was 2026-11-06 19:05:06 EST; this does not establish cause. Recovery via backup and `git read-tree HEAD` is confirmed by the prior session record at `docs/changelog/2026-11-07/20261107_161106-git-index-repair.md`.

## Documents and review

Read `docs/ROUTER.md`, `docs/regressions/README.md`, the dev-loop regression index and examples, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.

Documentation review confirmed the detail link resolves, the index count matches 23 entries, and `git diff --check -- docs/regressions/dev-loop/README.md` reports no whitespace errors. A TODO-pattern scan found only the explanatory patterns in the router/skill and historical changelog references; no actionable TODO in the changed regression docs required reporting.

No build, test, or gameplay run was relevant. No code behavior or prevention mechanism was changed. Regression status remains OPEN pending evidence about the initiating cause and whether a preventative workflow change is appropriate.
