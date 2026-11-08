# Task

- Task ID: fcs-journal-rotation-npc-tick-20261108
- Summary: Investigate FCS slowdown/reconnects, reduce JSONL rotation stalls,
  move NPC utility decisions and bounded timing logs into a hot module, and
  record the remaining measured performance failure.
- Status: Partial fix. Logger rotation copy removed and hot NPC module observed
  live. FCS NPC tick overload, reconnects, and the 240 FPS target remain open.
- Date and time: 2026-11-08T02:22:00Z
- Branch: current working branch; no commit made.

# Workspace preservation

The repository was already dirty, including unrelated streamed-world,
collision, configuration, performance, and dev-loop changes. Those changes
were retained. An isolated copy under the Codex visualization workspace was
used for FCS runtime trials; the existing connected-world run was not stopped
or changed. No production files were edited.

# Changes made for this task

- `src/debug/structured-log.cpp/.h`: keep `StructuredLogger` as the canonical
  writer; serialize JSON before the process writer mutex; assign event sequence
  values in append order; replace synchronous 100 MB segment copying with a
  close/rename/reopen rotation while retaining quota reservation and rollback.
- `src/modules/api/npc-behavior-api.h`,
  `src/npc/npc-behavior-module.cpp`, `src/npc/npc-utility.cpp/.h`,
  `config/modules/modules.json`, and `devscripts/module-build-worker.py`:
  add versioned plain-data NPC utility decisions, server module build inputs,
  and bounded generation-tagged owner timing events through
  `HostServicesV1::log`.
- `src/network/server.cpp` and hot module host sources: activate the server-only
  NPC module at the fixed server tick boundary.
- `docs/regressions/performance/README.md` and
  `docs/regressions/performance/fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md`:
  record the confirmed rotation stall, the partial correction, and the still
  failing NPC/runtime acceptance.

# Build evidence

The isolated FCS dev-loop published these debug builds:

- `2026-11-08T01:33:05.275857Z:939481495520db5fb7a2fd0647e5a284a3aa8fd16b8d11b655f1b2705fcdda59`
- `2026-11-08T01:48:26.634651Z:5bb1a2575b565eb76f0a6a5eff069a86777951ea867dd6c5c1387a054a41d1d4`
- `2026-11-08T02:18:29.299439Z:43a6150bc8266a76455f4acbb65699267f40bd9228d3edcebd2bc39bbead38cb`

The NPC module worker also compiled generation 1 successfully. The latest
short post-build run used build
`2026-11-08T02:18:29.299439Z:43a6150bc8266a76455f4acbb65699267f40bd9228d3edcebd2bc39bbead38cb`.
`git diff --check` and `python -m py_compile devscripts/module-build-worker.py`
completed without errors (line-ending warnings only).

# Runtime evidence

- Server startup journal confirmed Fun Counter Strike, map
  `dust2cyberiav4`, 19 mode-owned NPCs, and `weapon_set_id=30`.
- The live server reported `module.activated` for `npc-behavior` generation 1
  at `before_fixed_server_tick`. The same process wrote bounded
  `npc.behavior.decision-timing` events through its canonical `events.jsonl`.
- Real `agent.snapshot`, `agent.move`, and `agent.shoot` commands completed in
  isolated runs. In the short latest-build trial, `agent.snapshot` and
  `agent.move` completed and server JSONL parsed successfully.
- The 100 MB JSONL segment in run `20261108_014942` was sealed by rename at
  `2026-11-08T02:00:40Z`. The adjacent server tick window reported p99
  `38.508 ms` and zero >100 ms ticks. This did not repeat the prior 870 ms
  copy stall, although the FCS server was already running over budget from
  NPC work.
- Run `20261108_014942` recorded 19-NPC AI work averaging `53.815 ms` in
  `npc.tick-cost`, reaching `102.077 ms`; tick-window p99 reached
  `361.771 ms`, with 90 counted ticks over 100 ms. The client entered
  Reconnecting before the server journal rotated.
- Client one-second frame windows averaged `99.618 FPS` / `13.486 ms`; 37,543
  frames were counted over the 4.17 ms target. The 15-minute connected-client
  workload did not complete because the client disconnected.
- CPU/GPU identity and human gameplay-feel review were not obtained.

# Acceptance

The JSONL rotation stall has a specific correction and a live rotation-boundary
check. The overall slowdown is **not fixed**. The strict server rate, tick
budget, uninterrupted connection, zero frame over 4.17 ms, and 15-minute
connected workload requirements remain unmet. Module utility decision timing
was generally below `0.003 ms`; measured remaining costs are in the broader NPC
decision, navigation, sensing, physics, and containment phases owned by the
host. Continue with short live trials against those measured phases.

# Regression record

Added [fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md](../../regressions/performance/fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md)
and indexed it in the performance regression README. The regression remains
open for NPC workload and client reconnect behavior; the copy-based rotation
is recorded as corrected.
