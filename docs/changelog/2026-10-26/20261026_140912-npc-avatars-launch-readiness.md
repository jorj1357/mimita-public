# NPC avatar policy and launch readiness

- Recorded: 2026-10-26T14:09:12Z (10:09:12 EDT)
- Branch: `20261011zt`; HEAD: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Result: source and build pass; Payload launch observed; TDM avatar playtest pending

## Request and first divergence

Implement a reusable NPC avatar group and respawn policy for current and future modes, with TDM choosing `competitive_no_headless` and retaining each NPC's avatar across deaths. During the work, the user reported that dashboard `[1]` did not launch a client promptly in Payload. The server process booted, bound UDP, and ticked, but its `logger.started` event never reached the shared journal, so dev-loop readiness timed out before starting the client. The first broken step was StructuredLogger's tree quota: a closed 2,316,992,363-byte legacy text log counted against the one-gigabyte ceiling, while `pruneCompletedRuns()` only selected journal run directories.

## Changes

- `config/gamemodes/masterlist/gamemode-masterlist.json`, `devscripts/gamemode_json_sync.py`, and all 16 `config/gamemodes/*.json`: define and materialize optional `npc_avatar` with a named group and `keep_per_npc` or `reroll_each_spawn` policy. TDM enables `competitive_no_headless` and `keep_per_npc`.
- `src/gamemode/gamemode.h/.cpp`: load the NPC policy; retain the last valid masterlist on a bad reload; watch the masterlist and re-resolve mode groups when it changes.
- `src/npc/npc-avatar.h/.cpp`, `src/network/server-gamemode.h/.cpp`, `src/network/server-npcs.cpp`: select an allowed avatar at the server spawn owner, respect explicitly forced role avatars only when allowed, keep an allowed prior avatar for the stable policy, reroll when configured, and emit `npc.avatar.spawn-selection` with chosen name and candidate count. Existing modes with the new block disabled keep legacy selection.
- `src/debug/structured-log.cpp`: include flat legacy `.txt` logs under dated log folders in quota pruning, using the same oldest-first order and Windows open-file rename guard as completed run directories.
- `docs/specs/gamemodes/teamdeathmatch.md` and `docs/specs/open-gameplay-issues*.md`: document policy and separate source/build progress from live and human review.
- Moved the closed oversized file from `logs/10-25-2026/Gameterminal_log_233621.txt` to `.dev/archived-logs/2026-10-26/Gameterminal_log_233621.txt` within the workspace. It was preserved, not deleted.

## Evidence

- Source/config: `python devscripts/gamemode_json_sync.py --check` passed; `git diff --check` passed (line-ending warnings only).
- Build: dev loop builds 3368, 3369, and 3371 succeeded. Build 3371 contains the StructuredLogger fix.
- Runtime: before remediation, the server wrote a conventional text log and ticked but did not emit `logger.started`; client launch failed at the readiness timeout. After archival, Payload launch in the existing dev loop emitted server readiness, then started the client about two seconds after launch selection. Latest build 3371 is running with server and client ready in `logs/2026-10-26/20261026_140242/events.jsonl`; dev-loop status reports `running_build=3371` and `latest_successful_build=3371`.
- Limits: the over-quota pruning branch has compiled but was not exercised again with an oversized closed log. TDM NPC group selection and retention have not yet been observed across NPC deaths in a live TDM run. Human visual/playtest acceptance remains open. Dashboard completion takes several more seconds after the client process starts.

## Preservation and routing

Read `docs/ROUTER.md` and its relevant gamemode, avatar, JSON, runtime, dev-loop, build, logging, and review routes. The router-referenced `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are absent. Preserved unrelated dirty configuration, docs, and existing untracked changelogs, including the pre-existing `*girl*` deny pattern in the avatar group. No commit or deployment was made.
