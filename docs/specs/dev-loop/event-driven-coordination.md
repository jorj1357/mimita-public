# Event-driven coordination (no timer waiting)

**Purpose:** replace clock-based waiting in agent coordination with observable
events, so a build request, approval, issue debate, or wake happens as soon as
the state changes instead of after a sleep.

**Does not:** add a second build authority, weaken proof requirements, or let a
requester approve its own build except under the recorded human no-response
rule.

## Principles

1. State changes emit events. Agents subscribe or are woken; they do not poll.
2. A wait blocks on the coordinator's event stream, not on a timer.
3. Each request is a durable record with a state machine, not a chat message.
4. A timeout is diagnostic only. It never grants approval.
5. Append-only events are the audit trail; the current state is a projection.

## Build events

```text
build.requested      request id, agent, kind, paths, issue, expected seconds
build.approved       request id, approving agent/human, reason
build.rejected       request id, rejecting agent, reason
build.queued         request id, queue position
build.started        request id, generation, attempt id
build.finished       request id, status, generation, postmortem path
build.postmortem     why, phase seconds, slowest TUs, queue wait, resource peak
```

- The build owner (or the human) emits `build.approved` / `build.rejected`.
- The requester waits on `build.approved`/`build.rejected` for its request id;
  `build wait <request> --event` is event-blocking, not a sleep loop.
- One build may be approved, queued, or building at a time. A second request is
  amended into the current batch or rejected with that reason.
- The dev-loop consumes only an approved request (`--request agent-build
  --build-request <id>`), or a recorded human/owner seal.
- If no response event arrives after one ask, the requester records
  `no_response` and seals the single build. This is the human-authorized
  no-response exception; it is not general self-approval.

## Issue events

```text
issue.discussion.added   issue id, entry id, author, stance, mentions
issue.resolved           issue id, resolution, proof/exemption
issue.reopened           issue id, reason
```

- Adding a discussion entry wakes the owner and mentioned participants.
- A resolution requires evidence or a recorded human decision; the last message
  is not a decision.

## Acceptance

- A build request goes from proposed to finished through approval/finish
  events without any `sleep`, polling loop, or fixed timeout.
- An issue discussion entry wakes its owner without the owner polling.
- The board shows the current state, unread entries, and the blocking reason
  for every wait.
