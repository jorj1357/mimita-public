# Duel community mode wiring

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Timestamp: 2026-10-21T03:52:47Z (2026-10-20 23:52:47 EDT).
- Branch / base commit: `20261011zt` / `094e6e5a`.
- Scope: make the agreed standard Duel rules selectable and route them through the shared match, role, NPC, spawn, and result owners. Keep the already-running Melee session alive.

## Changes with exact before and after

- `config/gamemodes/duel.json:3-17`: before had the description `First to 20... max action... no downtime... like MGE`, `time_limit_seconds: 0`, and rematch settings. After configures first-to-20, 180 seconds with `time_limit_outcome: stalemate`, `respawn_seconds: 0.01` (next fixed update), 15/3/5-second phases, shared map spawn with radius 5, first-to-score, two one-slot teams, the NPC roster policy, and `weapon_set_id: 1`.
- `config/onlinemodes.json:27-35`: before the Team Deathmatch entry was followed by Bomb Tag. After adds the selectable community mode: `"id": "duel"`, `"gamemode_id": "duel"`, and `"max_players": 2`.
- `config/roles.json:317-355`: before the final role was `zombie_tower_normal_zombie_npc`. After adds `duel_red`, `duel_blue`, `duel_red_npc`, and `duel_blue_npc`; each selects `duel_standard` and `source`, and NPC roles use `balanced` behavior.
- `config/actor-presets/duel_standard.json:1-10`: new file; no prior text. Adds the shared preset with forced third-person camera and `source` movement.
- `src/gamemode/gamemode.h:182-184`: before only had `int timeLimitSeconds = 0;`. After adds `timeLimitOutcome`, defaulting to `high_score` so existing modes keep their behavior.
- `src/gamemode/gamemode.cpp:159`: before parsed `time_limit_seconds` and then `respawn_seconds`; after also parses the generic `time_limit_outcome` field.
- `src/gamemode/match-roles.h:113`: before `forceFirstPerson` was followed by `ActorPresetPresentation`; after adds `forceThirdPerson`.
- `src/gamemode/match-roles.cpp:239-242`: before recognized only `first_person`; after also parses forced `third_person` from the same camera contract.
- `src/network/community-match-client.cpp:273-274, 288`: before only forced first person; after applies a forced third-person preset and includes both camera flags in the existing preset-application diagnostic.
- `src/network/community-match-client.cpp:569-573`: before winner text came only from a winner participant ID; after shows `STALEMATE` for a timed-out mode configured with `time_limit_outcome: stalemate`, and passes the winner-presence bit to the generic results banner.
- `src/network/server-gamemode.cpp:4503-4521`: before every `first_to_score` timeout selected the highest-scoring actor. After the configured stalemate outcome clears winner IDs, emits `gamemode.match.stalemate`, and records the match; the default high-score path remains. Score-limit evaluation remains before timeout evaluation.
- `src/gui/hud/match-leaderboard.h:40`: before `setWinner` accepted only name and visibility; after it accepts `hasWinner` (default true) to support no-winner results without a mode-specific HUD.
- `src/gui/hud/match-leaderboard.cpp:37-43, 51-56, 170-175`: before every visible result was prefixed/formatted as a winner; after a no-winner result renders its status text directly, while winner formatting is unchanged.
- `src/network/server-gamemode.cpp:5383`: before the legacy `matchMode == "duel"` scoring branch captured all Duel kills; after it applies only when the mode does not declare `first_to_score`, allowing standard Duel through shared team-aware scoring and results phases.
- `docs/specs/gamemodes/duel.md:5-140`: before this specification described the preset and spawn placement as unresolved and left the same-tick score/timeout rule open. After records the chosen preset, weapon set, movement, NPC fill, spawn offsets, phase loop, timeout label, and score-before-timeout rule; leaves map coverage and series wins open.
- `docs/specs/gamemodes/gamemodes.md:111-119`: new generic contract; no prior section. Documents `high_score` and `stalemate` timeout outcomes and the shared score-first boundary.

The mode-specific rocket movement file `config/movement/movement-source-rocket.json` was left untouched. Numerous dirty files were already present before this implementation, including Melee presentation/combat files and `src/network/server-gamemode.cpp`, `src/network/community-match-client.cpp`, `src/gamemode/gamemode.cpp`, and `config/roles.json`; their pre-existing edits were preserved.

## Instructions and review

Read `docs/ROUTER.md`, `docs/specs/gamemodes/duel.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/networking/networking.md`, `docs/architecture/player-npc-systems/player-npc-systems.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/logging-checker-v1.md`.

- Specification review: `PASS_WITH_HUMAN_REVIEW`. Resolved the decisions supplied by the user; exact score panel contents and session series wins remain open.
- Logging review: `PASS_WITH_HUMAN_REVIEW`. Stalemate has a bounded authoritative event. Live phase, roster, camera, kill, and respawn assertions still require a Duel runtime journal.
- Config check: `PASS`. Parsed the Duel mode, online mode catalog, and actor preset as JSON; parsed `roles.json` after stripping its existing `//` comments and confirmed all four Duel role IDs.
- Build: `PASS`. Canonical dev-loop published final build 2726. `C:\mimita-v9\.dev\builds\2726\mimita.exe --versioninfo` reported that exact executable and `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_235242/events.jsonl`. This identifies the executable only; it is not Duel gameplay evidence. The existing dev-loop run stayed alive on Melee using build 2722. One build request linked but hit the existing `WinError 5` metadata-rename error; retry succeeded and published build 2726.
- Runtime/human review: `NOT RUN`. The active server/client belong to a Melee session. Switching them to Duel would interrupt that unrelated run, so the Duel acceptance scenario remains pending user approval to replace the active run. No synthetic gameplay test was added or run.
- Regression: none confirmed.

- Whitespace check: git diff --check reported three trailing-whitespace lines in the pre-existing user-edited docs/specs/gamemodes/juggernaut.md; no Duel implementation whitespace errors were reported. The unrelated lines were left untouched.


- New changelog file, no prior text: # Duel community mode wiring.
