# Replay export and crash evidence intake

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-26T21:27:04Z (2026-10-26 17:27:04 EDT)
- Branch: `20261011zt`
- Base commit: `c393337d60749c30399f669604658404d968479e`; no commit created.
- Pre-existing edits: `config/analytics.json`, `config/audio/music-settings.json`, and `config/collision.json` were already modified. They were preserved. The issue tracker had other existing content and was edited only for this request.

## Changed

- `docs/specs/open-gameplay-issues.md`, new intake entry near lines 46-62; updated Replay export owner row near line 666 and priority rank 8 near line 1024.
- `docs/evidence/crashes/crash_20261026_212548.txt`: copied the user-named crash report into the repository so the issue tracker can link to stable evidence.

## Exact content changes

- Old intake: no current replay-export fidelity/crash evidence entry.
- New intake: fullscreen and windowed export compatibility; ragdoll visibility; matching gameplay/replay sound cut and retrigger behavior; blood decals retaining world-space orientation rather than facing the replay camera; reported onset about one week before 2026-10-26 (approximately 2026-10-19, uncertain).
- Old replay tracker row: `Replay export reportedly broke after the fullscreen modes were added.`
- New replay tracker row: detailed user-reported fidelity gaps with linked crash evidence and a reproduction checklist; explicitly records the exception cause as unknown.
- Old priority rank 8: export/ragdoll unreliability, headless and progress requirements, generic request to isolate stages.
- New rank 8: includes fullscreen/windowed behavior, sound and blood fidelity, ragdolls, and the crash report as a lead, while retaining the existing headless/progress goals.
- Copied file SHA-256: `90A3778892D6B5A3DC4AED639B358A5DD00F86D5E5470989AF95BD7C8652D43B`.

## Evidence and interpretation

- Read `docs/ROUTER.md`, `docs/specs/replay/replay.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, and `docs/skills/documentation-checker-v1.md`.
- The router references `docs/doc-review-09-03-2026.md`, but that file is absent from this checkout.
- The crash report says `Exception type: unknown` and `What: (none)`; its stack is not symbolized to a useful game function. It lists repeated frame spikes attributed to `Rendering::ReplayActors::Actor` (up to 403.8 ms), followed by a 2602.8 ms `EngineTick::Work` spike. These are leads only; the report does not establish that replay export caused the crash.
- The approximate one-week onset is user-supplied, not independently established.

## Validation and remaining review

- `git diff --check`: PASS for whitespace. Git emitted a line-ending notice for the pre-existing `config/collision.json` modification; it was not changed here.
- Verified the copied report exists and the tracker links point to it.
- Documentation Checker v1: PASS for the updated issue rows and evidence link; user symptoms are attributed and unknown crash causation remains explicit.
- No build, test, runtime export, or crash reproduction was performed. Human review still needs a matched source capture and exported output in fullscreen and windowed modes, including ragdolls, sound transitions, and blood decal orientation. The crash needs its matching run journal and symbolized dump/stack before assigning a cause.
