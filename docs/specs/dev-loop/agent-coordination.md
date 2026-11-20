// 2026-11-19
/* purpose
* define shared persistent coordination for concurrent MiMITA agents
* keep tasks, user directives, notes, decisions, messages, and edit leases in one brain
* prevent two agents from silently overwriting the same file
* this file DOES NOT change build authority, gameplay behavior, or the module ABI
* this file DOES NOT promise that an idle model can be woken without a new turn
*/

# Agent coordination

## Authority and scope

This specification owns agent-to-agent coordination in the shared checkout. It
does not replace the dev-loop build/run authority, the build scheduler, the
checkout build lock, or server process ownership.

Agents request builds and isolated test runs through the root dev-loop. They
never invoke a compiler, `build.py`, a module worker, or a second loop. The
root dev-loop remains the only build authority. One EXE build runs at a time;
candidate game-DLL builds and pinned test runs are bounded by PC resources.

The coordination brain is advisory except for active edit leases, `halt`, and
`freeze`. It is not an access-control or security boundary.

The entry point is:

```text
python devscripts/agent-coord.py
```

## Shared brain, named agents

There is one shared authoritative brain and many named worker agents.

- The brain owns tasks, directives, notes, decisions, conflicts, messages,
  leases, freezes, and halt state.
- Each agent has a unique task-based name such as `build-perf`, `header-fanout`,
  `map-editor`, or `juggernaut-60hz`.
- Agents do not have private copies of task state. Their context is a cache;
  `sync` refreshes it from the brain.
- Named identity is required for leases, votes, audit, and conflict resolution.

## Session lifecycle

```text
agent-coord begin --agent <name> --task <description>
agent-coord sync
agent-coord end
```

`begin` registers the agent and starts a session. `end` releases every lease
held by that agent and marks the session inactive.

The main work unit is an **activation**:

```text
user input or delivered message
  -> agent works
  -> agent yields
```

All tool calls and assistant messages inside that block are one activation.
They are not separate turns.

At the start of every activation, run `sync`. During a long activation, check
`dev-loop.py --status` at natural boundaries. `wait` and `ask --wait` are
bounded blocking commands; they are not background polling loops.

## Optional intent

Claims are not mandatory.

`announce` broadcasts likely files, directories, operations, and purpose:

```text
agent-coord announce --agent map-editor \
  --paths src/mapeditor docs/specs/entity-editor/entity-editor.md \
  --purpose "map editor environment controls"
```

Announcements expire and do not block anyone. They exist so other agents can
see likely overlap before it happens. Creation, deletion, and rename intent
should be announced because there is no existing file to compare.

## Messages

Messages are local, bounded, append-only coordination records:

- `question`, `answer`, `ack`
- `announce`
- `handoff.request`, `handoff.accepted`, `handoff.declined`
- `shared.request`, `shared.accepted`
- `proposal.created`, `proposal.applied`, `proposal.rejected`
- `lease.check`, `lease.expired`
- `blocker`, `status`, `broadcast`

Every agent has a read cursor. `sync` and `inbox` deliver unread messages and
mark them read. `wait` blocks until a message arrives or the timeout expires.

Questions may target one agent or `any`. The first answer closes the question.
`ack` prevents a false timeout while the owner is investigating.

Recommended timeouts:

| Item | Default |
|---|---:|
| Normal question response | 5 minutes |
| Long question | 15 minutes |
| Edit lease | 15 minutes |
| Lease check | 10 minutes |
| Intent announcement | 60 minutes |
| Abandoned-session fallback | 12 hours |

True idle-session injection is not provided. Delivery is guaranteed at the
next activation, through dev-loop checkpoints, and through bounded wait.

## Activity and board

The brain owns a live activity record for every agent. It is the shared view
that lets many agents behave as one coordinated worker pool.

```text
agent-coord activity set --agent agent-a \
  --task build-fix --issue issue-... --phase compiling \
  --paths src/gamemode/gamemode.cpp --build-id <id> \
  --game-generation 7 --run-id <id> --blocker "" \
  --next-step "start pinned scenario" --wait-reason "EXE slot"
agent-coord board
```

Activity records task, issue, phase, activity, paths, build ID, game
generation, run ID, blocker, next step, wait reason, and update time. `board`
shows every agent and active issue/task/lease. Coordination actions such as
`begin`, `sync`, `announce`, `edit begin/commit`, task changes, issue
assign/defer, and build/test requests update the relevant activity where
practical.

The spec must be honest about cadence: this is a reported checkpoint state, not
a live feed of an agent's internal reasoning. Agents update at activation and
phase boundaries. A missing update is visible as a stale timestamp, not hidden.

## Issues and append-only records

User reports, TODO surfacing, build-friction findings, improvement proposals,
and capability proposals are `issues` in the brain.

```text
agent-coord issue add "<title>" --quote "<verbatim report>" --source user \
  --via agent-a --kind bug --severity high
agent-coord issue triage <id> --summary "..." --owner agent-b
agent-coord task add B --owner agent-a --issue <id> --acceptance "..."
agent-coord issue defer <id> --reason "..." --until "<UTC>"
agent-coord issue clarify <id> --question "..."
agent-coord issue review <id> --verdict needs-evidence --body "..."
agent-coord issue close <id> --resolution "..." --evidence "..."
```

Issues have `kind` (`bug`, `todo`, `docs`, `build-friction`, `improvement`,
`capability`), state (`new`, `triaged`, `assigned`, `active`, `in_progress`,
`blocked`, `deferred`, `needs_clarification`, `in_review`, `done`,
`cancelled`), owner, reviewers, priority, severity, evidence, deferral, and
resolution.

Detail files live under `docs/issues/<kind>/` and are append-only. The original
human quote is immutable; interpretations, evidence, triage, reviews,
clarifications, and resolutions are appended. `docs/issues/README.md` is a
generated index; it is not a second hand-maintained status source.

A TODO or ambiguous report creates a `needs_clarification` issue with the exact
path, line, and quote. Agents do not edit the source document to resolve it.

## Build/test permits and self-improvement

An active agent may request a build or isolated test through the root dev-loop
without human approval. The request records the agent, task, issue, permit/build
ID, and run ID. Requests are bounded by PC resources; one EXE build at a time.
A pinned test run uses a specific EXE artifact and game-DLL generation/path and
does not replace the shared runtime.

When a build or test waits, the request records the wait reason. An avoidable
wait or a wait above the measured budget creates a `build-friction` issue.
Agents may create an issue for any wait they believe is unnecessary. See
[change-to-evidence-latency](change-to-evidence-latency.md).

Agents are also expected to propose improvements and capabilities, including
coordination improvements a human may assume an AI cannot do. Any agent may
triage, review, reproduce, and gather evidence for or against the proposal. A
human makes the final decision on changes to shared authority. See
[agent-self-improvement](agent-self-improvement.md).

## Tasks and directives

Agents record the user instruction they received:

```text
agent-coord directive add --source user --via Agent2 \
  --task B --text "..." --method "..."
```

The brain stores directives, tasks, dependencies, and methods. It detects:

- the same task with conflicting methods
- dependency cycles
- duplicate ownership
- contradictory constraints
- decisions made stale by new input

A task owner is the default synthesis authority. If the owner is inactive, the
task may be transferred. Other agents contribute notes, options, objections,
and patches; they do not silently replace an active decision.

## Autonomous decisions

Agents do not wait for user approval before normal work.

When inputs conflict:

1. The brain records all known directives and conflicts.
2. The task owner synthesizes the best combined approach.
3. The owner records an active decision immediately.
4. A Markdown decision document is published.
5. Agents implement the decision.
6. The user is notified afterward.
7. A later conflicting user directive marks the decision stale and triggers
   re-synthesis.

A decision records:

- input directive IDs
- chosen approach
- rejected alternatives
- rationale and tradeoffs
- affected paths
- dissent
- supersession history
- document path

Final decisions are published under:

```text
docs/coordination/decisions/
```

Votes are advisory only. One vote per active agent. Unanimity is recorded but
does not replace the owner's decision.

## Edit leases

No agent holds a file for a whole task. A lease covers only the write:

```text
agent-coord edit begin --agent A --path src/x.h --region name --hash <base>
# read, edit
agent-coord edit commit --agent A --path src/x.h --hash <new>
```

Rules:

- At most one active lease per path.
- `edit begin` checks halt, freeze, active lease, shared participation, and the
  expected base hash.
- `edit commit` validates the current file hash, then releases the lease
  immediately.
- `edit abort` releases immediately.
- A failed commit still releases the lease and records a conflict; the agents
  must reconcile before writing again.
- Expired leases are marked stale and audited. They are never auto-merged or
  reverted.
- `guard --staged` fails when a staged file is leased by another agent, frozen,
  or covered by global halt.

## Shared files

Agents may agree to share a file, but only one writes at a time.

```text
agent-coord shared begin --agent A --path src/x.h --with B --regions regions.json
agent-coord shared accept --agent B <shared-id>
agent-coord edit begin --agent A --path src/x.h --region ...
```

Regions must be disjoint. Different regions are edited sequentially under a
file lease. If both agents need the same region, use a patch proposal.

If both agents already changed the same file, `shared` conflict handling freezes
the path, shows the working-tree diff, and requires reconciliation. There is no
automatic merge.

## Proposals and competing implementations

`proposal create` stores a patch from one agent for another agent to apply or
reject. Competing implementations remain as alternative patch options. Only the
selected option edits shared files unless the user explicitly approves a
feature-flagged coexistence.

## Evidence and mandatory proof

An issue may close only from proven evidence:

- `proof_kind=integration` requires a build generation, a required event list,
  and matching events in a pinned runtime journal.
- `proof_kind=unit` is allowed only for explicitly isolated logic such as
  serialization, hashing, or a pure invariant.
- Tests are supporting evidence; they do not replace integration event proof
  for gameplay or user-visible behavior.

```text
agent-coord proof record <issue> --kind integration --generation N \
  --server-journal <events.jsonl> --event scenario.finished
agent-coord proof verify <evidence-id>
agent-coord trace <issue-id>
```

`proof verify` checks the build generation, DLL generation/hash, required
events, and `scenario.finished` success where required. `issue close` refuses
without proven integration evidence or a unit proof. Evidence that is missing,
from the wrong build, from the wrong runtime, or contradicted remains
`unproven`.

The human message quote attached to an issue is durable evidence of what the
user said. Agents may add exact quotes with `issue quote <issue> "<text>"
--supports ...` and cite them for or against a proposal. The task owner decides
and records dissent; a quote is not valid shared evidence unless the agent
recorded it. `trace <issue>` includes the issue, tasks, reviews, quotes, and
evidence.

## Halt and freeze

- `halt` stops agent leases, commits, proposals, and guards.
- `resume` clears it.
- `freeze <path>` blocks new leases under that path.
- `unfreeze <path>` clears it.
- `task pause` pauses one task without stopping unrelated work.
- `agent stop` releases an agent's leases.

Halt is coordination-only. It does not stop the human dev-loop, build, game
processes, or an already-running server/client. Those remain controlled by the
existing dev-loop and process-ownership rules.

## Persistence and events

```text
.dev/agent-coord/
  brain.db           SQLite WAL authoritative state
  events.jsonl       append-only canonical JSONL audit
  reviews/           live conflict review Markdown
  proposals/         stored patch files
docs/coordination/decisions/   published decisions
```

Coordination events use the canonical JSONL envelope but remain a separate
stream from any game `.exe` `events.jsonl`. Game journals are per-run,
per-process evidence; coordination persists across runs and must not pollute
gameplay scenarios.

Required coordination events include:

- `coord.agent.registered`
- `coord.directive.recorded`
- `coord.note.appended`
- `coord.message.sent` / `coord.message.answered`
- `coord.lease.acquired` / `coord.lease.released` / `coord.lease.expired`
- `coord.shared.requested` / `coord.shared.accepted`
- `coord.conflict.opened`
- `coord.vote.cast`
- `coord.decision.recorded` / `coord.decision.stale`
- `coord.halt.set` / `coord.halt.cleared`
- `coord.freeze.set` / `coord.freeze.cleared`

## Dev-loop integration

`dev-loop.py` exposes read-only coordination views:

```text
dev-loop --coord-status
dev-loop --coord-inbox
dev-loop --coord-message <message-id>
```

Normal status output includes an unread/conflict/lease/halt summary and the
activity/issue board when the brain exists and `MIMITA_AGENT_ID` is set. The
status hook never builds, launches, or mutates game state.

`--no-coord` suppresses the status summary. The dev-loop remains the build/run
owner. Coordination records permits, waits, build/run ownership, and agent
activity; an agent’s build/test request still goes through the root dev-loop.

## Acceptance

- Two agents cannot write the same file at the same time.
- `edit commit` releases immediately.
- A failed commit releases and records a conflict.
- A stale lease does not silently overwrite a file.
- Conflicting directives produce one active decision without a user wait.
- A new directive marks the affected decision stale.
- Decision Markdown is published automatically.
- Votes and dissent are visible but not blocking.
- Coordination events never enter game journals.
- Halt/freeze block agent writes but not the human dev-loop or game.
- An agent can see new messages at its next activation or a dev-loop checkpoint.
- `board` shows every agent's reported task, phase, paths, build/run, blocker,
  next step, and wait reason.
- A user report is preserved verbatim; interpretation and evidence are appended
  to an issue document.
- An issue can be triaged, linked to ordered tasks, assigned, deferred,
  clarified, reviewed, and closed with evidence.
- A build/test wait is recorded and an avoidable or over-budget wait is a
  triageable build-friction issue.
- An active agent can request an isolated build/test through the root dev-loop
  without human approval or a second loop.
- Any agent can propose an improvement or capability; humans remain final for
  shared-authority changes.

