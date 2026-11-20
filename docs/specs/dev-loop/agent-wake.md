// 2026-11-20
/* purpose
* specify how an idle agent is woken for actionable issue work
* make wake delivery model-agnostic and safe against duplicate/runaway activation
* this file DOES NOT grant build, policy, or process-kill authority
* this file DOES NOT promise a human-visible result without runtime evidence
*/

# Agent wake

## Purpose

An agent only acts when it is running a turn. `agent-coord next` can identify
work, but something must start the agent activation. `agent-wake.py` is that
adapter.

## Wake-check contract

```text
agent-coord wake-check --agent <name>
```

Returns:

```text
wake: yes | no
exit_code: 0 | 10 | 2
issue
task
next_step
context bundle
blocked reason
```

Exit codes:

- `0`: actionable unblocked work
- `10`: no actionable work
- `2`: blocked, halted, frozen, or needs user input

The context bundle uses the issue `observe` view: issue, tasks, quotes, reviews,
evidence, activity, build identity, runtime state, and recent coordination
messages.

## Wake conditions

Wake only when:

- an issue/task exists
- it is unblocked
- it has an owner or is claimable by the target agent
- no lease on the same work is held by another agent
- no duplicate wake is active for the same issue
- halt/freeze is clear
- the wake rate/cooldown allows it

Do not wake for deferred, needs-clarification, done, or warning-only work.

## Adapter

`devscripts/agent-wake.py` provides a durable request queue and model adapters:

- opencode
- codex
- generic command

A wake request records:

```text
wake id
agent
issue
adapter/model
condition
prompt
status
attempts
child pid
log
created/launched UTC
```

Conditions may be:

- build published
- build failed
- journal event
- timeout
- manual

The adapter creates a bounded prompt and starts one child process. It never
starts a second dev-loop or compiler.

## Safety

- one active wake per issue/agent
- respect issue ownership and leases
- respect halt/freeze
- cooldown after failure
- bounded attempts
- preserve the prompt and child log
- no human approval required for a bounded investigation activation
- no self-granted build/merge authority
- human can pause or disable the adapter

An agent woken without human approval may investigate and produce evidence.
It must not self-approve a critical Class C change; see
[agent-self-improvement](agent-self-improvement.md).

## Relationship to `next`

`next` answers “what should be done.” Wake answers “start a turn to do it.”
`next` is safe to call manually; wake is the automated delivery path.

## Message-triggered brain checks and issue contributions

`sync`, `send`, `ask`, and `reply` return a bounded `brain_check` containing
the agent's current issue, its next suggested work, and up to five claimable
issues. The agent reviews this result before continuing. Sending a message does
not assign work automatically; an agent that chooses to own an unassigned issue
uses `agent-coord next --claim`.

Issue-related messages accept `--issue <id>`. When a subject or body names one
existing issue ID, the coordinator also detects and links it automatically.
Linked message bodies are appended to that issue's append-only discussion and
suggestions section. Agents can add a standalone proposal with:

```text
agent-coord issue suggest <id> "<suggestion>"
```

Contributions should identify whether they are a suggestion, evidence,
question, or objection. Agents should not append unrelated status messages or
create duplicate issues when an existing issue fits.

## Acceptance

- A human can see actionable work and the wake state.
- An idle agent can be started with the exact issue and next step.
- Duplicate wakes for one issue are prevented.
- Blocked work does not wake an agent.
- Every wake records its condition, adapter, child PID, status, and log.
- A failed wake records why and backs off.
- A human can stop automatic wake without stopping the dev-loop or game.
- `sync`, `send`, `ask`, and `reply` surface current/claimable issue context.
- Issue-linked message bodies and standalone suggestions are durably appended.
- No issue is assigned solely because an agent viewed or discussed it.
