# Agent command proof and priority 1/2 handoff

- UTC: 2026-10-16T23:10:15Z
- Local time: 2026-10-16 19:10:15 EDT
- Scope: specification documentation only; no gameplay code, builds, tests, or live processes changed.
- Instructions followed: `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, and `docs/workflows/runtime-scenario-validation.md`.

## Changed documents

### `docs/specs/dev-loop/dev-loop.md`

- Added the “Agent command proof before scenario automation” section at line 826.
- Before, the document described the normal-owner constraint and a generic
  completion evidence list, but did not provide per-command owner paths and
  minimum live-journal evidence. Exact existing text:

  > Build success and a running window are necessary setup evidence, not feature acceptance.

- After, the new section begins:

  > Do not treat command registration, queue acknowledgement, or a `startup.command.dispatch` event as proof that an agent action worked. Prove each adapter through its full owner path in a fresh identified run before building a larger scenario around it:

- It names current `agent.move`, `agent.aim`, `agent.action`, `agent.shoot`,
  and `agent.snapshot` owner paths and the minimum evidence needed to tell
  dispatch/acceptance from applied action and resulting state. It sequences
  these proofs before implementing the full runner, then priority 1 spawn and
  priority 2 weapon scenarios.

### `docs/specs/open-gameplay-issues-2026-10-16.md`

- Added “Execution order and agent handoff for priorities 1 and 2” at line 122,
  plus a copyable first-agent prompt at line 167.
- Before, priority 2 already said:

  > The component-based weapon migration is a separately staged architecture task, not an implied one-shot rewrite.

  and the next section was:

  > ### Plan next: high-impact gameplay and visual work

- After, a five-gate plan now precedes that section: prove agent command
  adapters with live journal evidence; complete scenario lifecycle/verdict;
  prove priority 1 before integrated combat acceptance; trace/fix priority 2;
  then migrate weapon definitions in parity slices. It permits source-level
  priority 2 investigation in parallel and makes the broad migration
  conditional on what the trace shows. It adds acceptance outcomes and an
  explicit no-code-yet investigation prompt.

### `docs/changelog/2026-10-16/20261016_231015-agent-command-scenario-handoff.md`

- New append-only changelog for this documentation session. No existing
  changelog was edited.

## Source facts checked

- `src/main-systems.cpp::registerAgentCommands()` registers the five agent
  adapters. `agent.move` dispatch is logged as `avatar.command.dispatch`.
- The current movement path includes `src/input/input-poll.cpp` and
  `src/engine/engine-tick-net.cpp`, where `agent.move.applied` is emitted.
- `agent.snapshot` emits pose/state sampling events from
  `src/main-systems.cpp`; its event alone cannot prove a preceding action.
- `src/main.cpp` emits `scenario.finished` with
  `assertions_evaluated=false` in the current scenario-command path.
- `devscripts/dev-loop.py` parses and queues scenario-file commands; its
  parser/queue path is not the complete start/join/wait/assert/verdict runner
  specified in `dev-loop.md`.
- `src/game/game-cli.cpp` prints `EVENTS_JSONL_PATH` for the running process.
  No runtime was launched in this session, so this changelog makes no claim
  that a fresh agent command or gameplay scenario has passed live.
- Weapon destination architecture and current mixed registration/slot model
  are already described in `docs/specs/weapons/weapons.md`; this update points
  to staged migration and avoids duplicating that architecture spec.

## Validation and limits

- Inspected routed docs and relevant source references; ran `git diff --check`
  on the two edited specification documents. No whitespace errors were
  reported; Git noted that the existing dev-loop document has LF/CRLF
  normalization pending.
- No build, test, or runtime validation was performed, as requested by the
  documentation-only scope.
- Router references `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md`; those files were not available in the
  inspected checkout.
- No product behavior decision was made. Whether priority 2 requires the
  broad stable-ID/weapon component migration remains conditional on its
  trace-first findings.
