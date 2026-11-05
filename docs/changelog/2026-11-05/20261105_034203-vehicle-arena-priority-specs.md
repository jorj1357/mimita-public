# Vehicle Arena First Priority

- Timestamp: `2026-11-05T03:42:03Z` (2026-11-04 23:42:03 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `bb1df89d5a6743205def70507e5e3d911191943e`
- Result: `PASS_WITH_HUMAN_REVIEW`; canonical gate is RED for unrelated lint/runtime-scenario findings

## Scope and changes

Documentation-only implementation of the user's “Rocket League first” plan. Updated the vehicle feature record and owning specs:

- `docs/features/vehicles/vehicles.md:1-78`: changed the feature goal from an online weaponized car to a multiplayer car-and-ball arena; reordered phases to put the arcade profile, authored arena, replay fidelity, and quick clip path before builder, NPC, weapons, destruction, and generated maps; removed stale claims that online server authority/seat packets are absent.
- `docs/specs/vehicles/vehicles.md:36-96`: made the online arena the first showcase, added two-client/match/replay/human-feel acceptance, and listed broad vehicle capabilities as later work. `:538-563` now defines the reusable mode-selected arcade profile. `:647-658` points vehicle prediction to the networking contract. `:782-802` reorders implementation priority. The builder wording at `:712-715` now places visual assembly after the arena.
- `docs/specs/gamemodes/vehicle-arena.md:5-8,33-40,67-80,104-154,214-220`: clarified that a stable authored arena is first; connected arcade controls and human handling acceptance to shared systems; aligned the HUD with the shared contract; added full-match/two-client and contested 50/50 reconciliation acceptance; added replay acceptance and link. Preserved the Rocket League comparison baseline already being authored in this shared workspace.
- `docs/specs/networking/networking.md:5023-5057`: added the Vehicle Arena prediction contract: local car input history and reconciliation, keyed car-ball prediction, authoritative tick ordering, no duplicate impulse, authoritative goals/resets, and first-divergence evidence.
- `docs/architecture/collision/collision.md:189-202` and `docs/specs/debug-logging/debug-logging.md:238-258`: defined bounded vehicle-ball correction and first-divergence evidence; ordinary contacts are not logged continuously, while targeted recorded scenarios may capture full per-tick detail.
- `docs/specs/replay/replay.md:46-54,127-130`: added required vehicle, ball/contact, goal/reset, score, phase, and timer fidelity and a complete-match replay scenario.
- `docs/specs/creator-tools/video-editor.md:24-30,107-121`: prioritized choosing a recent moment, adding text, previewing, and exporting before advanced camera controls and automatic highlights.
- `docs/specs/entity-editor/entity-editor.md:980-1011`: moved the primitive builder after the arena and made the first arena use authored kickoff, ball, and goal data.
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md:11-20`: deferred generated arenas until an authored arena is reliable, then sequenced map editing and connected modules.

Key exact direction change: the feature record previously began “Make the first playable vehicle experience an online, weaponized car”; it now begins “Make MiMITA's first vehicle showcase a focused multiplayer car-and-ball arena on the shared vehicle system.” The vehicle spec previously said “a later arena mode adds a ball, goals, and scoring”; it now states the arena is the first vehicle showcase and sends prediction policy to `docs/specs/networking/networking.md`.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Documentation review: cross-spec ownership and wording reviewed; all relative Markdown links in the ten changed docs resolve. `git diff --check` reported no whitespace errors (Git emitted line-ending conversion notices for edited files).
- Canonical build: PASS.
- Focused gate selftests: grenade, area-effect, team-brain, objective, NPC-navigation, collision, and canonical-contact selftests all PASS.
- Weapon and gamemode JSON sync checks: PASS.
- Agent gate: RED. `damage-authority.findings` is 82 against baseline 81. No source/configuration or baseline edits were made for this documentation task.
- Three fast runtime scenarios failed before their assertions could run because their live-client events timed out: `grenade_tick_contract_v1` (`logs/2026-11-05/20261105_033420/dev-loop/events.jsonl`), `npc_wall_containment_v1` (`logs/2026-11-05/20261105_033651/dev-loop/events.jsonl`), and `scenario_record_roundtrip_v1` (`logs/2026-11-05/20261105_033900/dev-loop/events.jsonl`). The independent scenario-recording roundtrip check passed. These do not establish or refute Vehicle Arena behavior.
- No Vehicle Arena runtime, multiplayer, visual, or driving-feel acceptance was performed. Human playtesting of arcade handling remains required.

## Documentation TODO findings

The routed whole-doc scan found existing informal TODOs in touched specs; they were left unchanged per `docs/ROUTER.md`:

- `docs/specs/procedural-infinite-world/procedural-infinite-world.md:1`: “todo - explain the inf dng slr infinite world gneration”. Suggested expansion: define the seeded chunk-generation model, reusable authored modules, authority/validation boundary, and measured server/client workload.
- `docs/specs/networking/networking.md:107`: “todo - explain little quality of life things and discoverability and scaling of mimita engine”. Suggested disposition: move this product-discoverability vision to its owning product/design spec and keep networking requirements focused on network behavior.
- `docs/specs/networking/networking.md:109`: “todo explain all console commands either in here or a centralized document”. Suggested disposition: link the centralized terminal-command specification and state that command syntax/behavior is owned there.

## Pre-existing and concurrent workspace changes

At initial inspection, unrelated edits to five remote avatar screenshots, `avatar.json`, `config/analytics.json`, `config/audio/music-settings.json`, and the `devscripts/blender-ai` submodule were present and preserved. During this task, a separate Rocket League reference-baseline edit and its changelog appeared in the shared workspace, as did untracked `devscripts/open-dev-loop-dashboard.py`; these were not authored or altered by this session. No unrelated work was reverted.
