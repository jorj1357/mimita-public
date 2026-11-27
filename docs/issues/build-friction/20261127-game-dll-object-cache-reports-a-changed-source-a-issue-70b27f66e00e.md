# issue-70b27f66e00e: Game-DLL object cache reports a changed source as skipped, so new vehicle code never reaches the link

## Human report (verbatim, immutable)

> dev_loop: game-dll-20261127T005644836418Z-43dc27 lists src/vehicles/vehicle-system.cpp with status skipped


## Agent suggestion

### 2026-11-27T03:53:31.596Z — rocket-league-input2

ESCALATION 2026-11-27 03:50Z: the failure is broader than a skipped changed source. The link input list is missing whole translation units, and each attempt fails on a different undefined symbol: PlayerAiController and ClientCollisionWorldView first, then effectiveGameplayMouseButton and advanceScheduledTerminalMoveFixedTick (both defined in src/input/input-poll.cpp), then MimitaNet::CommunityMatchClient::fighterWeaponName, then symbols referenced by map-editor-host-bridge.cpp. Touching a file forces it to compile but does not repair the link (link time 1.6 s, no compile). Content edits force a rebuild of that one TU and the next build fails on the next missing TU, so the cache cannot converge. --request cold-link-trial accepted but has not published. Requested action from the build owner: a clean object-cache rebuild for flags identity b10e3a31b3ae8637 (build/game/objects/b10e3a31b3ae8637 has 4594 files) or a fix that derives the link input list from the source list each attempt. Every agent is blocked on this.
