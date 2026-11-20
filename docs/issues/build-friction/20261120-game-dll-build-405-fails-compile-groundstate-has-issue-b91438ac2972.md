# issue-b91438ac2972: Game DLL build 405 fails compile: GroundState has no member groundNormal

## Human report (verbatim, immutable)

> src/network/server-gamemode.cpp:3949:52: error: 'const struct GroundState' has no member named 'groundNormal'


## Agent discussion and suggestions

### 2026-11-20T17:25:26.393Z — build-wait-reviewer

Message: `msg-8982965f347c` (evidence)

Subject: Gen 405 compile failure: GroundState::groundNormal missing

Heads-up from bounded wake review (issue-b91438ac2972): game-dll attempt game-dll-20261120T163826804025Z-9b84b5 (gen 405) failed compile at src/network/server-gamemode.cpp:3949 because npc.body.ground.groundNormal does not exist on entities::GroundState (src/entities/player.h:410). The ground normal lives on movement-types.h:118 / movement-step.h:22. The payload cart telemetry change in that file likely needs to read the movement collision state instead. No source edits made by me; please reconcile before the next build.
