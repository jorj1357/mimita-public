# FCS objective steering and dash recovery proof

## Task

- Task ID: issue-be1adb51b6ac
- Session ID: work-f4525bc2e12b
- Owner: Codex
- Focus: Fun Counter Strike objective routing, corner recovery, and NPC combat
- Summary: Fixed failed-plan fallback steering so its chosen objective direction reaches actual movement, recomputed directions when stuck/area recovery invalidates a commitment, and blocked objective-rush dashes while recovery or failed navigation owns movement. Generation 478 completed a live FCS run with NPC bomb plant/explosion, NPC-on-NPC kills, CT/T reports, positive CT displacement, and zero executed dashes during stuck or failed-navigation states.
- Status: runtime-proven
- Date, time, timezone: 2026-11-22T08:15:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca
- Final commit: 9fd637d595a308483a9f82c88349a5915c82a0ca (changes remain uncommitted)

## Requested behavior

FCS NPCs should leave corners, travel to the bomb objective, plant, and fight opposing NPCs. Dashes must not interrupt stuck or failed-navigation recovery.

## Implementation changes

- `src/npc/npc.cpp:2758`: Keep a movement commitment for a failed objective plan only while it still advances toward the objective and no progress, stuck, or area-escape recovery is active. Otherwise choose a fresh objective-biased open direction.
- `src/npc/npc.cpp:2837`: Apply the fallback direction to `moveDir` even when the route plan is invalid, while preserving the later area-escape steering override.
- `src/npc/npc.cpp:3453`: Suppress FCS objective-rush dashes while a stuck episode, area escape, failed commitment progress, stuck timer, or failed path without a path owns movement.
- `src/npc/npc.cpp:3724`, `src/npc/npc.h`, `src/network/server-npcs.cpp`: Log each executed dash with route/recovery state and remove the obsolete one-shot dash-log field and respawn reset.
- `src/npc/npc-navigator.cpp:700`: Keep the local trap timer accumulating during failed-progress drift so a broad loop cannot reset corner recovery.

## Build and runtime evidence

- Canonical dev-loop published game DLL generation 478, `build/game/generation-478/game-g478.dll`, SHA-256 `369a206bc5df5b256e552aacf1e853c0aa65309bc4a9c1437e4b8afb51dc1355`.
- Pinned runtime: `fcs-route-proof-applied-fallback`, run `20261122_081330_fcs-route-proof-applied-fallback`, FCS mode 45 on `dust2cyberiav4`, using EXE build `2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344` and DLL generation 478.
- Server journal: `logs/2026-11-22/20261122_081330_fcs-route-proof-applied-fallback/server-01/events.jsonl`.
- Client journal: `logs/2026-11-22/20261122_081330_fcs-route-proof-applied-fallback/client-01/events.jsonl`.
- At 289.29s NPC 100017 began planting at A; after two `left_site` interruptions, `objective.planted` recorded operator 100017 at `[-904.61, 150.47, 2363.12]` at 296.69s. `objective.exploded` and a Terrorist round win were recorded at 397.10s.
- The same run recorded eight `kill.reward` events with both `killer_is_npc` and `victim_is_npc` true, and 48 visible `npc.team-report` events, including 25 from CT team 0.
- All 1,072 logged executed `npc.dash` events had `stuck_episode_active=false` and `nav_plan_failed=false`. CT NPCs had positive first-to-last horizontal displacement (8.8–58.9 units); observed maximum displacement from their initial positions ranged from 107.8–207.4 units.
- Issue evidence `evidence-7d99aad1439a` verified; gameplay task proof `task-proof-19abbe522e81` verified `proven`. Issue `issue-be1adb51b6ac` closed with this evidence.

## Build notes and limitations

- Generation 474 failed because a stale `hasLoggedDash` respawn reset remained in `src/network/server-npcs.cpp`. A subsequent stale snapshot repeated that failure; generation 476 compiled after the reset was removed. Generations 477 and 478 published successfully.
- The over-budget game-DLL wait was automatically captured as build-friction issue `issue-b24f9f79794d`.
- No synthetic test was added. This entry claims live server/client journal evidence; no separate human visual review is claimed.

## Changed files and reverse steps

| File | Change | Reverse step |
|---|---|---|
| `src/npc/npc.cpp` | Objective fallback steering and stuck/failed-navigation dash gate; log every executed dash | Review and revert only this file's task diff |
| `src/npc/npc-navigator.cpp` | Preserve trap timer during failed-progress drift | Review and revert only this file's task diff |
| `src/npc/npc.h` | Remove one-shot dash-log state | Review and revert only this file's task diff |
| `src/network/server-npcs.cpp` | Remove obsolete dash-log respawn reset | Review and revert only this file's task diff |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` | Append review and proof-backed resolution | Preserve append-only issue history |
