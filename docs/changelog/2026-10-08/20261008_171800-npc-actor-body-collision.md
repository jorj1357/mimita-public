# NPC actor body and triangle collision slice

- Timestamp: 2026-10-08 17:18 EDT
- Repository: `C:\mimita-v9`
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Implemented the next runtime slice of universal actor collision. Dedicated-server NPCs now receive a headless character collider using the same six physical body parts as the player path, and the existing AimBody NPC path can produce a hybrid pose before the shared actor-triangle pass. The server now runs one fixed-tick actor-triangle pass over live NPCs and connected players, covering NPC/NPC, player/NPC, and player/player participant paths.

## Source changes

- `src/entities/player.h` and `src/entities/player-loader.cpp`: added `Player::loadCharacterColliders`, reusing the character registry and headless GLB collider loading for server actors.
- `src/network/server.h`: added a headless collision-body adapter to `ServerPlayer`.
- `src/network/server-npcs.cpp`: initializes NPC collider bodies, creates player collision proxies, runs the shared server actor pass, and emits participant and periodic summary evidence.
- `config/debuglogger.json`: enabled important collision events and file output for the proof run.

The implementation preserves the existing player/NPC AimBody path and does not create a second NPC body-animation system. Ragdolls, support inheritance, full replicated player AimBody claims, mass-weighted heavy/light response, and swarm optimization remain outside this slice.

## Required evidence and documents reviewed

Read the routed architecture, collision, movement, networking, logging, performance, runtime-scenario, build/EXE, live-development, completion, and focused-review documents selected by `docs/ROUTER.md`. The repository's build/runtime evidence rules were followed: build/link evidence is separate from runtime behavior evidence, and no synthetic self-test was used as the primary proof.

## Build evidence

Forced a uniquely named relink after the incremental build reported no source changes:

`C:\mimita-v9\mimita-20261008T1715-actor-collision.exe`

Build status: `BUILD SUCCESS`, return code `0`. `--versioninfo` reported run `20261008_170210` and the canonical event path format. `git diff --check` reported no whitespace errors; only normal CRLF conversion warnings.

## Runtime evidence

### NPC hybrid body activation

`logs\10-08-2026\20261008_170252\events.jsonl` records three NPC `actor-hybrid.pose-input` events with `aimbody_mode: hybrid`, `hybrid: true`, and `parts: 6`. The same run records three eligible `actor-collision.participant` events, each with `physical_part_count: 6` and `body_triangle_count: 72`.

### NPC/NPC triangle collision

`logs\10-08-2026\20261008_170349\events.jsonl` was run on Dust2 with two NPCs sharing a spawn location. It records:

- `actor-collision.contact` with both participants `actor_kind: npc`, body-part names, normals, penetration, and `triangle_contacts: 16`;
- `actor-collision.response` for the same pair using `backend: triangle_pair_manifold`;
- periodic summaries showing `participant_count: 3`, `pair_count: 3`, `broadphase_pairs: 2`, `triangle_candidates: 117`, `triangle_contacts: 32`, and `responses: 2`;
- 440 contact events and 440 response events during the four-second overlap run.

This is direct runtime proof that NPCs now have physical triangle bodies and that NPC/NPC contact reaches the shared authoritative server solver.

### Player participation

`logs\10-08-2026\20261008_170402\events.jsonl` records a player as an eligible four-participant server actor with the same 72-triangle body representation. The shared pair pass includes the player in its pair count. That run did not place the player close enough to an NPC to produce a player/NPC contact, so player/NPC and player/player contact behavior is not claimed as human-accepted by this change.

## Open findings

- The overlapping three-NPC run degraded from approximately 60 Hz toward approximately 49 Hz / 100 ms loop windows. This is a real performance issue for dense swarms; it must be addressed before swarm readiness is claimed.
- Collision proof logging is intentionally verbose while `collision` is set to `important` with file output. It should be changed to bounded state-change/scenario-filtered sampling after the owner path is optimized.
- Server player proxies currently use the headless DefaultGuy collider. Full authoritative parity with each player's replicated hybrid limb pose and weapon triangles is still required.
- Ragdoll participation, actor support inheritance, and heavy/light mass behavior are not proven by this slice.

## Human acceptance required

Use the real executable to deliberately overlap a player with an NPC and two players, then inspect the canonical journal for `actor_a_kind`/`actor_b_kind` combinations and observe that the bodies stop passing through one another. Human review must also assess movement feel and whether the measured fixed-tick cost is acceptable.
