// 2026-11-27
/* purpose
* make every user-visible agent statement readable by every active agent
* make user context enter the brain once with a stable source reference
* replace private side-channels with one append-only broadcast model
* this file DOES NOT give every agent authority to act on every message
* this file DOES NOT replace issue discussion or proof ownership
*/

# Agent broadcast and shared context

**Status:** Desired behavior contract.

## The rule

> If an agent says something to the user, every active agent can read it.

> Every user message enters the shared brain exactly once.

There are no private agent-to-user side conversations that change shared work.
The chat is the user's interface to one agent; the brain is the shared record
for all agents.

## Channels

| Channel | Audience | Durability | Use |
|---|---|---|---|
| `user_context` | all agents | append-only | verbatim user message, once |
| `announce` | all agents | append-only | decision, finding, blocker, resource event |
| `issue` | issue owner + participants | append-only | debate, evidence, proposal |
| `status` | board/projections | current state | activity, phase, wait reason |
| `question` | named agent or all | durable until answered | bounded request for evidence |
| `reply` | question sender + all | append-only | answer with evidence |

A message that changes shared work MUST go to `announce`, `issue`, or
`user_context`; a private chat summary is not sufficient.

## User context

Every user message is stored once:

```json
{
  "context_id": "ctx-...",
  "source": "user",
  "session": "ses_...",
  "source_ref": "<stable message id>",
  "quote": "<verbatim text>",
  "created_utc": "...",
  "linked_issue": "issue-...",
  "priority": "normal | high"
}
```

Rules:

- The quote is immutable. Never delete or rewrite it.
- The same message is not stored twice; `source_ref` is unique.
- If the context is linked to the wrong issue, `context move` records a reason
  and notifies the affected owners.
- High-priority context wakes active agents; normal context is delivered at the
  next activation.
- An agent response does not create a new user context.

## Announcements

An announcement is for information another agent needs to avoid duplicate work
or a wrong assumption:

```text
I am changing the crouch capsule in player.cpp; do not edit it.
Generation 801 is proven for crouch; use run 20261127_180833.
The build is waiting on memory; stop idle clients before requesting.
```

Rules:

- State the fact, the evidence, and the affected paths/issues.
- Link an issue when the announcement has evidence.
- Do not post status-only chatter.
- An announcement wakes the issue owner and any agent whose lease covers the
  named paths.
- An announcement does not grant permission; edit leases still decide writes.

## Questions and replies

- Ask one bounded question with exact evidence: path, build ID, run ID, event,
  or line.
- The asker does not sleep. It subscribes to `reply` and continues other work.
- One short window is allowed as a failure detector; no reply means the asker
  records the timeout and proceeds carefully.
- A reply quotes the evidence and names the answer, not just "yes".

## Brain check

Every coordination `send`, `ask`, `reply`, `sync`, and activation returns a
fresh brain check:

- the current issue and its participants;
- unowned claimable work;
- active leases and halts;
- recent announcements affecting the agent's paths;
- unread replies and user contexts.

An agent that ignores the brain check is working from stale context.

## Projections

The taskbar, dashboard, board, and chat summaries are projections of the same
records:

- a taskbar window shows run, mode, owner, issue;
- the dashboard shows leases, waits, builds, and runs;
- the board shows activity and next steps;
- the issue shows the append-only debate.

No projection is a second source of truth.

## Anti-patterns

- Telling the user something in chat that other agents need and never recording
  it in the brain.
- Editing the same file after another agent announced it without an edit lease.
- Posting "working on it" with no fact, evidence, or path.
- Asking a question and then sleeping instead of subscribing.
- Storing the same user message under two context IDs.

## Acceptance

- A user message appears once in `user_context` with the verbatim quote.
- An announcement is visible to every active agent at its next brain check.
- A question has a durable record and a reply event.
- The taskbar and dashboard show the same run, owner, and issue as the brain.
- No shared-work decision exists only in a private chat.
