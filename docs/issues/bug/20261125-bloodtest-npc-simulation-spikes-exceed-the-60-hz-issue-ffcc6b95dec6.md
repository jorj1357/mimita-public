# issue-ffcc6b95dec6: Bloodtest NPC simulation spikes exceed the 60 Hz server tick budget

## Human report (verbatim, immutable)

> Direct observation 2026-11-25 gen 627 isolated run 20261125_005609: in 177 one-second performance.server-tick-window samples, tick_count averaged 61.36, but total_max_ms exceeded 16.67 ms in 176/177 windows (99.44%) and npc_max_ms exceeded 16.67 ms in 176/177; the worst window had total_max_ms 54.49 and npc_max_ms 47.75 with only 2 NPCs. Direct observation 20261125_004049: 281/283 windows had npc_max_ms over 16.67 ms, worst 48.18 ms.


## Agent suggestion

### 2026-11-25T06:04:42.636Z — opencode-deepseek-v4-1

Clean-environment confirmation 2026-11-25, generation 640, run 20261125_060235 (normal Bloodtest movement 15 m/s, one NPC): 55 performance.server-tick-window samples, tick_count average 59.58, 14/55 windows (25.5%) had total_max_ms over 16.67 ms, npc_max_ms over 16.67 in 12/55, max_npc 1646.9 ms (startup/first-NPC spike). After the startup spike, normal windows are still ~60 ticks/s but individual NPC ticks exceed the budget; this is not 99.99% 60 Hz. Client FPS on the same run was 229.5-246.7, p50 3.74-3.94 ms, p95 5.59-6.37 ms, p99 7.08-7.60 ms, so the 240 FPS target is met while the server NPC spike remains.
