# Dev-loop specification contract

- Timestamp: 2026-10-13T20:05:00Z
- Branch: `20261011zt`
- Result: PASS_WITH_HUMAN_REVIEW
- Pre-existing edits: none observed before this session.
- Repository code changed: no.
- Specification changed: `docs/specs/dev-loop/dev-loop.md`

## Routed documents and focused reviews

Read and applied:

- `docs/ROUTER.md`
- `docs/specs/debug-logging/AI-fix-live.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`

## Change

Replaced the informal 36-line TODO with a 449-line normative development-loop
specification. The new specification defines:

- purpose, ownership, terminology, and separate build/runtime/human claims;
- process-safety invariants and ownership requirements;
- source watching, build, publish, retention, and stale-artifact behavior;
- exact meaning of `[1]` and automatic-build behavior;
- server/client identity and replacement rules;
- persistent-server and shutdown behavior;
- headless versus Windows Firewall semantics;
- required bounded structured events and evidence fields;
- the AI-fix-live investigation procedure;
- mode-11 performance-evidence requirements;
- acceptance criteria and unresolved implementation questions.

The specific intended stale-build behavior is now written as:
build 2135 server -> source change -> build 2137 -> explicit `[1]` ->
identify and stop only the owned stale server -> launch and verify 2137 server
and client identity.

## Validation

- `git diff --check -- docs/specs/dev-loop/dev-loop.md`: passed; only the
  normal LF/CRLF warning remains.
- `python devscripts/dev-loop.py --status --json`: read-only status succeeded.
- Current live run observed:
  - profile: `npc-navigation`
  - mode: `10`
  - build: `2195`
  - server/client executable: `.dev/builds/2195/mimita.exe`
  - `builds_match=true`
  - fresh journal: `logs/2026-10-13/20261013_195456/events.jsonl`
- This runtime evidence proves the current mode-10 pair identity only. It does
  not prove mode-11 collision performance, avatar selection, firewall absence,
  or the new specification's unimplemented requirements.
- No build, restart, process stop, or code/runtime mutation was performed.

## First implementation mismatches identified

- The script compares the in-memory tracked server executable and normalized
  arguments, but status exposes only `builds_match`; it does not expose the
  first mismatch field required by the specification.
- Process discovery currently guarantees one dev-loop daemon for the checkout,
  but the server lifecycle is primarily held in the current daemon's
  `server_process` state. Safe adoption/rejection of a server left by a
  previous daemon remains unproven.
- Firewall handling currently observes possible prompt windows and records
  them; it does not establish a preflight permission result. The specification
  intentionally requires this to be reported as clear, prompt detected, or
  check unavailable.
- The current mode-10 live run is not a valid mode-11 acceptance run.

## Human review still required

- Confirm the normative process-replacement and persistent-server policy.
- Decide whether a stale owned server should be stopped only at explicit launch
  time or whether an explicit server-stop action is also required.
- Review the required Windows Firewall preflight behavior before implementation.
- Approve implementation work against the new specification.

