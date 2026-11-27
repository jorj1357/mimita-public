# issue-51b425637ea9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-27T01:21:11.891Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-075e654596dc), read-only, for the 2026-11-27T00:44:12Z occurrence. No source edit, build trigger, or fix performed.

OBSERVED FACTS
- Wait game-dll-build:2026-11-27T00:44:12.064Z, stage game_build, elapsed 61.272s (budget 60.0s). Attempt game-dll-20261127T004412307260Z-20aea5, request 2026-11-27T00:42:17.246Z-d93af8bb:mode3:game, source_generation 17, dev-loop run 20261127_001718.
- Acceptance "reviewer wake queued exactly once" is satisfied: logs/2026-11-27/20261127_001718/dev-loop/events.jsonl contains exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id, both at wall_time 2026-11-27T00:45:18Z. wait_id, stage, elapsed, build/run IDs and evidence are present on the issue.
- The wait was NOT a stall; the attempt was actively compiling at 61.272s: manifest_snapshot 2.078s, manifest_validation 0.031s, source_snapshot 0.796s, packet_registry codegen 2.390s, game-build.started 00:44:32Z, TU1 src/engine/engine-tick-ui-game-hud.cpp done 00:45:06Z (54.1s), TU2 src/render/render-player.cpp done 00:45:13Z (60.6s), TU3 module-identity-provider done 00:45:22Z (69.8s).
- The waited-on attempt ultimately FAILED at link (total 105.875s, link 29.906s, gen 742): ld.lld undefined symbols PlayerAiController::{active,meleeWeapon,buildFrame,setMeleeWeapon,setActive} and ClientCollisionWorldView ctor + vtable. changed_paths were only engine-tick-ui-game-hud.cpp and render-player.cpp; player-ai.cpp and client-collision-world-view.cpp were skipped by the object cache even though their definitions exist in source (src/actor/player-ai.cpp:635, src/combat/client-collision-world-view.cpp:23).
- Stale-cache recovery confirms the cause: three link failures on gen 742 (20aea5 105.9s, f583bc 56.3s, 0200fc 90.9s), then a one-line "build-cache recovery" comment added to src/actor/player-ai.cpp and src/combat/client-collision-world-view.cpp forced those TUs to recompile; attempt game-dll-20261127T005644836418Z-43dc27 then SUCCEEDED (generation 748, total 173.781s, compile 99.875s, link 50.078s, compiled 3: player-ai.cpp 96.179s, client-collision-world-view.cpp 84.018s, identity provider 3.409s). No behavior code changed.
- The flat 60s budget is below the natural wall time of a normal game-DLL build: even the successful gen-748 attempt took 173.8s, and the failed 20aea5 was 105.9s. This is another false-positive over-budget wake on a healthy, progressing build.
- The reason field still stores a truncated raw worker JSON tail (": CFG cleanup..."), matching the known tech-debt in issue-6e08d89b55e3.

CLAIM SEPARATION
- Wait-budget observation: valid but expected under the current flat policy; fired during healthy progress.
- Build failure: a real object-cache-freshness defect (cached objects for two TUs lacked their definitions), already recovered by another agent. Not caused by the wait; recorded and cross-linked.
- Build success is not runtime/behavior proof; none applies to this build-friction bookkeeping issue.

OWNER / NEXT STEP / UNKNOWNS
- No fix claimed here. Object-cache freshness is cross-linked to issue-a6890271d414 (owned by build-wait-reviewer); flat-budget scaling is covered by issue-136d4553ff92 and issue-9a4503f6ed68.
- Unknown: the exact cache predicate that allowed player-ai.cpp / client-collision-world-view.cpp to be skipped while their cached objects were stale. Not investigated in this bounded review.
