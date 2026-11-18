# FCS Terrorists keep moving toward the round site

- Task: Investigate and correct FCS Terrorists leaving their chosen site route
- Owner: Codex
- Status: PASS_WITH_HUMAN_REVIEW
- Time: 2026-11-18T09:49:24Z (2026-11-18 04:49:24 EST)
- Branch/base: `checkpoint/one-checkout-condensation-20261111` / `43b89938b8aa29e3bffab982a544972f8dad7968`

## Specification and diagnosis

The FCS contract says: “Every living Terrorist NPC travels to that chosen
site.” The NPC behavior contract further says Ts “fight only when a hostile
interrupts.” CTs must not receive the Ts' private site choice; they use
perception-gated reports and defend/rotate based on what they learn.

Live run `20261118_093933`, server PID `25896`, map `dust2cyberiav4`, showed
site choices stable within rounds (A for rounds 0–2, B for 3–5, then A for 6).
The server journal is
`logs/2026-11-18/20261118_093933/server-01/events.jsonl` and was live through
sequence 165656 at 2026-11-18T09:48:57.785Z. Before this change it recorded 85
T `MoveToObjective` utility decisions alongside 27 `KillTarget` and 26
`Survive` decisions. That means scoring could still redirect their movement
goal despite `objective_bias: 5.0`.

Navigation independently fails on the same route. The run recorded 379 T
`npc.nav.compare` results with `nearest_polygon_failed_start` and 144 with
`nearest_polygon_failed_both`; destination polygons were found in sampled
start failures. Custom fallback attempts recorded 90 `blocked`, 47 `initial`,
and 11 `empty` plan failures, plus 81 T `no_progress` movement failures. One
sample for actor 100016 was at `[-601.82, 32.18, 2361.12]` with chosen-site
destination `[-899.61, 153.67, 2361.31]`: Recast could not locate a start
polygon, so it fell back to the custom planner. The actors were assigned the
site; route acquisition/progress is the separate failure.

## Source change

`src/npc/npc.cpp` is `game`-owned in `build/source-manifest.json`.

- After shared utility scoring in `senseWorld`, an FCS objective-rush
  Terrorist farther than 4 m from its known objective now keeps
  `MoveToObjective` as its movement goal, even when combat scoring would choose
  another goal. It still fights through the existing combat path. Bomb
  recovery, grenade hazard avoidance, and a carrier eligible to plant are
  preserved. Once planted or at the site, ordinary objective/defense scoring
  resumes.
- The existing five-second `npc.rush` sample now includes the actor position,
  objective-priority state, visible-target state, remaining route nodes, and
  last replan reason. It is emitted while a target is visible as well, so the
  objective route can be checked during combat.
- Existing user changes in `npc.cpp` (weapon-cycle filtering) were preserved.

CT behavior is unchanged: their site defense and investigation destinations
continue to come from their own sites and team reports, not the T round choice.

## Validation and remaining evidence

- `git diff --check -- src/npc/npc.cpp`: passed.
- No tests were added or run.
- The root dev-loop observed the saved source change and reported
  `game_building`, then published generation 344 at
  `build/game/generation-344/game-g344.dll`; the running server PID 25896
  recorded activation at `2026-11-18T09:50:04.686Z`. Post-activation
  `npc.rush` records show T actors retaining `MoveToObjective` and reporting
  live positions. Example actor 100018 was 294.9 m from site B with six route
  nodes in `following_route`; actor 100017 was 323.5 m away with `no_path`.
  This proves the DLL activated and objective priority remained selected, but
  not that all Ts can navigate, arrive, or plant. The same run still had T
  route failures (`initial` 23, `blocked` 15, `target_moved` 3, `empty` 3).
- The pre-change journal confirms the objective choice but shows unresolved
  Recast start-polygon and custom-planner failures. Successful travel by every
  T, plant completion, CT investigation, and post-plant defense remain open.
- No human visual review was performed. No regression entry was added because
  the navigation failure's owning cause has not yet been isolated.

## Live DLL build journal/dashboard observability

The whole-game worker now emits a bounded build-note event after it computes
the saved-input change report, including the source revision and changed paths.
For each translation unit it emits a structured `dev-loop.game-build.file`
journal event with CXX/SKIP/failure outcome and elapsed seconds. The worker
also retains the latest 100 file results in `build/game/progress.json`; the
dashboard displays the latest four with timings. Existing source identity and
change report are used; no agent prompt or author identity is inferred.

- Source: `devscripts/game_build_worker.py`, `devscripts/dev_loop/game_build.py`,
  `devscripts/dev-loop.py`, and `devscripts/dev_loop/gui.py`.
- Validation: `git diff --check` passed for the focused changed source files.
  No synthetic tests or build were run. The currently running dev-loop process
  initially predating these Python edits was replaced through the controlled
  `--ensure-running` path. The new daemon PID 19536 reports the on-disk source
  hash `506ad06e690d144702d00d424fe2d9de08c974b3c9b1fe1fc18d6eff3fdcda43`.
  A follow-up status preserved run `20261118_093933`, server PID 25896, client
  PID 2120, and published DLL generation 344. No new DLL build was initiated,
  so per-file journal events and the updated dashboard view have not yet been
  observed on a fresh attempt; runtime acceptance is pending.
- The NPC DLL activation evidence above is independent of the Python
  observability edits and does not verify them.

## Reviewed documents

- `docs/ROUTER.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/gamemodes/funcounterstrike.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md` and `docs/regressions/gameplay/README.md`
- `docs/specs/hotreload/hotreload.md` and
  `docs/handoffs/hot-reload-agent-focus.md`
- `docs/changelog/2026-11-18/20261118_093529-fcs-npc-objective-priority.md`
- `docs/changelog/2026-11-18/20261118_091404-npc-a-star-frontier-heap.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/specs/dev-loop/dashboard.md`
- `docs/specs/dev-loop/build-time-observability.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
