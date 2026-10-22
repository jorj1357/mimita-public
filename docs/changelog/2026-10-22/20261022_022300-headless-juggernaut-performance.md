# Headless Juggernaut performance measurement

Time: 2026-10-22T02:23:00Z (2026-10-21 22:23:00 EDT)

Branch: `20261011zt`
HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a`

## Result

Added 60-frame performance-window measurements for p95/p99 frame time, the
configured FPS budget, frames over budget, main-thread CPU time, and optional
GPU timer data. Enabled the existing budget-config loader at frame startup and
fixed its lookup to select the root `performance` object instead of the logger
category with the same name. Set the configured measurement target to 240 FPS.

The dev-loop launched build 2758 headless in Juggernaut on
`dust2cyberiav4`. `dev-loop.launch.visibility` confirmed every client window was
hidden. `versioninfo.executed` identified the exact executable and journal:
`C:\mimita-v9\.dev\builds\2758\mimita.exe` and
`C:\mimita-v9\logs\2026-10-22\20261022_021917\events.jsonl`.
The live roster journal confirmed 68 entities (67 NPCs plus one player) and no
empty NPC avatars.

## Measured outcome

The final 60-second slice contained 12 resource windows and 720 frames, all
over the 4.17 ms 240 FPS budget. Median window-average frame time was 53.79 ms;
median window p95 was 90.36 ms. Median main-thread CPU time was 49.22 ms per
frame. GPU timers were disabled for this final slice to limit measurement
overhead. The most expensive client scope was `Replay`, which includes
`Simulation::SimulateTick`; ragdoll corpse updates also varied substantially.
The server reported 67 NPCs and median tick time of 10.32 ms across the final
minute's windows.

These measurements show no performance improvement: this change instruments
and corrects the target budget; it does not optimize gameplay. The logs measure
main-thread CPU, not all threads, so they do not establish total multicore use.
Do not add Replay and its nested simulation scope together when attributing
cost.

## Validation

- Dev-loop debug build 2758 published successfully; build output reported one
  compiled translation unit and 563 skipped units. `src/perf/perf.cpp`'s
  object timestamp was newer than its source and the published executable.
- Exact `--versioninfo` preflight recorded the executable, build date/time,
  working directory, and `EVENTS_JSONL_PATH` in the live journal.
- Headless runtime was observed in the live journal; client and server build
  identities matched.
- The budget-config event reported `target_fps: 240`; the resource windows
  reported 240 FPS as the target.
- No regression was filed because no previous 240 FPS performance baseline is
  established. No gameplay optimization or visual acceptance is claimed.
- Existing unrelated modified and untracked files were preserved.

## Follow-up: warmed 68-actor run and hitch reduction attempt — 2026-10-22

The existing release build 2768 stayed in the headless Juggernaut scenario on
`dust2cyberiav4` with 68 total entities (one player and 67 NPCs). Its canonical
journal is `logs/2026-10-22/20261022_031006/events.jsonl`.

The latest rolling 60 seconds in that journal covered 22,560 rendered frames
across 376 60-frame windows. Average frame time was 2.496 ms (about 401 FPS),
but 1,974 frames exceeded the 4.17 ms budget: 8.75%, above the requested
less-than-1% limit. The worst frame was 37.42 ms. Thus average throughput met
240 FPS after startup, but stable minimum 240 FPS did not. In sampled worst
windows, `Replay` / `Simulation::SimulateTick` and `Setup::ConfigPolling` were
associated with the largest spikes. The server's latest sample had 67 NPCs and
1.28 ms average tick time; earlier windows varied up to 4.57 ms.

Added per-stage client network scopes (`Network::ReceivePoll`,
`Network::PacketDispatch`, and `Network::PostReceive`) to identify the work
inside the previously broad `Network::MpTick` measurement. Changed configuration
hot-reload polling from every 10 rendered frames to a 167 ms wall-clock interval,
so a high render rate no longer increases filesystem checks. This preserves
hot reload while limiting polling to about six checks per second. These source
changes have not been measured in a fresh runtime yet.

The dev-loop build first failed because the performance-scope macro reused one
local variable name for scopes in the same function. The post-receive scope was
moved into its own block. A direct release `build.py build-only release`
completed successfully (1 translation unit compiled, 563 skipped), which proves
compilation only. The dev-loop control endpoint then stopped accepting requests
while its state still reported build 2768; the changed executable was therefore
not published through the dev loop, and no post-change journal exists. The
existing 2768 session remained running. Do not claim the hot-reload polling
change improved runtime until a fresh dev-loop run measures it.

The performance-scope deduplication also removed the prior 1,024-entry capture
saturation: recent windows reported zero saturated frames. This corrected the
profiler's visibility; it is not a gameplay performance improvement.

## Follow-up: visible Juggernaut validation — 2026-10-22

The old headless client's journal stopped at 03:22 UTC and the dev-loop control
thread stopped responding. After verifying their exact process identities, the
frozen task-run client and unresponsive daemon were stopped; the existing
watchdog restarted the daemon. The external server was then stopped through the
dev-loop control path so the next client and server shared a fresh build.

Release builds 2770, 2772, 2773, 2774, and 2775 were published through the
dev loop and run in visible Juggernaut mode on `dust2cyberiav4`. The exact
build-2775 executable was also checked with `--versioninfo`. Its live run is
`logs/2026-10-22/20261022_033448/events.jsonl`; server and client build IDs
matched. The client received 68-entity snapshots: one player and 67 NPCs.

The initially visible build-2770 run missed the 4.17 ms budget on all 2,040
frames in its final rolling minute (29.142 ms mean). Per-scope logs implicated
remote-entity updates, simulation, and actor rendering. The per-frame spike
report and per-snapshot NPC-avatar diagnostic were bounded to once per second;
the independent frame-budget counter still counts every miss. New scopes split
post-receive networking into remote entities, avatar, timeline, sword, and
projectile work. A remote-entity hot loop also stopped copying the entire
weapon registry once per actor per frame and now reads the existing registry
by reference. Later runs were faster in some windows, but scene activity and
view differed, so the before/after observations do not establish a controlled
performance gain from any one edit.

A once-per-second client viewport event now reports remote NPCs, living NPCs,
loaded models, and NPC centers inside the camera projection with nonzero
velocity. The rendering category was disabled, so this performance diagnostic
uses the enabled performance category. Build-2775's latest complete rolling
minute contained 3,840 frames and 3,840 budget misses (100%), with 15.368 ms
mean frame time and 675.41 ms worst frame. In the same minute, the highest
camera-projection count was 47 of 67 remote NPCs, with 47 moving. Projection
does not prove that those models were unobstructed or visible to a human.

The requested result is **not achieved**: this run did not place 68 moving
actors on screen and did not sustain 240 FPS. Recent scope windows still put
actor rendering near 5 ms per frame before the rest of the game is counted.
The current scenario spreads actors around the map and the local player is the
camera, so an explicit 68-visible-actor staging scenario and a rendering-path
optimization are still needed. No human visual acceptance is claimed.

## Follow-up: render logging and body scopes — 2026-10-22

The actor render owner built and serialized a complete render-state JSON object
for every remote actor on every frame even when its rendering log category was
disabled. It now checks the category before constructing that payload. Healthy
actors also constructed ragdoll-only JSON payloads in both the render wrapper
and physical-body draw path; those calls are now guarded by the existing dead
and ragdoll state. Added bounded body, outline, and cosmetic draw scopes. The
build-2777 scopes showed player body drawing around 2.2–3.2 ms per 60-frame
window, cosmetics around 0.3–0.4 ms, and no outline draw time in sampled windows.

Dev-loop release build 2778 compiled and published, then ran visible Juggernaut
with matching client/server build IDs. Its journal is
`logs/2026-10-22/20261022_034032/events.jsonl`. The latest complete rolling
minute ended at 03:41:53 UTC: 4,920 of 4,920 frames exceeded 4.17 ms, mean
12.147 ms and worst 1183.35 ms. The client still received 68-entity snapshots,
but the maximum camera-projection count was 40 remote NPCs, of which at most
39 were moving in view. Later combat and view composition differed from earlier
runs, so no controlled FPS gain is claimed. The 240 FPS and 68-visible-actor
acceptance criteria remain open.
