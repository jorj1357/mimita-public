# Juggernaut NPC navigation investigation

- Status: `PASS_WITH_HUMAN_REVIEW`
- UTC timestamp: `2026-10-08T19:37:00Z`
- Display timezone: `America/New_York` (2026-10-08 15:37)
- Branch: `afad20a-rebuild`
- Scope: investigate and repair Fighter swarm clustering, corner/wall contact, and missing pursuit on `dust2cyberiav4` in Juggernaut mode.

## User evidence

The attached screenshot is treated as human-observation evidence only. It shows a concentrated group of Fighters in a constrained map area while the Juggernaut is elsewhere; it is not a structured runtime proof.

## Source and ownership evidence

- `docs/architecture/player-npc-systems/npc-group-behavior.md`: TeamFocus/SquadCoordinator own shared focus, anchor, slots, cohesion, and swarm suggestions; navigation owns route feasibility; shared physics owns final movement.
- `src/network/server-gamemode.cpp:2390-2420`: when a gamemode has `actor_preset`, the current roster assignment branch assigns that preset to every participant instead of applying the gamemode's declared role counts.
- `config/gamemodes/juggernaut.json:5,18-29`: Juggernaut declares `actor_preset: juggernaut_arcade` while also declaring Fighters and Juggernauts as separate team roles.
- `config/roles.json:28-31`: `juggernaut_fighter` references `juggernaut_swarm`.
- `config/behavior-profiles.json:505-509`: `juggernaut_swarm` enables `navigation_backend: recast`, `focus_enabled`, and `swarm`.
- `src/network/server-gamemode.cpp:3360-3440`: squad tuning and focus/slot context are only populated for NPCs whose resolved behavior is active and `focusEnabled`.
- `src/npc/npc.cpp:936-1025,1730-1775`: navigation diagnostics and movement arbitration already exist; the final movement still reaches shared physics.
- `src/npc/npc.cpp:1028-1115`: `npc.movement-decision` is bounded to one per NPC per second but is emitted at `Verbose`.

## Runtime journal evidence

Journal inspected: `C:\mimita-v9\logs\2026-10-08\20261008_193450\events.jsonl`.

- Build/runtime identity: build `1730`, source-current run, mode `juggernaut`, map `dust2cyberiav4`.
- Map/navmesh preparation succeeded: 7,442 collision triangles, 1,339 walkable triangles, 1,436 Recast polygons, navmesh version 1.
- Fighter target events repeatedly contain `preset: juggernaut_fighter` but `profile: ""`; 1,311 target-change records had an empty profile versus 340 with `juggernaut_limited`.
- The journal contains zero `npc.squad-mode-changed`, `npc.focus-changed`, or `npc.squad-slot-assigned` records. Therefore the intended Fighter swarm layer is not evidenced as active in this run.
- Fighter route failures are `npc.nav-plan-failed` with `path_nodes: 0` and `search_radius: 0.0`; observed reasons include `initial`, `target_moved`, `finished`, `blocked`, and `empty`.
- The journal contains no `npc.nav.compare` or `npc.nav.result` records for the Fighters, consistent with their empty behavior profile not selecting the Recast backend.
- The journal contains 1,599 `npc.stuck-episode` records for team 0 with empty profiles and 62 for `juggernaut_limited`; these are recovery summaries, not proof that each episode was visually acceptable.
- The configured NPC movement category is `important`, while the complete post-physics movement snapshot is `verbose`; the current journal therefore cannot prove the final requested direction, collision result, and net progress for each Fighter.

## Implementation and follow-up runtime evidence

- `src/network/server-gamemode.cpp`: role-driven modes now keep explicit team roles ahead of the mode-wide actor preset; `serverResolveActorSpawnProfile` now reads the descriptor's explicit behavior profile instead of an empty local result. The roster emits `npc.roster-profile-resolved` at `buildObjectiveRoster`.
- `src/gamemode/match-roles.cpp`: same-ID actor-preset files now preserve omitted gameplay metadata from the matching role, including behavior profile, health, loadout, team, and spawn group.
- `src/network/server-npcs.cpp`: NPC adoption emits the resolved profile at the real simulation boundary.
- `src/npc/npc.cpp`: one bounded NPC movement sample per eight actors is `IMPORTANT`; the same bounded sample is used for Recast route results.
- Canonical build: `python build_agent.py` succeeded after the changes; the build compiled the changed owners and produced `.dev/builds/1742/mimita.exe`. The linker printed a resource-merge warning while the build wrapper still reported success; the executable launched and produced runtime evidence, so this warning is recorded separately from runtime proof.
- Exact executable identity was captured with `--versioninfo`; the controlled run journal was `C:\mimita-v9\logs\10-08-2026\20261008_154707\events.jsonl`.
- In that run, roster resolution was `64 juggernaut_swarm` Fighters and `4 juggernaut_limited` Juggernauts. The journal contained 12 `npc.focus-changed` events and 1 `npc.squad-mode-changed` event.
- Sampled Fighter Recast results were authoritative and successful: start and destination polygons were found, navmesh version was 1, and route lengths were nonzero (roughly 303–411 m in sampled records). There were no Fighter `npc.nav-plan-failed` events; the one failure was a Juggernaut.
- Remaining `npc.stuck-episode`/recovery records are primarily Fighter local-separation events around the shared spawn cluster, not evidence of failed Recast routing. A real-client Dust 2 acceptance run is still required to confirm that the visible corner behavior is gone.

## Finding

- Severity: high
- Type: spec-code disagreement / missing runtime configuration application
- Specification: Fighters should use the shared `juggernaut_swarm` profile, move as a coordinated unit, spread into slots, and reach the Juggernaut without wall-grinding.
- Actual behavior: current runtime evidence shows Fighters with no behavior profile, no squad/focus events, custom route failures, and high stuck recovery counts.
- First divergence: behavior/role resolution before TeamBrain group setup. This must be fixed or disproven before tuning wall avoidance, local avoidance, or Recast geometry.
- Recommended action: preserve role-based assignment for Juggernaut's Fighter/Juggernaut roster and treat the mode actor preset as presentation-only or apply it only at the intended presentation boundary. Add a bounded roster-resolution event proving actor, role, team, preset, requested profile, resolved profile, and source (`role`, `mode`, or override). Do not add a second swarm AI.
- Human decision required: confirm whether `actor_preset: juggernaut_arcade` is intended as a mode-wide presentation override or was intended to replace role assignment. The written Juggernaut spec says roles own actor-preset and behavior composition, so the current role-preserving interpretation is recommended.

## Logging/fix sequence

1. Add or verify one change-edge `npc.roster-profile-resolved` event at `serverResolveActorSpawnProfile`/roster application. It must expose role and profile resolution, not only the final NPC profile.
2. Re-run the real mode and require `juggernaut_swarm` on Fighters plus `npc.squad-mode-changed`, `npc.focus-changed`, and `npc.squad-slot-assigned` before judging movement.
3. Temporarily enable bounded `npc.movement-decision` visibility or promote a sampled subset to important. Compare requested direction, final direction, collision/stuck state, route destination, and positive progress for the same actor/tick window.
4. Only after profile/slot evidence is correct, inspect Recast query failures (`nearest poly`, polygon refs, corridor, endpoint projection, failure category) and custom fallback behavior. Do not tune wall escape first.
5. Then test a controlled roster size. The mode currently allows 64 Fighters and 4 Juggernauts despite the description saying 16 and 2; crowding and the prior documented roster-duplication issue can independently amplify corner bundling.

## Validation status

- Source investigation: complete for the first divergence.
- Build evidence: canonical build succeeded and produced `.dev/builds/1742/mimita.exe`; build wrapper warning recorded above.
- Runtime evidence: `events.jsonl` now proves role/profile propagation, TeamFocus activation, and successful sampled Recast routing. Remaining spawn-cluster separation requires human gameplay review before further movement changes.
- Focused skills read: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`.
- Human gameplay acceptance: still required after the profile-resolution fix and a fresh real-client Dust 2 run.

## Pre-existing worktree edits

The following unrelated edits were present before this investigation and were preserved: `config/analytics.json`, `src/combat/death-system.cpp`, `src/network/multiplayer-interpolation.cpp`, `src/network/server-players.cpp`, `src/ragdoll/ragdoll-mode.cpp`, and `src/ragdoll/ragdoll-mode.h`.
