# issue-56f7a692b646: Make arcade collision the default for all gamemodes

## Human report (verbatim, immutable)

> yes, make arcade collision the default for all modes for now and then the other mode is opt in


## Resolution

### 2026-11-20T06:44:34.593Z — arcade-collision-default

Arcade collision now defaults to true in GamemodeFeatures and CollisionConfig; all 22 mode JSONs explicitly set arcade_collision=true, and non-arcade collision remains available by setting false.

Evidence: evidence-9a4ba45b64b9 (integration, proven)
