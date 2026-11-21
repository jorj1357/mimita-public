// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Trace public server row join for 9K5YPND

# Task

- Task ID: session-3357a6a4a3fb
- Session ID: session-3357a6a4a3fb
- Owner: root
- Focus: Public room listing; row-click ICE join trace
- Summary: Queried the public coordinator and confirmed 9K5YPND was listed online as NPC Dev Server, sandbox/funworld3, 0/999; uppercase lookup succeeds and lowercase lookup does not. The user confirmed they clicked the listed JOIN row, so code casing is not the failure path. Traced the row through gPendingConnect to mpIceConnectStart. A pinned local dev-loop client joined its test server successfully, but no public row click was performed because Windows UI inventory timed out; no cause or fix is claimed.
- Status: room lookup confirmed; public row click not reproduced; awaiting visual runtime evidence
- Date, time, timezone: 2026-11-20T23:53:42+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

The repository already contained concurrent edits in configs, dev-loop build scripts, NPC code, other issue records, and other changelogs when the generated status snapshot was taken. They were not edited by this session and were preserved. Coordination updated the existing networking issue with this user's exact report, room-code clarification, click-path clarification, and evidence messages. No game source/configuration files were modified here.

# Requested behavior

User requested that a public server shown as `2/999` be joinable through MiMITA.exe, and asked how AI can inspect the issue. The user clarified that `9k5ypnd` is the server join code and that they clicked the listed row's JOIN button (contexts `ctx-9520bd9816e4` and `ctx-82cc5b8fd7b4`).

# Specification alignment

- Current paths reviewed: `docs/specs/networking/networking.md`, `docs/specs/gui/guiv2.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/workflows/multi-client-dev-testing.md`, `docs/specs/dev-loop/agent-runtime-registry.md`, and `docs/specs/dev-loop/dev-loop.md`.
- `docs/specs/networking/networking.md` requires join request, server accepted/rejected, and client accepted/rejected events with attempt/bytes/token presence, player ID or reason code, assigned ID/map/spawn generation/build.
- A server-list entry and its advertised count do not prove the end-to-end join. The investigation therefore records list/lookup state separately from runtime join events and does not claim a cause from the listing alone.
- The existing umbrella issue requires explicit connection-stage evidence, truthful status, and visual/gameplay evidence tied to a run. This investigation follows that requirement; the exact public-row failure remains unproven.

# Exact implementation changes

No product source/configuration was edited. The following existing code path was traced:

- `src/gui/menus/online-menu.cpp:224-234`: a server-browser row's JOIN handler copies `e.code` directly to `r.roomCode` and sets `connectToServer`.
- `src/gui/gui-main.cpp:1810-1864`: the unprotected row click creates `gPendingConnect`, keeps the room code unless joining an owned local server, then transitions to gameplay.
- `src/engine/engine-tick-state.cpp:554-584`: pending room-code connect emits request/preflight events and calls `mpIceConnectStart` on the normal async ICE path.
- `src/network/multiplayer-packets.cpp::iceConnectWorker` and `mpIceConnectStart` (line 1582): the async worker runs the room-code ICE attempts; the UI state machine starts it for the queued room code.
- Adjacent manual-entry path: `src/gui/menus/online-menu.cpp:469-488,518-535` and `src/network/coordinator-client.cpp:297-329` pass the typed code unchanged. Direct API observation showed lowercase lookup returned `exists=false`, uppercase returned `exists=true`. This is not the user's reported row-click path and is not attributed as the cause.
- Current coordinator list response: room `9K5YPND`, `NPC Dev Server`, sandbox/funworld3, `0/999`, no password; uppercase ICE lookup reported online. The earlier `2/999` count was not present at this later snapshot.
- New behavior: none. The active connection owner received the row-click clarification, source handoff, and missing evidence requirements in issue discussions `msg-ee1641689614` and the preceding room lookup evidence message.
- No unrelated behavior was changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md` | Not present | This investigation record | Required final record for this activation | Remove only this new changelog if reverting the record |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` | Existing append-only issue record | Added the user's two clarification contexts and issue-linked messages/evidence | Put exact code, row-click path, listing snapshot, and handoff in front of the existing owner | Keep append-only; correct with a later note if needed |
| Other paths in the generated git-status snapshot | Concurrent or pre-existing changes | Preserved without editing | Protect work from other agents | Do not revert |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category: Existing server-browser/coordinator and ICE join owners; source reviewed, no diagnostics code changed.
- Inputs: Read-only `/api/coordinator/list` and `/api/coordinator/ice/lookup` for the supplied room code; source path from row click through the pending connection and ICE worker.
- Decision: Treat public listing, coordinator lookup, local loopback join, and the user's failed row click as separate evidence classes.
- Output: Current room metadata and a trace handoff appended to the active networking reliability issue.
- Failure/rejection reason: No room rejection or public ICE failure was observed because the exact row click could not be performed in this session.
- Rate limiting: Not applicable; no logging changes.

# Validation

- Focused routes: `docs/ROUTER.md`, networking and GUI specs, runtime scenario validation, multi-client dev testing, agent runtime registry, and dev-loop spec.
- Read-only coordinator observation: `/api/coordinator/list` returned uppercase `9K5YPND`, 0/999, NPC Dev Server, sandbox/funworld3; uppercase `/ice/lookup` reported online and lower-case `9k5ypnd` returned `exists=false`.
- Isolated run: `20261120_234544`, pinned EXE build `2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55` and game generation 416. Client `network.client.join.accepted` recorded assigned player 1 and server run ID `20261120_234544` after connecting to the run's own loopback server. The test run was stopped through `python devscripts/dev-loop.py --stop-test-run 20261120_234544`.
- Failed setup attempts: unpinned test was refused because current DLL source validation did not match; an explicit pinned mode-1 test run `20261120_234206` ended at `socket_bind_failed` because the selected launch mode also hardcoded port 24632 while the test harness supplied an ephemeral bind. No client/public-room attempt occurred in that run.
- Windows UI inventory via CUA `getState`/`sky.list_apps` timed out. No actual public browser row click was made by the agent; no public client/server join journal exists.
- No source edits and no build request. The latest game-DLL build state reported a disk guard failure (2031 MB free, 2048 MB required); no rebuild was attempted.
- Output: issue contexts `ctx-9520bd9816e4`, `ctx-82cc5b8fd7b4`, owner message `msg-ee1641689614`, local journals under `logs/2026-11-20/20261120_234544/`, and this changelog.

# Measured evidence

- Reported count: 2/999 at the time of the user's failure (user-provided, not independently captured).
- Later coordinator snapshot: 0/999, server status online, public listing code uppercase `9K5YPND`.
- Local loopback join: client journal event `network.client.join.accepted` at run-relative t=13.437; this measures a local test server only, not a public room.
- No public join timing or transport measurements were captured.

# Regression review

- Regression entry appended: no.
- Reason: The user's click failure could not be reproduced; current directory listing and a separate local join do not confirm its cause.
- Related issue: `issue-cf9cd77eb056`, owned by `root-connection-reliability`.

# Human acceptance

- Visual review: The user clarified they clicked the listed row, but the agent could not inspect or operate the game window because the Windows UI inventory service timed out.
- Gameplay review: Local test client's loopback join was accepted; no public-room gameplay was performed.
- Multiplayer review: Public-room click-to-ICE-to-join remains unverified.
- Still unverified: First missing event after the user's row click; why the client did not enter while the row was shown at 2/999; whether the current 0/999 snapshot is related.

# Agent handoff context

- Owner: `root-connection-reliability`
- Focus: Public room listing; row-click ICE join trace
- Files to check: `src/gui/menus/online-menu.cpp`, `src/gui/gui-main.cpp`, `src/engine/engine-tick-state.cpp`, `src/network/multiplayer-packets.cpp`, and both journals from the exact user run.
- Do not redo: The user-confirmed path is the public server row JOIN button, not lowercase manual entry; the current row hands its uppercase list code to `gPendingConnect.roomCode`, which leads to `mpIceConnectStart`.
- Next exact step: In a UI-capable session, click the row for `9K5YPND` and capture the screen plus exact client/server `events.jsonl` from `network.client.connect.requested` through ICE and accepted/rejected terminal outcome.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "8b470e4249101ba96d6bc32a4a3b1133e09366bc6c476b4bf9d504f95394a17d", "dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "published_utc": "2026-11-20T21:27:56.236Z", "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 416
- Artifact: build\game\generation-416\game-g416.dll
- Test-run client journal: `C:\mimita-v9\logs\2026-11-20\20261120_234544\client-01\events.jsonl`
- Test-run server journal: `C:\mimita-v9\logs\2026-11-20\20261120_234544\server-01\events.jsonl`
- Session ID: session-3357a6a4a3fb

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M src/npc/npc.cpp
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 111 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  22 +++-
 devscripts/game_build_worker.py                    |  94 ++++++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  23 ++++-
 ...e-issues-preview-overlay-first-frame-context.md |  11 +-
 docs/issues/README.md                              |   5 +-
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 ++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  40 ++++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 +++++
 src/npc/npc.cpp                                    |  11 +-
 16 files changed, 330 insertions(+), 31 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-20T20:20:51.788061Z",
    "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55",
    "latest_successful_source_generation": 38
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "8b470e4249101ba96d6bc32a4a3b1133e09366bc6c476b4bf9d504f95394a17d",
    "dll": "build\\game\\generation-416\\game-g416.dll",
    "generation": 416,
    "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-416\\game-g416.dll",
    "generation": 416,
    "published_utc": "2026-11-20T21:27:56.236Z",
    "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 31.195,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-20T21:40:39.596Z"
  }
}
```
