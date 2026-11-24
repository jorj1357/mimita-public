# Agent work-time issue expansion

Date: 2026-11-23
Agent: `agent-efficiency-evidence-loop`
Issue updated: `issue-89c11130189a`

## Change

The user's new request was linked verbatim as context `ctx-ac43c81863bc` to the
existing issue “Track agent work-time and phase latency.” No duplicate issue
was opened because that issue already owns agent phase timing and has four
open implementation tasks.

Appended a passive investigation proposal connecting that issue to:

- `issue-136d4553ff92` for DLL build stages and fanout;
- `issue-7d3dcfd7433b` for end-to-end wait latency;
- `issue-a8772368be34` for oversized coordination responses;
- `issue-91e3384b219b` for periodic independent reviews and convergence.

The proposal records historical baseline evidence from the issue's independent
review: 27/27 sampled work sessions had only a `received` phase, 0/27 had
issue/task linkage, and the reviewer identified one manually phased session
with 2,236,437 ms across received/planning/editing/review. This is historical
evidence and is explicitly marked for remeasurement.

The proposal treats coordination latency as a hypothesis. It asks for a
passive schema/provider audit and matched baseline across coordination
send/answer, tool/edit operations, build queue/compile/link, tests, handoffs,
unattributed time, and resource usage. It distinguishes observable wall-clock
activity from hidden model reasoning; no chain-of-thought content or inferred
thinking durations are proposed for collection. It also defines a repeatable
claim/hypothesis/falsifier/experiment loop, preserves negative results, and
keeps shared-policy decisions under human control.

## Validation and limits

- `agent-coord issue readiness issue-89c11130189a` reports ready=true with no
  missing fields and four existing tasks.
- No source, spec, build, or runtime changes were made. The passive baseline
  investigation and any telemetry implementation remain open work.
- The existing issue-review records show a phase-attribution gap, not proof
  that coordination is the dominant source of elapsed time. Fresh samples are
  needed to establish that claim.
