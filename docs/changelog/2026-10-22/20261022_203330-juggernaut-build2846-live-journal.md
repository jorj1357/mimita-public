# Juggernaut build 2846 live journal inspection

- Time: 2026-10-22T20:35:52Z
- Branch / HEAD: `20261011zt` / `cbfa4b0df4e56b6f217c45dbe986b49df599b50a`
- Result: new dev-loop run completed an approximately one-minute journal sample; 240 FPS failed and the client later became unresponsive.

## Build and run identity

- Canonical build request: `python devscripts/dev-loop.py --request build --wait-for-build --build-mode debug --json`; dev-loop reported success and published build 2846.
- Build output said `Nothing changed`; build 2846 SHA-256 `8e1c250ec7b1932ea9f263d554a8f28ef6fdd063eaf5f1fb9beb42df0f8bae92` matches build 2845. This was a fresh launch of the same binary content, not newly compiled gameplay code.
- Exact `--versioninfo` identity probe: `.dev/builds/2846/mimita.exe`, PID 18076, run `20261022_163254`, identity-probe journal `logs/10-22-2026/20261022_163254/events.jsonl`.
- Real dev-loop run: `20261022_203006`, mode 9 Juggernaut, `dust2cyberiav4`, visible client, matching client/server build 2846, source generation 30.
- Runtime journal: `logs/2026-10-22/20261022_203006/events.jsonl`.

## Runtime findings

- 143 client resource windows covered 8,580 frames from 20:30:11Z to 20:31:12Z: average 7.169 ms/frame, about 139.5 FPS by summed frame time, 6,642/8,580 frames (77.4%) over the 4.17 ms budget, main-thread CPU 6.813 ms/frame, maximum frame 4,911.14 ms. The worst 60-frame window averaged 94.59 ms and missed budget on all 60 frames.
- After the second roster build, 93 windows/5,580 frames over about 46 seconds averaged 8.236 ms/frame (121.4 FPS), with 5,215/5,580 frames (93.5%) over budget and main-thread CPU 7.983 ms/frame. This is not a full one-minute post-roster acceptance run.
- Server tick windows report 67 NPCs plus one player. Roster-profile events contain 126 Fighter and 8 Juggernaut records from two 63+4 roster builds. The human's Fighter assignment was not directly confirmed in this journal. One client viewport event reported 67 remote NPCs, 38 in view and moving. Movement-decision events sampled 17 distinct NPC IDs, so continuous movement of every NPC is not proven.
- Latest sampled server tick averaged 3.51 ms (14.72 ms maximum) with 67 NPCs and one player; server loop remained near 60 Hz. This does not explain the measured client main-thread over-budget result by itself.
- In the worst frame window, main-thread CPU averaged 89.84 ms per frame while frame time averaged 94.59 ms. The outer `Rendering` scope recorded 4,925.69 ms self time and 7,184.4 MiB of global allocation deltas. The window-wide global allocation delta was 7,247.38 MiB. Global allocation counters include all threads, so render ownership of those allocations is unproven; GPU timer queries were disabled. The render scope is the strongest time-localization clue, but its uncaptured child work and the stall source remain unknown.
- The journal contained 13,641 events and reached about 12.7 MB. Top events included 2,522 `npc.pursuit-state`, 1,939 `npc.target-changed`, 1,255 `anim.state.changed`, and 1,101 `npc.stuck-episode`; event volume is notable but not established as the frame-time cause.
- Journal stopped at `2026-10-22T20:31:13.259Z`. The client PID 26056 remained present. Windows reported `Responding=False` during one poll, then `Responding=True` in a later poll; working-set/private-byte readings also varied substantially (1.43/22.62 GiB, then 6.03/15.59 GiB, then 4.22/15.57 GiB). At the latest poll, the server PID 32296 also existed and responded; the host had 1.17 GiB physical memory free. The client stopped updating its journal even though it later responded to Windows. This supports a severe client stall with memory pressure, not a confirmed process crash or identified leak. No exit code or crash dump was captured.
- No `WeakConnection`, disconnect, or crash event was recorded in the captured run; only the initial `Connecting` to `Connected` transition was logged. This reproduces severe stalling/unresponsiveness but not the reported 27-second freeze followed by a crash.

## Work and limits

- No gameplay source was edited. No tests were run. A new debug build was published and launched through the existing dev-loop as requested; the process was observed but not terminated.
- The host reports 12 logical processors. A whole-system CPU reading near 7% can therefore represent roughly 0.84 fully busy logical processor; the journal’s frame-window main-thread CPU measurements are a more direct signal than the total percentage. No per-thread profile was captured.
- Performance acceptance is not met. The post-roster window is shorter than 60 seconds, GPU cost is unknown, per-thread allocation ownership is not measured, and the exact cause of the multi-second render-scope stall/client memory growth remains unproven.
- Pre-existing working-tree changes were preserved.
