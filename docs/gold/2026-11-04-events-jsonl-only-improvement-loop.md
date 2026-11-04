// 2026-11-04T11:20:00Z
/* purpose
* record the preferred improvement loop centered on the live executable and events.jsonl
* make speed, runtime evidence, and the first divergence the default priorities
* remove or demote work that does not feed that loop
* this file does NOT claim every task has only one valid tool
* this file does NOT authorize skipping runtime safety, secrets rules, or human acceptance
*/

# Gold direction: events.jsonl is the loop

- Timestamp: `2026-11-04T11:20:00Z`
- Audience: AI coding agents and the player
- Status: preferred working direction for gameplay, bug, and behavior work
- Related: `docs/specs/debug-logging/debug-logging.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/gold/2026-09-21-event-driven-performance-debugging.md`

## The direction

The strongest, fastest, and most trustworthy way to improve MiMITA is to work through
the live `mimita.exe` and its `events.jsonl` journal. One loop, repeated:

```text
1. Get the idea or the bug.
2. Implement the code.
3. Compile and link.
4. Put it into a running mimita.exe.
5. Check that the behavior actually happened the way we specified.
```

Step 5 is concrete, not a vibe. Examples:

- The vehicle should move forward when commanded.
- Its wheels should show real contacts with the ground.
- Its speed, position, and contact state should appear in the journal.
- A human CT should be able to hold F and defuse.
- NPCs should plant, recover, defend, and defuse as specified.
- Grenades should spawn, tick, and expire at the specified times.

If step 5 is not what we want, the loop continues:

```text
use the live journal as evidence
-> find the first divergence between expected and actual
-> add or improve the owner-level logging that exposes it
-> rebuild through the dev loop
-> run the same real behavior again
-> compare the new journal
```

That is the whole method: trial, error, evidence, and repeat. The journal is the source of
runtime truth. Code is our current explanation of how to make the result happen.

## Anything outside this loop should go away

The user's direction is to remove or demote work that does not make the loop faster or more
truthful. In practice:

- Do not make an agent quality gate the everyday completion ritual. The live executable and
  its journal are the completion evidence for behavior work.
- Do not create a scenario or synthetic self-test as the first response to a user-visible
  behavior problem. Reproduce with the real client/server and read the real journal first.
- Do not add a second debug file, a parallel status dump, or a hand-written evidence summary
  when the canonical journal can carry the record.
- Do not spend time on broad matrices, speculative plans, or unrelated checks before the
  first divergence is visible in the journal.
- Do not keep logging that buries the signal. If an event fires thousands of times without
  changing a decision, remove it, aggregate it, rate-limit it, or move it to trace level.
- Do not treat a successful build, a green status line, or a passing harness as proof that
  the behavior happened. Only runtime evidence proves runtime behavior.

Keep the safety invariants. Do not expose secrets, do not edit production directly, do not
delete active evidence, and do not claim visual, multiplayer, or human acceptance without
observing it.

## Speed rules

- Reuse the already-running dev loop. Build, publish, launch, and send commands through it.
- Keep exactly one build active. Do not start competing builds or a second daemon.
- Prefer the smallest owner-level patch that can change the observed behavior.
- Add the smallest bounded event that names the first divergence. Then rebuild and look again.
- Compare the same behavior before and after. Do not change three things at once.
- Delete duplicate or obsolete paths when the replacement is proven.
- Keep the journal small enough to read: one canonical file, useful events, bounded repeats.

## What good evidence looks like

A useful journal entry names the owner and the values that decided the outcome. It is written
where the behavior is owned, not reconstructed later by a tool. For a bug, the first useful
record shows:

- who acted;
- what they asked for;
- what the authoritative owner accepted or rejected;
- the exact expected and actual values;
- the tick or time;
- the reason for the first divergence.

Good example from the Fun Counter Strike work: a human holding F produced no defuse. The
owner-level event `objective.interaction-rejected` named the actor, team, distance, range,
designated defuser, and reason. The fix then produced `objective.defuse-start` with
`operator_kind=player` and `objective.defused` in the same live journal.

Good example from the NPC stall work: `performance.server-tick-stall` named the tick and the
stage totals; `npc.tick-cost` narrowed it to NPC AI; `npc.slow-update` narrowed it to
`navigator.update`; then one wasteful extra-route calculation was removed and the slow-update
count fell from roughly 122 in 70 seconds to 16 in 75 seconds.

Bad evidence is a build success message, a green checkmark, a hand-waved "should work", or a
scenario that does not run the real behavior the player cares about.

## Completion

An edit is complete when the live executable produced the specified behavior and the journal
shows it. Human visual or gameplay acceptance remains a separate claim and must be observed
directly. Build, runtime, and human review are different kinds of proof; report them separately.

This direction is about speed through better truth, not speed by skipping reality.
