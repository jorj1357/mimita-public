# Community browser join freeze investigation

- Task ID: `session-20261119-browser-join-freeze-investigation`
- Summary: Found the community server browser still switches directly to `GAME_PLAYING` before a join is accepted. Checked existing in-game agent mouse commands and additive dev-loop client support. No source change or confirmed runtime fix was made.
- Status: `PASS_WITH_HUMAN_REVIEW` (investigation only; issue remains open)
- Date, time, timezone: `2026-11-19T15:44:03Z` (10:44:03 EST)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base/final commit: `214849f07f9a4bc045d1461b1dbaf463a2084a85`

## Pre-existing changes

The shared checkout was already heavily modified before this session. This session did not edit source, configuration, or regression records. It used the existing dev-loop control path and created this changelog. The current dev-loop build attempt was already active and later reported a coherence rejection because `src/game/server-runtime-module.cpp` changed during the build.

## Findings

- `src/gui/gui-main.cpp`, `GUI_MENU_SERVERS` join handling, still queues the join, hides the server menu, then sets `state = GAME_PLAYING` while a comment says this is temporary until `GAME_CONNECTING` exists. This occurs before welcome, player assignment, map synchronization, or snapshot evidence.
- The networking specification requires initial join/map synchronization to remain **Loading** and reserves **Reconnecting** for a previously established session judged lost.
- `docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md` already records the related eager-gameplay-state failure for the GUI-created local-server path. The community-browser path is a distinct remaining assignment. This session did not prove that it caused the observed Windows hang, so no regression cause/fix claim was appended.
- `src/main-systems.cpp` already registers `agent.mouse_move`, `agent.mouse_down`, `agent.mouse_up`, `agent.mouse_click`, and `agent.input`. No `agent.mouse_scroll` command or in-game screenshot/button-rectangle query was found in the inspected code. The dev-loop can add clients and keeps per-instance journals, but it does not by itself provide screenshot inspection or a repeatable menu-flow runner.

## Runtime actions and evidence

- Before the test, dev-loop status showed run `20261119_152651`, server PID `3240`, and client PID `26928`; Windows reported a `mimita.exe (Not Responding)` window. The existing client journal had stopped recording around `2026-11-19T15:33:55Z`; that alone does not identify the hang cause.
- Requested `python devscripts/dev-loop.py --start-client --server server-01 --name "Agent UI Regression"`. It created run `20261119_153958`, reused server PID `3240`, and launched clients PID `24952` (`client-01`) and PID `25664` (`client-02`). This was not the intended isolated second-client-only result; the prior run's client was not verified as preserved. Do not claim the operation preserved every existing client.
- The new clients wrote their own journals. The observed recent records include render/performance and input-state events, but the dev-loop marked them stale because snapshots were not observed. No GUI create/join flow was exercised and no button-level visual acceptance was performed.
- The selected executable was published build `2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92`; it predates current source. No build request, compiler, or build wait was used. No runtime evidence proves a source correction.

## Validation and remaining work

- Source inspection only; no source edits, build validation, or gameplay fix.
- Preserve the active dev-loop processes. First correct/test the start-client control semantics so an additive run action cannot silently become a new run with an extra primary client.
- Implement the Loading/connect presentation without preventing queued joins from being consumed; test the community-browser create/join flow on a clearly identified client with its own journal and screenshot evidence.
- Add command-driven scroll and a safe screenshot/button-layout inspection surface to the test workflow, then repeat the actual GUI flow and correlate input, connection stages, frame heartbeat, and server/client journals.
- Append to the existing networking regression only after the runtime repro confirms the root cause and a corrected executable passes the flow.
- Human visual review remains outstanding. A claim that the game will never freeze is not supported by finite testing.

## Documents consulted

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/dev-loop/dashboard.md`
- `docs/specs/dev-loop/multi-instance.md`
- `docs/workflows/multi-client-dev-testing.md`
- `docs/specs/networking/networking.md`
- `docs/regressions/README.md`
- `docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md`
- `docs/specs/gui/guiv2.md`

## Focused review skills

- None applied; no source implementation was changed.
