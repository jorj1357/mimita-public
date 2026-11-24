# CPU-first optimization loop: generation 605 and next candidate

## Current user goal

Reduce MiMITA runtime resource use through a repeated, falsifiable code-change and live before/after loop. Every build requested for this CPU effort must contain one isolated CPU-focused change. Preserve active 240 FPS frame-time requirements, fixed 60 Hz server simulation, and gameplay behavior. Build pressure alone is not runtime CPU proof.

## New build evidence

Attempt `game-dll-20261124T192206880822Z-db2cf7` changed `src/vehicles/vehicle-system.cpp` and published generation605 successfully in15.177s, completing at2026-11-24 19:22:22.025Z. This was other-agent feature work, not a CPU optimization pass. Its resource sidecar sampled15 times over12 logical CPUs: host CPU mean53.068%, peak70.726%, available RAM minimum4,291,698,688 bytes (4.0GiB). It had no matched prebuild baseline or journal-aligned mode/frame/tick evidence. The result is not causally comparable to generation604.

Artifact paths:

- `.dev/build-timings/game-dll-20261124T192206880822Z-db2cf7.json`
- `.dev/build-timings/game-dll-20261124T192206880822Z-db2cf7.jsonl`
- `.dev/build-timings/game-dll-20261124T192206880822Z-db2cf7.resource.json`
- `.dev/build-timings/game-dll-20261124T192206880822Z-db2cf7.resource.jsonl`

## Live runtime ownership check

Read-only process inventory briefly found PID30308, a dev-loop executable server for Vehicle Arena (`arena1`, no NPCs), launched at19:26:54Z from an agent-owned runtime directory. The root dev-loop did not list it. I asked `vehicle-settle-physics` for ownership/journal and permission before a bounded passive sample; no response arrived. At the next check PID30308 was no longer present, so I took no sample and did not alter its runtime. Free RAM was3.72GiB. There was no current connected client/server pair for the CPU goal, and no before/after run was started.

## Source candidate, not yet tested

`config/video-settings.json:4` and `src/video/video-settings.h:61` default `maxFrames` to999; `src/main-systems.cpp:1248` applies it at startup. `src/video/frame-pacer.cpp:50-68` sleeps only to enforce the configured cap and has no foreground/background or minimized-window branch. A separate background cap is a plausible client-side experiment. No causal CPU evidence exists yet.

Proposed test: with the same client process and scenario, capture foreground and unfocused/minimized CPU using fresh journal and process evidence; keep the foreground cap at240 FPS, then add one bounded background cap (30 or60 FPS) and repeat. Keep it only if background CPU falls beyond measurement variation, foreground frame-time remains within4.167ms after focus returns, the server stays at60Hz, and behavior remains correct. This test requires a generation-aligned connected run and safe memory headroom.

The other active candidate is `NpcDifficultyConfig::pollReload()` at `src/network/server-npcs.cpp:2103`; existing `npcDifficultyConfigPollMs` is wall time, so CPU cost remains unknown. Do not bundle both candidates into one build.

## Outcome

No runtime optimization was edited or tested in this session. Evidence and falsifiable candidate were appended to the CPU issue. Further work is waiting for adequate headroom and owner coordination for a live client/server measurement window.
