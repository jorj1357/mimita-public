# FCS JSONL rotation stall and NPC tick overload

Recorded: 2026-11-08T02:22:00Z  
Status: PARTIAL FIX — synchronous segment copy removed; FCS tick overload and
reconnect remain unresolved  
Related specifications: `docs/specs/performance/performance.md`,
`docs/specs/debug-logging/debug-logging.md`,
`docs/specs/npc-behavior/npc-behavior.md`  
Related source: `src/debug/structured-log.cpp`, `src/network/server-npcs.cpp`,
`src/npc/npc.cpp`, `src/npc/npc-utility.cpp`,
`src/npc/npc-behavior-module.cpp`  
Related changelog: `docs/changelog/2026-11-08/20261108_022200-fcs-journal-rotation-and-npc-tick.md`

---

## Expected behavior

Fun Counter Strike should keep the authoritative server near 60 Hz, avoid
long-lived JSONL contention, and keep a connected client responsive while
server-owned NPCs fight and respawn. New diagnostic calls in the NPC decision
owner should be changeable by hot module reload and appear in that process's
canonical `events.jsonl`.

## Reproduction and run identity

Runs used the isolated `fcs-slowdown` dev-loop profile on
`dust2cyberiav4`, Fun Counter Strike, and its 19-actor mode-owned NPC roster.
The mode journal confirms `weapon_set_id=30` from
`gamemode.match.configured`. The client connected to the server's reported
loopback address. The isolated copy was used so the existing connected-world
run and unrelated workspace edits stayed intact.

| Run | Build | Evidence |
|---|---|---|
| `20261108_013349` | `2026-11-08T01:33:05.275857Z:939481495520db5fb7a2fd0647e5a284a3aa8fd16b8d11b655f1b2705fcdda59` | 15+ minute server process; client entered Reconnecting and then ReconnectFailed. Rotation used a synchronous active-file copy. |
| `20261108_014942` | `2026-11-08T01:48:26.634651Z:5bb1a2575b565eb76f0a6a5eff069a86777951ea867dd6c5c1387a054a41d1d4` | Server rotated at `2026-11-08T02:00:40Z` using the rename implementation. Client had already disconnected from NPC tick overload. |
| `20261108_021820` | `2026-11-08T02:18:29.299439Z:43a6150bc8266a76455f4acbb65699267f40bd9228d3edcebd2bc39bbead38cb` | Short post-build live check: module generation 1 activated; `agent.snapshot` and `agent.move` completed; event rows parsed as JSON. |

Canonical journals are under
`C:\Users\guita\.codex\visualizations\2026\11\07\01a1b815-34df-7f01-8654-0d7b6573bf87\mimita-fcs\logs\2026-11-08\` in the matching run directory's `server-01` and `client-01` folders. The logger test includes a rotated `events-0001.jsonl` segment.

## Confirmed regression: copy-based journal rotation

In run `20261108_013349`, the server emitted
`npc.tick-cost` at `2026-11-08T01:43:09.766Z` with `total_ms=880.7145` and
`logger_mutex_hold_ms=870.635`; the logger wait was only `0.021 ms`. The active
JSONL segment had reached its 100 MB limit. `rotateActiveSegment()` copied the
entire active journal to a sealed segment synchronously before truncating it.
The tick thread therefore remained in the journal write path for the duration
of that copy. The client entered Reconnecting at `2026-11-08T01:43:54.688Z`
and reached ReconnectFailed. The timing is consistent with server stalls
contributing to the disconnect; it does not establish that this one rotation
was the only cause of that reconnect.

### Correction

`rotateActiveSegment()` now closes the active stream, renames the completed
segment, and opens a fresh `events.jsonl` while the per-journal lock preserves
append order. It no longer copies the full segment. JSON serialization also
runs before the process writer mutex; sequence values are assigned under that
mutex so the written order remains the sequence order. Quota reservation,
write rollback, and per-segment naming remain in `StructuredLogger`.

At the rename run's rotation boundary, the server window p99 was `38.508 ms`
with zero over-100 ms ticks. This is evidence that the boundary did not repeat
the prior 870 ms stall. It is not evidence that all logger writes are cheap:
the separate per-tick logger hold counter sometimes aggregated tens of
milliseconds across many writes.

## Confirmed remaining cause: NPC work exceeds the tick budget

Run `20261108_014942` contains 478 `performance.server-tick-window` events in
the captured interval. Their p99 average was `71.148 ms`, maximum `361.771 ms`,
and their `over_100ms_count` values sum to 90. The 58 `npc.tick-cost` events
averaged `67.444 ms`, with a maximum of `170.457 ms`; measured `npc_ai_ms`
averaged `53.815 ms` and reached `102.077 ms`. At least one one-second server
window recorded only 25 ticks. The client entered Reconnecting at
`2026-11-08T01:53:33.528Z`, well before journal rotation.

The per-NPC `npc.slow-update` samples identify expensive work inside
`NpcSystem::updateOneNpc()` (`src/npc/npc.cpp`):

- `physics_ms`: average `9.713 ms`, maximum `80.445 ms` across slow-update
  samples.
- `decision_ms`: average `7.449 ms`, maximum `42.619 ms`.
- `navigator_ms`: average `3.814 ms`, maximum `40.213 ms`.
- `sense_ms`: average `3.668 ms`, maximum `43.702 ms`.
- Recovery containment events averaged `10.948 ms` and reached `29.288 ms`.

The new `npc-behavior` module logs its actual generation and decision time.
Observed decisions were generally below `0.003 ms`, so the extracted utility
scoring function is not the dominant portion of the broad `decision_ms`
measurement. The remaining NPC decision, navigation, sensing, physics, and
containment work stays host-owned and needs focused profiling and optimization.

## Client performance and acceptance

In run `20261108_014942`, 502 one-second `PERFORMANCE_FRAME` records averaged
`99.618 FPS` and `13.486 ms` per frame. They counted 37,543 frames over the
4.17 ms budget; 231 sampled windows reported every frame over budget. The
client disconnected before completing the 15-minute connected workload.
CPU/GPU identity could not be retrieved through the available local system
queries. No visual or human gameplay-feel acceptance was performed.

The 59–61 Hz, p99 ≤16.67 ms, zero >100 ms stalls, uninterrupted connection,
zero frames >4.17 ms, CPU/GPU identity, and full 15-minute workload criteria
are **not met**. This entry records the confirmed journal-rotation cause and
partial correction; it does not claim the FCS slowdown is fixed.

## Hot module and diagnostic evidence

The isolated live process reported `module.activated` for `npc-behavior`,
generation 1, at `before_fixed_server_tick`. Its canonical server journal then
contained bounded `npc.behavior.decision-timing` events with actor, goal,
action, `decision_ms`, and generation fields. In the first live run the module
was built and activated while the server process was already running, showing
that the owner-level event call and decision policy can be supplied by a hot
module without moving logger file I/O, physics, or server authority out of the
host.

## Validation limits

- Build: the isolated dev-loop published the three build identities above;
  build success is separate from runtime acceptance.
- Runtime: real agent commands completed in runs `20261108_013349` and
  `20261108_021820`; the longer run `20261108_014942` reproduced NPC overload
  and a client reconnect failure.
- Rotation: one post-correction rotation completed without an 870 ms boundary
  stall, but that boundary occurred after its client had disconnected.
- Human review: not performed.
- No synthetic gameplay test was used.
