# Human action provenance handoff

- Task ID: `work-e1128df8e941`
- Session ID: work-e1128df8e941
- Agent session: `session-506b5daa3b4c`
- Owner: `resource-profiler`
- Date: 2026-11-24 UTC
- Related issue: `issue-70cbf4d13c02`
- User context: `ctx-e6fa980a577e`
- Status: Proposal recorded; instrumentation not implemented

## Request

> how to make it so  when i do things as human its marked as a thing  so it snot like out of nowhere a rpobelm

## Finding

The coordination user-context bus stores user messages, but it does not observe a direct click in Task Manager or an external action in the game. Runtime journals can show mode transitions when the owner emits an event; they do not prove who initiated a transition unless the event carries actor/provenance data.

## Proposal recorded

Add an append-only human-action marker with UTC timestamp, `actor=human`, action, target/PID, before/after values when observable, run/build/generation identifiers, and evidence path. Automatically capture actions already routed through dev-loop APIs; provide a quick manual marker for external actions without a trustworthy observer. Preserve unknowns and do not treat timing correlation as causation. See issue `issue-70cbf4d13c02` for the full proposal and acceptance.

## Files and coordination records

- `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md`: appended the proposal at line 110; the issue already had no action-provenance entry at this location before this session.
- Shared coordination brain: recorded the exact user quote once as `ctx-e6fa980a577e` and linked it as related context to `issue-70cbf4d13c02`.
- This changelog records the handoff; no game source, build, or runtime state was changed.

## Validation and limits

- Read the context-contract and coordination specifications and confirmed the distinction between user-message records and external action observation.
- Confirmed the new quote and proposal are present in the shared issue record.
- No action-marker implementation, build, or runtime proof was performed. The next step is to add the marker at the dev-loop action boundary and specify the lightweight manual fallback for actions outside that boundary.


