// 2026-11-20T03:35:00Z
/* purpose
* record the pinned Juggernaut baseline, dashboard presence policy, and first
* remote-actor render optimization attempt
* separate live runtime evidence from source-only/pending-build changes
* this file DOES NOT claim 60 Hz or 240 FPS acceptance
* this file DOES NOT claim the pending culling build is published
*/

# Juggernaut baseline, dashboard presence, and remote culling

## Task

- Task ID: session-20261120-60hz-baseline-dashboard-presence-remote-culling
- Owner: coordination-v2
- Focus: pinned 67-NPC baseline, dashboard keep-open policy, client render
  budget, build-friction evidence, coordination handoffs
- Status: baseline observed; dashboard policy implemented/tested; culling
  source saved but not built or proven

## Pinned baseline

- Run: `20261120_032618`
- EXE artifact: `2026-11-20T02:29:08.406738Z`
- Build ID: `2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755`
- Game generation: `390`
- Launch mode: `9` Juggernaut, map `dust2cyberiav4`
- NPC roster: `67`; remote client NPCs: `67`
- `physical_contact_ms`: `0.0`
- Server loop: `11.7-12.9 Hz`; loop average `387-425 ms`; tick stalls
  `117-573 ms`.
- Server NPC window: `ai_ms` `818-888 ms/s`, total `851-943 ms/s`, 67 NPCs;
  slow decisions/path planning and collisions dominate.
- Client: `8-15 FPS`; p50 frame `50-96 ms`; network entities render up to
  `922 ms`; ragdoll world collision up to `810 ms`; snapshots missed `697`,
  interarrival `~1.0-1.4 s`.
- The first 60 Hz/240 FPS target is not met. The baseline is valid workload
  evidence, not a success claim.

The baseline is recorded in issue `issue-8bfc1ca56222`; task
`juggernaut-60hz-baseline` has a verified `runtime_proven` task proof linking
the pinned server journal and `performance.server-phase-budget` with
`npcs=67`.

## Dashboard presence

- Added `.dev/dashboard-policy.json` with `required` default `true`.
- Added:
  - `--dashboard-policy on|off|status`
  - `--ensure-dashboard`
- The watchdog checks the policy and ensures one coordinator dashboard when
  absent.
- Mutating dev-loop commands and isolated test-run starts ensure the dashboard.
- Pure `--status` remains read-only and does not create a dashboard.
- Closing the dashboard is temporary while required; `--dashboard-policy off`
  is the persistent opt-out.
- Verified existing dashboard PID `2132`; policy on/off/status and ensure
  commands produced the expected results.
- Updated `dashboard.md` and `dev-loop.md`.

## Pinned-run fix

- Fixed the isolated-run generation check so a pinned game generation is not
  rejected when the global current generation changes during startup.
- Added `pinned_game_generation` to the validation payload.
- A pinned run now verifies its own pinned DLL identity rather than requiring
  the current global generation.

## Remote render culling (pending build)

- Added conservative frustum culling for remote players/NPCs before
  `renderNetworkPlayer` and remote weapon rendering in
  `src/engine/engine-tick-render.cpp`.
- Reused the same view-projection for the viewport evidence block.
- Build note: `a4ae110de0dc`.
- This source change is **not compiled or runtime-proven**. It is intended to
  reduce `Rendering::Actors::NetworkEntities`, not yet the ragdoll cost.
- The gen-393 attempt (`game-dll-20261120T024625732466Z-8281b0`) reported
  elapsed `2785 s`, dependency scan `1629 s`, preflight `252 s`, compile
  `692 s`, and no new DLL published. A build-friction task records this.

## Coordination

- Sent the baseline and runtime-registry/build-queue requirements to
  `build-runtime-subagents`.
- Sent the dashboard presence policy request to `dashboard-coordination`.
- Reported the `agent-coord begin` parser failure to `build-runtime-subagents`;
  it currently blocks new agent registration.
- Recorded build-friction and remaining 60 Hz/240 FPS tasks on issues
  `issue-346c68de182d` and `issue-8bfc1ca56222`.

## Evidence

- `python -m unittest devscripts.quality.test_agent_coord`: 30 tests passed.
- `python -m unittest devscripts.quality.test_manual_build_guard`: 3 tests
  passed.
- `py_compile` passed for changed Python.
- Live pinned run journals:
  `logs/2026-11-20/20261120_032618/server-01/events.jsonl` and
  `.../client-01/events.jsonl`.
- Dashboard CLI smoke: policy on/off/status and ensure returned required state
  and PID `2132`.
- No 60 Hz, 240 FPS, culling, deployment, or human visual acceptance is
  claimed.

## Remaining work

- Build and prove the remote-cull change; measure `NetworkEntities` and FPS.
- Reduce server NPC decision/path/collision cost; evaluate background nav
  chunks, update budgeting, and CPU affinity.
- Reduce client ragdoll/network-entity cost and snapshot misses.
- Diagnose/reduce the gen-393 dependency scan and compile time.
- Fix the `agent-coord begin` parser before new agents can register.
