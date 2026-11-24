# Damage backlog and atdm snapshot investigation

## Scope

Captured the user-confirmed delayed damage presentation in run `20261123_235443` and the follow-up TDM/atdm disconnect and NPC snapshot freeze report. No gameplay source was changed.

## Damage evidence

- Pinned run `20261123_235443`, mode Juggernaut Horde on `dust2cyberiav4`, build `2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca6ca16ca189d512b`.
- Client snapshot tick reached 41180 while confirmed damage presentation was still processing ticks 21101/21102, about 5.6 minutes behind at 60 Hz. Journal: `logs/2026-11-23/20261123_235443/client-01/events.jsonl`.
- Code path sends one reliable confirmed-damage event per damage result and presents each individually. The local pending-victim-health queue is capped at 500 ms, so it cannot alone explain the minutes-long backlog.
- User chose to keep authoritative server ticks and batch client catch-up. Issue `issue-33dd666c3008` records this direction. No source edit was made because the relevant damage files are already dirty and another agent had not confirmed ownership.
- No matching lightning/thunder sound asset was found in the inspected asset inventory; sound reuse remains unconfirmed.

## TDM / atdm evidence

- Run `20261124_001546`, TDM NPC Elite on `atdm`, same stale build; `run.json` records source generation 4 and build mismatch.
- Client snapshot applications had a roughly 149.743-second gap. Server continued ticking; atdm snapshots were two chunks, about 1.7 KB, for nine NPCs. A client chunk timeout was recorded.
- After the gap, telemetry showed `Connecting`, zero packets received, and zero applied snapshots while the server player count was zero; reconnect requests/accepts repeated. Dev-loop status at 00:34 UTC showed server online and no active client PIDs.
- Server NPC phase windows near 00:34:27–00:34:38 reported 9 NPCs and roughly 0.51–0.72 seconds aggregate work per one-second window. This is meaningful workload, but does not establish why the client stopped receiving.
- Evidence favors investigating reconnect/receive recovery separately from map streaming. The first missing reconnect packet/state transition is not proven, and current source-generation mismatch prevents attributing the behavior to current code.
- The user context and evidence were attached to existing issue `issue-453d0596e294`; no duplicate issue was created.

## Validation and remaining work

- Read-only source/journal inspection only; no tests or builds requested or run.
- Both runs are pinned to the old build. Runtime behavior after a source change remains unverified.
- Network implementation was not edited because the relevant shared files are already dirty and ownership inquiries returned no response during this investigation.
- Next evidence should use the root dev-loop with a current build, identify the first missing stage from reconnect accept through map-ready and first fresh snapshot, then compare atdm against a lighter map under matched NPC/player load. Damage fix should coalesce stale client presentation while preserving server-authoritative 60 Hz damage timing.

## Cross-thread journal follow-up (2026-11-24)

- Verified rotated `client-01/events-0002.jsonl` contains a second server process, PID 19132, started at 00:19:25Z on `dust2cyberiav4` / TDM. The process wrote its server events into the client instance journal.
- During this period the manifest-owned server PID 5192 remained the atdm server; it had zero connected players. The second server later recorded two players and active snapshots/NPC simulation. This makes an in-game server/session transition a plausible lead for the apparent freeze/disconnect and sparse client journal.
- The transition's initiating action and process/session relationship are not proven. Do not attribute the report to map chunk loading from this evidence.
- Separate atdm performance evidence includes a 571.5 ms NPC phase in one server tick (tick 77548). The independent dust2cyberiav4 server later ran near 58 Hz with active snapshots. Keep the atdm workload stall separate from the client/server session transition until the initiating path is traced.
- Added this cross-thread lead to existing issue `issue-453d0596e294`; no source change or runtime intervention was made.
