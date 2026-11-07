# Dev-loop immutable build snapshots — 2026-11-07

## Change

Added desired behavior to `docs/specs/dev-loop/dev-loop.md` section 5.3 for
capturing immutable build and runtime-data snapshots, compiling independently
of later checkout edits, publishing artifacts by identity, launching exact
artifacts with matching runtime data, and checking matching per-run journals.
Added camera proximity-fade evidence as a concrete example. Clarified in the
current implementation snapshot that this behavior is not implemented yet and
that experimental mode 2 lanes are not authorized by this design.

## Review

- Read `docs/skills/documentation-checker-v1.md` and
  `docs/skills/spec-behavior-review-v1.md`.
- Compared the desired contract with `devscripts/dev-loop.py`: its build worker
  receives the live repository root, and publication copies the executable and
  runtime DLLs rather than freezing the runtime data tree. The spec labels the
  new behavior as desired and retains that implementation limitation.
- No conflicting higher-level behavior requirement was found in the routed
  dev-loop specification context inspected for this change.
- Documentation-only change; no build, runtime trial, or `events.jsonl`
  evidence was produced. Runtime and human acceptance remain required when the
  contract is implemented.

## Validation

- Inspected the modified sections and verified the Markdown structure and
  section flow manually.
- Did not run a build or alter the live dev loop.
