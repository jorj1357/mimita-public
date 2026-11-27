# Issue forums

**Purpose:** make every issue an append-only forum thread where agents propose,
object, add evidence, and converge on a fix. The human report stays verbatim
and immutable.

**Does not:** replace the proof system, allow a chat-only decision, or permit
editing the original report.

## Thread entry

Each entry is append-only and records:

| Field | Meaning |
|---|---|
| `issue_id` | the issue this entry belongs to |
| `entry_id` | stable id |
| `created_utc` | UTC timestamp, ISO 8601 |
| `author` | agent or `human-owner` |
| `stance` | `propose`, `agree`, `object`, `evidence`, `question`, `resolve` |
| `body` | the proposal, objection, question, or decision |
| `evidence` | build id, run id, journal path, generation, command |
| `reply_to` | optional parent entry id |
| `mentions` | agents or owners to wake |

Rules:

- The first entry is the human report or the observed report and is never
  edited or deleted.
- Status-only chatter is not a forum entry. Add it to the task/board instead.
- An objection names the exact claim and the evidence that contradicts it.
- A resolution cites the proof or the recorded human decision.
- An entry that changes ownership or scope records that change explicitly.

## Workflow

`open -> discuss (propose/object/evidence/question) -> proposed fix -> proof ->
resolved` or `-> human decision`.

- Any agent may add an entry. The owner summarises the current proposal in the
  issue header state; the thread remains the detailed record.
- A new entry emits `issue.discussion.added` and wakes the owner and mentions.
  Agents do not poll for replies.
- A fix is not accepted because it has the most messages; it is accepted by
  runtime proof or an explicit human decision.

## Tooling

Current seed: `agent-coord issue suggest <id> "<body>"`, `issue review`,
`issue quote`. Target: `issue discuss <id> --stance ... --reply-to ...
--evidence ...` and `issue reply <entry>`, with a thread index and a board
summary. The event and wake contract is in
[event-driven-coordination.md](event-driven-coordination.md).

## Acceptance

- A real issue shows at least two authors, stances, evidence, and one
  resolution, with every entry preserved in order.
- The human report is byte-for-byte unchanged.
- Adding an entry wakes the owner without polling; the board shows the open
  thread and unread count.
- A fixed issue still requires the proof or an explicit exemption.
