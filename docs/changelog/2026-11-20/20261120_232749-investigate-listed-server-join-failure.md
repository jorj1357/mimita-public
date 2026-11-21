// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Investigate listed server join failure

# Task

- Task ID: session-15fbad2d0001
- Session ID: session-15fbad2d0001
- Owner: root
- Focus: server browser join; AI issue visibility
- Summary: Linked the user's 9k5ypnd report to the active networking reliability issue and traced the server-browser list-to-join handoff. The UI renders coordinator player counts and permits JOIN independently of the browser ping result. Sent the active issue owner a request for paired reproduction evidence; no user-specific room code/run was available, no source was edited, and no runtime behavior was claimed.
- Status: investigated; awaiting exact room identity and runtime reproduction
- Date, time, timezone: 2026-11-20T23:27:48+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

The first `git status` was run after the coordination context and issue-linked message were written, so it cannot distinguish every concurrent/pre-existing change. The captured status listed `devscripts/blender-ai`, another changelog, `docs/issues/README.md`, the map-world capability issue, and the networking umbrella issue as modified. Those unrelated changes were preserved. This session's own durable issue-discussion append is described below.

# Requested behavior

The user reports that MiMITA.exe shows a public server with `2/999` players but does not join it, asks for reliable joining whenever a server is listed, and asks how to make AI inspect the issue. The user's exact quote is stored once in coordination context `ctx-1e45d8b7bd8a` and linked to `issue-cf9cd77eb056`. The user supplied `9k5ypnd`; it did not resolve as a coordination issue ID, so it is preserved as their report reference, not treated as the server code.

# Specification alignment

- Current specifications: `docs/specs/networking/networking.md`; `docs/specs/gui/guiv2.md`.
- Exact networking event contract: `network.client.join.request_sent` records attempt, bytes, and token presence; `network.server.join.accepted / network.server.join.rejected` record player ID or reason; `network.client.join.accepted / network.client.join.rejected` record assigned player ID, map, spawn generation, and build.
- The evidence request sent to the active connection owner follows that contract by requiring a click-to-join trace through client request, coordinator ICE begin-join, both transport/join sides, and an explicit terminal reason.
- No specification promises that a coordinator listing alone guarantees a live end-to-end connection. Whether the reported listing was stale, unreachable, password protected, or rejected remains unknown until the exact room is reproduced.

# Exact implementation changes

No product source/configuration was changed. Source review found:

- `src/gui/menus/online-menu.cpp:218-234`: displays the list-provided player/max-player values and sets the room code plus `connectToServer` when JOIN is clicked.
- `src/network/server-browser.cpp:99-183`: refreshes coordinator entries; each returned entry is separately ping-probed at lines 161-162, then replaces the visible snapshot. The browser refresh interval is 3,000 ms.
- `src/network/coordinator-client.cpp:333-375`: fetches `/api/coordinator/list` and copies advertised player counts and server metadata into entries.
- Old behavior observed in source: a row can display an advertised count and still expose JOIN regardless of the separate ping result. This does not prove the user's specific failure cause.
- New product behavior: none. I recorded the gap and requested a runtime trace from the current issue owner instead of changing shared networking code while another agent owns that investigation.
- No unrelated product behavior was changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md` | Not present | This investigation record | Required final session record | Remove only this new file if the session record must be reverted |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` | Existing issue record | Appended issue-linked message `msg-3b546c192e0e` with source finding and reproduction request | Put the new report in front of the active owner | Append-only issue history; do not erase. Correct with a later message if needed |
| Other paths in captured git status | Already dirty or changed concurrently; exact start state was not captured | Preserved | Unrelated work | Do not revert |

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

- Owner/category: Existing public browser/list and normal connection owners; reviewed only, no diagnostic code added.
- Input: Coordinator server-list response, separate UDP ping, and browser row JOIN action.
- Decision: Keep advertised count and ping status as separate observations; do not treat either as proof of successful join.
- Output: Issue discussion message `msg-3b546c192e0e` asks the active owner to correlate list snapshot/count with the authoritative roster and join stages.
- Failure or rejection reason: Not available without the room identity and a matching run journal.
- Rate limiting: Not applicable; no logging change.

# Validation

- Focused routes reviewed: `docs/ROUTER.md`, `docs/specs/networking/networking.md`, `docs/specs/gui/guiv2.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/agent-context-contract.md`, and `docs/skills/issue-triage-v1.md`.
- Commands: coordination sync/board/issue brief; `python devscripts/dev-loop.py --status --json`; `--list-instances`; focused source searches; `python devscripts/changelog.py --check --session-id session-15fbad2d0001`.
- Build status: No build requested. Dev-loop's active run was `20261120_232200` in FCS, server PID 33940 and client PID 24476; it was unrelated to the reported server. A game-DLL build request had failed the disk guard at 2031 MB free versus 2048 MB required; no build was needed for this read-only diagnosis.
- Runtime evidence: None for the user's server. No matching room code, server name, screenshot, or run journal was provided; no live join was attempted and no runtime success/failure is claimed.
- Output: Context `ctx-1e45d8b7bd8a` linked to `issue-cf9cd77eb056`; issue-linked owner message `msg-3b546c192e0e`; this changelog.

# Measured evidence

- Before/after: Not measured; no implementation change.
- Timing: Coordinator list refresh is configured for 3,000 ms; per-server UDP ping timeout is 800 ms. These are source constants, not measurements of the reported session.
- Tick/frame/network measurements: None for the user's session.

# Regression review

- Regression entry appended: no.
- Reason: The user-reported failure has not been reproduced, so it is not yet a confirmed regression with a proven cause.
- Related issue: `issue-cf9cd77eb056`; its existing connection lifecycle and AI visual-proof work remains active.

# Human acceptance

- Visual review: Not performed on the user's failing server/browser screen.
- Gameplay review: Not performed.
- Multiplayer review: Not performed.
- Still unverified: Whether the listing was current; whether the server was reachable and accepting joins; the first failing stage; why the client failed to enter.

# Agent handoff context

- Owner: `root-connection-reliability` received the issue-linked handoff; no source ownership was taken.
- Focus: public server browser list-to-join trace and visible AI evidence for the failure.
- Files to check: `src/gui/menus/online-menu.cpp`, `src/network/server-browser.cpp`, `src/network/coordinator-client.cpp`, and the established join pipeline in `src/network/multiplayer-tick.cpp` / `src/network/ice/ice-server.cpp`.
- Do not redo: source trace of list refresh, displayed advertised count, separate ping probe, and row JOIN action; see issue message `msg-3b546c192e0e`.
- Next exact step: identify the server code/name and approximate failure time; capture the browser row and authoritative server roster, then reproduce JOIN through the real executable and correlate client/server journals through accepted/rejected terminal outcome.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "8b470e4249101ba96d6bc32a4a3b1133e09366bc6c476b4bf9d504f95394a17d", "dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "published_utc": "2026-11-20T21:27:56.236Z", "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 416
- Artifact: build\game\generation-416\game-g416.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_232200\client-01\events.jsonl
- Session ID: session-15fbad2d0001

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
m devscripts/blender-ai
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
```

## Captured diff stat

```text
devscripts/blender-ai                                         |  0
 ...unch-failure-issues-preview-overlay-first-frame-context.md | 11 +++++++----
 docs/issues/README.md                                         |  3 ++-
 ...ve-collaborative-editing-and-credite-issue-421c5be1669f.md |  6 ++++++
 ...imita-development-and-networking-con-issue-cf9cd77eb056.md | 10 ++++++++++
 5 files changed, 25 insertions(+), 5 deletions(-)
```

## Captured changed paths

```text
M	devscripts/blender-ai
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
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
