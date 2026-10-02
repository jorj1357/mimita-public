# Disable no-cold-build rule

- Date: 2026-10-01
- Scope: Documentation policy update

## Change

The no-cold-build rule is disabled as of 2026-10-01. Cold builds are fine for
now. The prior live-development, build, task-completion, regression, telemetry,
and audio restrictions remain in the documentation as historical context but no
longer block the current workflow.

## Updated references

- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/architecture/live-development/live-development.md`
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/README.md`
- `docs/architecture/live-development/hot-audio-contract.md`

## Evidence

- Confirmed the current-policy marker appears in all six active references.
- `git diff --check` passed for the documentation edits.
- No game build or runtime validation was needed because this was documentation-only.
