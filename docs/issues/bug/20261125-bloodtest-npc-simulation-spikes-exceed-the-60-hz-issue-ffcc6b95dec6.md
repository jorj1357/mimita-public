# issue-ffcc6b95dec6: Bloodtest NPC simulation spikes exceed the 60 Hz server tick budget

## Human report (verbatim, immutable)

> Direct observation 2026-11-25 gen 627 isolated run 20261125_005609: in 177 one-second performance.server-tick-window samples, tick_count averaged 61.36, but total_max_ms exceeded 16.67 ms in 176/177 windows (99.44%) and npc_max_ms exceeded 16.67 ms in 176/177; the worst window had total_max_ms 54.49 and npc_max_ms 47.75 with only 2 NPCs. Direct observation 20261125_004049: 281/283 windows had npc_max_ms over 16.67 ms, worst 48.18 ms.

