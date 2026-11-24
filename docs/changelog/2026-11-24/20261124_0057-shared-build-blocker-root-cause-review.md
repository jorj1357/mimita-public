# Shared build blocker root-cause review (issue-d75f928fbd6f)

## Scope

Bounded review of user context `ctx-db97843045e4` for issue `issue-d75f928fbd6f`. No source was edited, no build was requested, and no lease was taken. The issue already existed and remained open; this session added independent root-cause evidence and acknowledged the context.

## Findings

- ORIGINAL FAILURE: build attempt `game-dll-20261123T234301570308Z-6c7a9c` failed `changed_source_preflight` after 157.347 s with `'projectileTrailConfigFor' was not declared in this scope` at `src/network/multiplayer-projectiles.cpp:443` (definition at line 516). Timing evidence: `.dev/build-timings/game-dll-20261123T234301570308Z-6c7a9c.json`.
- HEAD ORIGIN: HEAD `186eebd2` itself contains the defect. Its diff adds the `projectileTrailConfigFor(def)` call inside `spawnProjectileTrail` and only adds the definition later without a forward declaration, so HEAD could not compile that translation unit. The failure was not a stale working copy.
- FIX: `npc-elite-fcs-5` added a signature-matching forward declaration at line 438. Declaration order only; no behavior change.
- UNBLOCK PROOF: build attempt `game-dll-20261124T003901444604Z-3c8d4e` (generation 576, source_generation 40) completed `exit_code=0` at 2026-11-24T00:54:50.553Z and published `build/game/generation-576/game-g576.dll`; the blocked TU compiled. Evidence: `.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.jsonl`.
- OWNERSHIP: original path owner `melee-parity` was inactive; the issue was reassigned to the active fixer `npc-elite-fcs-5`. No cross-agent conflict was introduced.

## Validation and remaining work

- Compile evidence only. This is not gameplay acceptance; a pinned FCS runtime journal is still required by the issue's proof expectation.
- The forward declaration is still uncommitted, so committed HEAD remains broken until the owner or a human commits it.
- No duplicate issue was created and no agent-owned file was edited by this reviewer.
