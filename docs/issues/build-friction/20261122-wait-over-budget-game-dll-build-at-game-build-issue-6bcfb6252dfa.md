# issue-6bcfb6252dfa: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T03:43:32.228Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait game-dll-build:2026-11-22T03:17:28.859Z (elapsed 60.933s, budget 60.0s).

VERDICT: not-wedged / policy miscalibrated. The build was actively making forward progress the whole time; the 60s over-budget trip is expected for a normal game-DLL build, not a stuck wait.

EVIDENCE (run 20261122_011031 dev-loop/events.jsonl):
- 03:18:33.446 dev-loop.wait.over_budget recorded; elapsed 60.933s >= 60.0s; attempt game-dll-20261122T031729161388Z-2185e7, source_generation 5, revision f34b538, request auto-5.
- Stage timeline same attempt: resident_state_codegen 2.87s -> packet_registry_codegen 61.80s (single stage alone exceeds the 60s budget) -> manifest_snapshot 14.85s -> manifest_validation 19ms -> source_snapshot 43.58s -> compile of src/network/multiplayer-packets.cpp 169.2s.
- Build reached a real terminal cause at 03:22:45, total 317.6s: compile error src/network/multiplayer-packets.cpp:2403-2404 (assignment of read-only variables lastHeardAge / snapshotAge), owner codex-ingame-gui-reconnect.
- That defect was then fixed: revision a7150409 compiled clean and generation 446 published at 03:28:48 (360.5s).

ROOT CAUSE OF THE RECURRING FRICTION (not the build itself):
- dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py lines 107-110) is applied uniformly to game-dll-build at dev-loop.py lines 3518-3535.
- Normal game-DLL build duration measured here is 317-626s (see .dev/build-timings) and single stages (packet_registry_codegen 61.8s, npc.cpp 227s) exceed 60s. So every healthy game-DLL build trips the budget and spawns a duplicate build-friction issue plus reviewer wake. This issue is one of roughly 40 identical open ones.

RECOMMENDATION (needs human approval; not edited here):
- Give game-dll-build its own larger budget, or gate issue creation on lack of forward progress (no new stage/file event within N seconds) instead of a flat elapsed threshold. Natural owner: issue-136d4553ff92 (account for every DLL build stage).

No source edited; no files claimed.
