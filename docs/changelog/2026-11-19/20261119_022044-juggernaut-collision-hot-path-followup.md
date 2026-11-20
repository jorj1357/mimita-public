# Juggernaut collision hot path follow-up

# Task

- Task ID: session-20261119-juggernaut-collision-followup
- Session ID: session-20261119-juggernaut-collision-followup
- Owner: Codex
- Focus: Continue the Juggernaut server Hz investigation with 67 NPCs
- Summary: Traced the dominant cost to NPC physics/collision work. The actor-triangle solver is enabled but cannot run for NPCs with empty physical body parts, so they fall through to the legacy multi-pass capsule collision pipeline. The target is still unmet.
- Status: in-progress
- Date, time, timezone: 2026-11-19T02:20:44+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Evidence and findings

- Live run: `20261119_014024`, mode 9 Juggernaut, map `dust2cyberiav4`, server PID 29972, clients 25916 and 18472. Dev-loop status still showed the original EXE build ID `2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d`, active DLL generation 345, and 67 server-side NPCs.
- The latest completed one-minute journal sample from the preceding investigation turn had 46 server windows, all below 59.9 Hz; median 14.493 Hz, minimum 3.333 Hz, maximum 41.806 Hz. Maximum server stall was 417.414 ms and maximum NPC phase was 376.741 ms. The fixed 60 Hz target was not met.
- `npc.phase-window` samples show physics and collision as the largest accumulated NPC stages. A representative earlier window recorded 296.278 ms physics and 249.529 ms collision for 378 NPC updates; another recorded 259.210 ms physics and 175.526 ms collision for 424 updates.
- `config/collision.json` enables `actorTriangleSolver`. In `doGLBTriangleCollisions`, a solver miss for NPCs is logged as `missing_world_or_body_triangles`; the live journal reports `body_part_count: 0`, after which execution enters the legacy path.
- That legacy path performs a sweep/slide, a separate depenetration gather and solve, floor recovery, and stuck checks. These repeated per-NPC operations are an evidence-backed hot-path target; no collision behavior was changed in this follow-up.
- The `juggernaut_limited` profile's Recast selection was removed to restore its previous implicit custom-backend behavior. The current runtime did not yet provide a clean A/B result proving improved Hz and successful movement for every NPC.

# Build and runtime status

- The root dev-loop remained in `game_building`, candidate generation 351, attempt `game-dll-e6094819e0b649d0`, revision `dd5353baccf47f1bf4e27614af0a1de862464f7642f64f24bb3ac4312d34fce0`. It had not published or loaded when inspected; the live server remained on generation 345. No build was started manually and no build wait was issued.
- No source change was made in this follow-up, to avoid changing compiler inputs while the root-owned candidate was already running. No build, restart, or synthetic test was run.
- All-NPC movement and the 99.99% 59.9–60.1 Hz acceptance target remain unproven. Continue from the live run and wait for a loaded, coherent dev-loop candidate before making a hot-path edit and measuring its effect.
