# Juggernaut 240 FPS handoff investigation

- Time: 2026-10-22T16:02:30Z (2026-10-22 12:02:30 EDT)
- Branch / HEAD: `20261011zt` / `9ec73c3ae724037da33d032d54843cbf1186f97a`
- Result: `PASS_WITH_HUMAN_REVIEW` for source investigation; performance goal remains open.
- No gameplay or configuration source was edited. Pre-existing dirty files were preserved.

## Evidence and ownership

- `config/gamemodes/juggernaut.json:17-31` orders Fighters first (capacity 64) and Juggernauts second (capacity 4).
- `src/network/server-packet-chat.cpp:397-408` maps `team_pick 1` to zero-based team 0. `src/network/server-gamemode.cpp:2149-2157` fills the human's team to capacity minus one and the other team to capacity. With Fighter chosen: 63 Fighter NPCs + 4 Juggernaut NPCs + 1 human = 68 actors.
- `src/network/server-gamemode.cpp:2207-2333` builds the roster and emits `npc.roster-profile-resolved`; `src/network/server-gamemode.cpp:6519-6525` emits `actor.team-assigned` with the applied role. These establish identity/role, not continuing activity; also inspect movement decisions/velocities and server collision profiles.
- `src/perf/perf-frame.cpp:126-139, 229-240, 279` counts every rendered frame over its configured budget and emits 60-frame summaries including p95/p99, maximum and optional GPU timing. `src/network/multiplayer-tick.cpp:2651` scopes remote-entity work.

## Runtime evidence

- Existing visible build-2778 journal: `logs/2026-10-22/20261022_034032/events.jsonl`; paired client/server IDs matched per the prior record in `docs/changelog/2026-10-22/20261022_022300-headless-juggernaut-performance.md:99-153`.
- Prior complete rolling minute: 4,920/4,920 frames over the 4.17 ms budget; average 12.147 ms; maximum 1,183.35 ms. Not a Fighter-side acceptance run.
- Later 60-frame windows in the same journal still showed 1-4 misses per window (1.7-6.7%); sampled p99 was 11.0-17.8 ms. A low average therefore does not meet the zero-miss target.
- Journal has server `actor.collision.profile` samples with 67 actors and NPC movement-decision events. That is evidence some NPC simulation ran, not proof every roster member remained active together. The new run must explicitly verify the full Fighter roster and ongoing actor movement.
- No controlled before/after gain or 240 FPS pass is established. Main-thread CPU is measured; all-core/per-thread usage is not.

## Review and validation

- Read router, performance spec, efficiency checker, runtime scenario workflow, actor preset/gamemode specs, terminal command architecture, logging spec/checker, regression README, build/live-development and task-completion guidance.
- Read-only source/journal inspection only; no build or new runtime was started in this investigation.
- Remaining human review: run the real visible Fighter-side Juggernaut scenario and verify stable zero-budget-miss minute and all actors' ongoing simulation.
