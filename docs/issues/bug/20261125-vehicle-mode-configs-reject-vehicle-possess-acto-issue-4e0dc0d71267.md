# issue-4e0dc0d71267: Vehicle mode configs reject vehicle_possess_actor

## Human report (verbatim, immutable)

> [GAMEMODE CONFIG] vehicle_arena.json -> vehicle_possess_actor: unknown setting


## Review (evidence-update)

### 2026-11-25T01:43:08.788Z — daily-review-20261125

Reviewer: daily-review-20261125

Direct source trace: src/gamemode/gamemode.cpp:158-168 validates each mode against the masterlist fields and emits unknown setting when a key is absent. The same file consumes vehicle_possess_actor into next.vehiclePossessActor at line 1037. A read-only search found no vehicle_possess_actor entry in config/gamemodes/masterlist/gamemode-masterlist.json, while both mode JSON files contain the key. The first divergent step is therefore the masterlist schema validation, not missing runtime consumption; this is an inference from source plus the direct config.gamemode.invalid events and awaits a build/runtime confirmation after the owner resolves it. The JSON configuration architecture states that the masterlist is the supported-key contract and unknown settings are rejected. Three observed runs selected Bloodtest, so direct vehicle_arena/freeplay startup behavior remains unknown. Cross-agent signal to arcade-collision-default on config/gamemodes/vehicle_arena.json timed out after 10 seconds; no response was present in inbox. No source, build, or runtime changes were made by this review. Next check: owner confirms schema path/ownership, then a root dev-loop build and pinned launch of both vehicle modes proves the field is accepted and applied.
