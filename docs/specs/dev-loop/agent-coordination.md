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

## Work-time and phase latency

Every activation that performs work opens one work session:

```text
agent-coord work start --agent <name> --task <id> --issue <id> \
  --message <received-message-id> --notes "received request"
agent-coord work phase <session> planning
agent-coord work phase <session> editing
agent-coord work phase <session> build_wait --wait-reason "build queue" --build-id <id>
agent-coord work phase <session> test_wait --wait-reason "pinned run" --run-id <id>
agent-coord work phase <session> review
agent-coord work end <session> --state done
```

The session measures wall-clock milliseconds from the received user message to
done and records every phase independently. The default phases are `received`,
`planning`, `editing`, `build_wait`, `test_wait`, `review`, `blocked`, and
`done`; a phase may be split when it becomes a measurable bottleneck. `work
summary` groups elapsed milliseconds by phase and agent, and `work list
--active` shows sessions that have not ended. The board includes active
sessions so a wait is visible, not hidden inside one long total.

`begin` opens one active work session for the agent, and `end` closes it. The
session is agent-owned and model-independent; agents do not need a
model-specific instruction to create it.

Build waiting is automatic on the shared dev-loop path. When an agent runs
`dev-loop.py --request build|game-dll --wait-for-build`, the dev-loop records
`build_wait` with the request ID and a wait reason before blocking and resumes
the previous phase when the build succeeds, fails, or times out. This applies
to every agent identity using the dev-loop, regardless of model or client.
Manual `work wait` and `work resume` are fallbacks for a wait outside that path.
`test_wait` uses the same rule when a bounded test-run wait is added.

The measurement is observational, not a performance target by itself. A phase
that is consistently large becomes an efficiency or build-friction issue with
the measured evidence and a proposed reduction.

## Next work and observation

`agent-coord next --agent <name>` returns the highest-priority unblocked issue
or task, its next step, blocked issues, and ownership. `--claim` assigns an
unowned issue to the agent.

`agent-coord observe <issue-id>` returns a bounded context bundle: issue,
tasks, quotes, reviews, evidence, activity, recent messages, build identity,
and matching runtimes. It does not dump unbounded logs.

`agent-coord next --mode waiting` returns repair work instead of the normal
next feature task: missing issue fields, tasks without proof, and stale proof.
It is read-only; the agent chooses whether to claim the issue after reviewing it.

Every `brain_check` also returns bounded `build_resource_gaps` derived from the
active game-DLL progress and newest attempt summaries. The stable `gap_key`
identifies a missing or partial capture so agents can record it once, route it
to the relevant build/resource issue, and coordinate the next discriminating
measurement. This is a gap signal, not a causal diagnosis or an issue priority
decision. A completed resource sample can still list unavailable fields such
as journal-aligned workload or a matched pre-build baseline; agents must keep
those unknowns visible instead of treating `captured` as complete proof.

The wake adapter starts a bounded activation for an idle agent; it does not
create a human or grant build authority. A woken agent must inspect its issue
brief and proof state before acting. It may investigate, repair context, and
append evidence; it may not self-approve a behavior completion.

## Review while waiting

When an agent waits for a build or test, it records the wait reason in activity
and asks for repair/review work:

```text
agent-coord activity set --phase waiting --wait-reason "build queue"
agent-coord next --agent <name> --mode waiting
```

The waiting queue returns, in priority order:

- issues missing reproduction, expected behavior, acceptance, paths, or evidence;
- tasks with no proof or unproven proof;
- tasks with stale proof;
- review-only work that can be completed without the build/test artifact.

Review findings are appended with `issue review` or `note`; the waiting agent
never edits a path owned by another agent. If the build/test completes, the
agent resumes its original task. If no repair work exists, it records that and
continues non-conflicting work.

## General issue review scheduler

The coordination CLI checks the shared review cadence at every `sync` and
when a work session ends. It schedules one bounded independent review after
five globally completed work sessions, or after 30 minutes once at least one
new work session has completed. The time condition is observed on the next
agent `sync`/work completion; the scheduler is not a background model or an
always-running process.

The scheduler uses a durable checkpoint in the coordination database and a
deduplication key in the wake queue, so concurrent `sync` and work-end calls
cannot enqueue the same review twice. Canceled and failed requests with the
same cadence key are requeued instead of being mistaken for a successful
schedule. Old launched requests are reconciled against their child PID so an
exited reviewer cannot block future sweeps indefinitely. It selects one
eligible issue at a time,
prioritizing unproven/stale proof, then priority and longest time since review.
Closed, cancelled, deferred, and clarification-waiting issues are not selected;
issues reviewed in the last 24 hours are skipped. If no eligible issue exists,
or no wake adapter is available, the scheduler records the result and keeps the
reason visible.

The scheduled reviewer cold-reads the issue brief and linked evidence, then
appends a review with observations, counterevidence, uncertainty, and a next
discriminating check. It also checks the evidence path for one concrete
development-process improvement; it appends a measurable proposal to the
existing improvement issue when supported, and records that no improvement was
evident when it was not. This is review-only work: it does not edit source,
close issues, or self-approve proof. A successful suggestion is not evidence
that the underlying issue is fixed.

Track scheduler health with eligible-issue review coverage, age of the oldest
unreviewed high-priority/unproven issue, evidence-bearing review rate,
no-op/duplicate rate, wake failures, and review latency. Revisit the cadence if
it creates repetitive low-value reviews or excessive agent load. A due review
cannot run while no agent activation or wake adapter is available; that gap
must remain visible rather than being reported as completed.

## Role-fluid collaboration and workflow review

Task roles are assigned by the work, not restricted by agent identity or model.
Any agent may temporarily act as investigator, implementer, evidence auditor,
independent reviewer, or workflow/meta reviewer when qualified and when file
ownership is respected. The task owner coordinates the work and records the
synthesis; no permanent privileged meta-agent is required.

At phase boundaries and useful wait periods, agents inspect whether the work
has uncovered missing timing, evidence, ownership, handoff, or test coverage;
avoidable human-only steps; repeatable work that can safely be delegated to AI;
or agent coordination overhead. Findings and recommendations must be
appended to the shared issue tracker, with evidence and a next check, before
or alongside being summarized to the user. Use `issue suggest`, `issue review`,
or a new full issue as appropriate. Do not duplicate a report when an existing
issue can own the added evidence.

Issues remain collaborative working records. Agents append new evidence,
reproductions, tests, failed attempts, counterevidence, hypotheses, proposed
fixes, metrics, and review conclusions over time. Separate observation from
inference and unknowns. A cold handoff must be possible from the issue brief
and linked artifacts without relying on the previous agent's private context.

### CPU and resource-pressure evidence handoff

Any agent that encounters MiMITA CPU, memory, disk, or build-resource pressure
adds a concise evidence packet to the appropriate existing issue (or creates a
complete issue if none fits). Record the UTC interval and sampling duration,
host logical CPU count, process PID and role, per-logical-CPU and whole-machine
CPU percentages, current runtime mode and workload, player/NPC counts, frame
cap/FPS or server tick evidence, concurrent build phase and top competing
processes when relevant, and links to the exact journal or artifact. Separate
measurements from interpretations and list unavailable evidence explicitly.
Read the live mode and workload from the active runtime journal: a dev-loop
launch profile can become stale when a player switches modes in-game. Link
related subsystem issues (for example networking or build throughput) instead
of moving their ownership, and leave additional work claimable unless taking
ownership through the normal claim flow. Keep one shared evidence trail; do not
create duplicate issues or coordination chatter.

## Issue context and awareness gaps

An issue is ready to work only when it has the quote, reproduction, expected
behavior, evidence links, acceptance, file paths/owners, dependencies, risks,
and proof expectation. Missing required context makes it `needs_info`.

`issue context` records the durable fields; `issue readiness` reports the
exact missing fields; `issue needs-info` lists every open issue that is not
ready. A bare title plus a summary is not actionable context.

```text
agent-coord issue context <issue-id> \
  --reproduction "exact input and steps" \
  --expected-behavior "observable correct result" \
  --acceptance "measurable pass condition" \
  --paths path/one path/two \
  --run <run-id> --evidence '{"journal":"..."}'
agent-coord issue readiness <issue-id>
```

An agent that lacks knowledge of game state, intent, ownership, or expected
behavior files an `awareness`/`needs_info` issue with the exact missing
question and impact. It does not silently assume the answer. The issue stays
visible on the board until the user or owner clarifies.

## Questions for the user and shared answers

Any agent may ask the user for missing intent or detail. The coordination
brain is the shared queue; an agent does not need to own the related task to
record a question:

1. Record the user's report and issue context as usual. Use
   `agent-coord issue clarify <issue-id> --question "<one clear question>"`.
   The issue enters `needs_clarification`, and the question is appended to its
   durable detail record. Put the issue ID in the agent's activity and next
   step so it is discoverable from the board.
2. The user-facing coordinator checks the shared board and
   `agent-coord issue needs-info`, then asks the user the exact open question
   in the active conversation. Writing an issue detail file alone does not
   notify the user. When several questions are pending, present them together,
   attributed to their issue and asking agent. Do not overwrite or combine
   distinct questions.
3. After the user answers, append the answer verbatim to the same issue with
   `agent-coord issue quote <issue-id> "<answer>" --supports "answer to the
   recorded clarification"`. Record any interpretation or resulting decision
   separately; never rewrite the original report or question.
4. Broadcast the answer and any new user direction through the shared brain
   with `directive add --source user --via <agent> ...` and, when agents need
   prompt delivery, `send --to all --kind broadcast --issue <issue-id> ...`.
   Every agent can retrieve the full issue with `issue brief <issue-id>` and
   receives the broadcast at its next `sync`/activation. The answer is shared
   state, not private context belonging to the agent that asked.

The user-facing coordinator surfaces questions from every agent, including
agents working on other issues. A question stays open until the answer is
appended and the issue's clarification state is updated. If an answer applies
to multiple issues, preserve it on each relevant issue and broadcast one
linked directive; do not infer that an answer to one question resolves
another.

User messages received directly by any agent follow the same sharing rule:
preserve the exact instruction as a user directive, link it to the affected
issue/task where known, and broadcast it so agents do not depend on private
chat context. Delivery to an idle agent occurs at its next activation or
checkpoint; the coordination brain does not promise to wake an idle agent.

## Cross-agent signal handshake

When an agent encounters evidence that another agent likely owns the work, it
must synchronize before guessing, overwriting, stopping, killing, or reverting
anything. This applies to:

- a build or `changed_source_preflight` failure in a file the agent did not edit;
- a dirty file or active lease with no local explanation;
- an unrelated isolated test run or pinned runtime still active;
- a stale proof, generation, journal, or build identity;
- a conflicting source edit, generated file, or permission error;
- resource pressure that appears to come from another agent's build/run.

Protocol:

1. Capture exact evidence: file path, build attempt ID, run ID, generation,
   journal path, event name, and the first observed failure line.
2. Ask the shared board for likely owners and send one bounded question:

   ```text
   python devscripts/agent-signal.py --agent <me> \
     --path src/owned/file.cpp \
     --build-id <build-attempt-or-build-id> \
     --run-id <run-id> \
     --question "Exact symptom. Is this your work? What is the first failing step?"
   ```

   The helper matches reported activity paths/build/run IDs, asks the likely
   owners with `agent-coord ask`, and broadcasts a response-required message to
   `all` when no owner is found. It never edits source or stops a process.
3. Read `sync`/`inbox`. Record the answer, no-answer timeout, or owner pointer in
   the affected issue or build-friction record.
4. Do not stop, kill, edit, or revert the other agent's work to unblock
   yourself. Continue independent work, switch to a non-conflicting task, or
   record a wait with a reason and a bounded next check.
5. The owner replies with the exact file/build/run, whether the change is theirs,
   the first failing step, and READY/NOT READY. If ownership is unknown, the
   coordinator keeps the signal visible and asks the human.

The normal trigger is automatic: an agent that sees a cross-agent signal uses
the helper before taking further action. Repeated identical signals are
deduplicated by build/run/path context; do not send status-only chatter.

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

Every agent has an independent read receipt for each message. `sync` and
`inbox` deliver that agent's unread messages and record that agent's read time;
reading a broadcast never marks it read for another recipient. The dashboard is
read-only and shows which agents have read each message. `wait` blocks until a
message arrives or the timeout expires.

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

## Proof ledger and completion gate

Every behavior change has a proof record. A proof links the task/issue, changed
paths, source fingerprint, pinned EXE/build ID, game generation, run ID, exact
journal, required event, and verification state. Unproven and stale proofs are
shown on `board`, `status`, and the dev-loop dashboard.

`task status ... done` and `issue close` require a verified `runtime_proven`
proof or an explicit `exempt` proof with a reason. Build-only results are
recorded as `build_only` and do not satisfy the gate. A linked path change marks
the proof stale; history is append-only. See
[agent proof and verification](agent-proof-and-verification.md).

## Build/test permits and self-improvement

An active agent may request a build or isolated test through the root dev-loop
without human approval. The request records the agent, task, issue, permit/build
ID, and run ID. Requests are bounded by PC resources; one EXE build at a time.
A pinned test run uses a specific EXE artifact and game-DLL generation/path and
does not replace the shared runtime. The build queue, rotating build owner,
per-agent/cohort DLL pointers, and preview overlays are specified in
[agent-build-queue](agent-build-queue.md). Runtime registration, health, idle
cleanup, and proof links are specified in
[agent-runtime-registry](agent-runtime-registry.md). Parent/subagent delegation
and cohort pointers are specified in [agent-subagents](agent-subagents.md).
Wake delivery and adapter safety are specified in
[agent-wake](agent-wake.md).

When a build or test waits, the request records the wait reason. An avoidable
wait or a wait above the measured budget creates a `build-friction` issue.
Agents may create an issue for any wait they believe is unnecessary. See
[change-to-evidence-latency](change-to-evidence-latency.md).

Agents are also expected to propose improvements and capabilities, including
coordination improvements a human may assume an AI cannot do. Any agent may
triage, review, reproduce, and gather evidence for or against the proposal. A
human makes the final decision on changes to shared authority. See
[agent-self-improvement](agent-self-improvement.md).

## Dashboard messages

The dev-loop dashboard reads a bounded coordination snapshot and may send a
human message. The snapshot includes agents, activity, open issues, tasks,
recent messages, notes, decisions, active leases, and unread counts. It does
not mark messages read.

A dashboard message uses the sender `human-dashboard` and is written through the
brain. It is not an AI agent session and must not update an agent's activity.
Recipient `all` broadcasts at the next activation/checkpoint; a named recipient
must exist. The message is delivered at the recipient's next activation or
bounded `wait`, not by waking an idle agent.

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
- `edit begin` never bypasses another holder based only on a region label.
  A direct save can replace the full file and erase a logically separate
  region's unsaved work.
- While another agent holds the file, an agent may prepare a repository-local
  patch and send it to the holder without editing the leased source:

  ```text
  agent-coord edit propose <path> --region <name> --patch <patch-file> \
    --summary "what this change does"
  ```

  This stores a proposal and sends the lease holder a `shared.request` message.
  The proposal is not applied automatically. The holder reviews it and may
  apply it after releasing the lease; `proposal apply` checks that the patch
  still applies cleanly. A conflict means the agents reconcile the proposal
  rather than overwrite the current file.
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

Regions must be disjoint. Region names are coordination labels, not enforced
source ranges or protection against stale full-file writes. Accepted shared
work is still applied sequentially under a file lease. An agent that cannot
wait for the current writer can prepare and submit a patch proposal for a
different region; the holder integrates it after releasing the lease. If both
agents need the same region, use a patch proposal and reconcile it explicitly.

If both agents already changed the same file, `shared` conflict handling freezes
the path, shows the working-tree diff, and requires reconciliation. There is no
automatic merge.

## Proposals and competing implementations

`proposal create` stores a patch from one agent for another agent to apply or
reject. Competing implementations remain as alternative patch options. Only the
selected option edits shared files unless the user explicitly approves a
feature-flagged coexistence.

## Architecture change guard

The human has recorded this risk:

> i worry this might be the super quick to jump to hot reloda architecture issue i had beofre, where im changing every file all at once without testing the actual mimita game which lead to a code base that was un reason-about able

This is a risk/watch item, not a confirmed regression. Agents must not treat
architecture work as proven merely because a build succeeds.

- Change one bounded slice at a time.
- Compile it.
- Run the actual game through the dev-loop.
- Require owner events in that executable's `events.jsonl`.
- Keep the previous behavior and spec references.
- Do not bulk-rewrite files or migrate broad subsystems without integration
  proof.
- If the risk actually occurs, add a confirmed regression under
  `docs/regressions/hotreload/`.

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
- A region label alone never permits concurrent direct writes to the same
  shared working-tree file.
- A disjoint patch can be prepared and sent to the active lease holder while
  that lease remains active; applying it still requires a free file lease and a
  clean patch check.
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
## Automatic user-context broadcast

Every user message is recorded once as a `user_context` record with the exact
quote, a content hash, source agent/session, stable source reference, priority,
and tags. The OpenCode prompt plugin performs this automatically; other
runtimes call `agent-coord context add` or the validated relay. The record is
broadcast to `all` as a `user-context` message and appears in `sync` and
`brain_check`.

Normal context is delivered at the next activation. High-priority context also
queues wake requests for active agents. The user message is never rewritten.

An agent that believes a context is attached to the wrong issue runs
`agent-coord context move <context> <issue> --reason "..."`. The correction is
append-only; both owners are notified. If ownership is ambiguous, the agent
opens a `context-routing` issue instead of silently choosing.

## Strict issue intake

Every new issue must contain the exact quote or observation, summary,
reproduction, expected behavior, acceptance, paths, evidence, risk, proof
expectation, and any dependencies/context. `issue add` rejects incomplete
records. `devscripts/agent-issue.py` is the validated front-end; it calls the
same brain and does not create a second issue system.
