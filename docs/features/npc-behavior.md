# NPC behavior feature record

Status: active; measurement instrument and CT defuse proof exist, Phase 2
difficulty/movement proof pending.

Purpose: one durable place where an agent can find the NPC goal, every related
issue, the exact evidence and journal paths, the current owner `file:line`
hypotheses, the decisions that constrain tuning, and the solution options that
have been proposed. Specifications remain the behavior contract; this record
tracks goal, evidence, decisions, and work.

Related specifications and workflows:

- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/specs/gamemodes/funcounterstrike.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md`
- `docs/features/performance/fps-improvement-ledger.md` for server tick cost

## 1. Underlying goal

Elite Fun Counter Strike NPCs should behave like relentless, coordinated human
players: continuous fighting, fast site/plant/defuse decisions, legal movement
technique use, low accidental wall/stuck time, and measurable per-round results.
The iteration goal is equally important: a change must reach a pinned runtime
journal quickly enough to tune against evidence. Hot game-DLL reload is one
accelerator, not the only method.

Per-round measures that define success:

- NPC-vs-NPC kills, time to kill, kill intervals, kills per round.
- Average and burst travel speed, dash, down-dash, bhop, air time, freeze.
- Wall contact or near-wall avoidance, blocked motion, stuck episodes per
  NPC-minute.
- Plant latency, defuse latency, defuse completion, round outcome.
- No force-hits, extra ammunition, hidden health, wall-fire, or bypassed
  movement/weapon/interaction rules.

## 2. Canonical issues

| Issue | Scope | Status | Notes |
|---|---|---|---|
| `issue-be1adb51b6ac` | FCS objective/defuse/speed regression | in_review | Primary FCS issue; contains most agent reviews and run evidence. |
| `issue-b18e63894585` | Elite NPC corner traps and weapon underuse | new | Juggernaut/TDM and shared navigation evidence. |
| `issue-0a2a3fc00768` | FCS NPC aim/TTK too fast | assigned | Older aim-error guidance; see decision log for supersession. |
| `issue-04bce0df0d33` | FCS corner/stuck duplicate | deferred | Duplicate of `issue-be1adb51b6ac`. |
| `issue-cb16e118f63c` | FCS stuck/progress failure | new | Additional stuck evidence. |
| `issue-82cdc1277704` | NPC target visibility/behavior behind walls | in_review | Reuse for target/route evidence. |
| `issue-1b2463da5666` | Juggernaut NPCs fail to fire | in_review | Shares wall/pathing evidence. |
| `issue-8bfc1ca56222` | 67-NPC runtime below 60 Hz | assigned | Server NPC cost and tick budget. |
| `issue-cd492e4faa08` | FCS NPC targeting after a kill | triaged | Hostility/retention after death. |
| `issue-adfeaafa1136` | Human melee vs NPC in FCS | assigned | Contact/tick-epoch issue; combat proof dependency. |
| `issue-196c3045b4cb` | Header fan-out, NPC tranche first | new | Build-latency work; first NPC flag split is built. |

Do not create another FCS NPC issue without checking this table first. Append to
the canonical issue.

## 3. Evidence index

| Run / generation | Mode / map | Journal | Key evidence |
|---|---|---|---|
| `20261122_081330_fcs-route-proof-applied-fallback` gen478 | FCS, `dust2cyberiav4` | `logs/2026-11-22/20261122_081330_fcs-route-proof-applied-fallback/server-01/events.jsonl` | 2,223 stuck episodes; 1,156 progress failures; 1 plant; 8 NPC-v-NPC kills; 0 defuse starts; 48 team reports. |
| `20261122_150906` gen500 | FCS, `dust2cyberiav4` | `logs/2026-11-22/20261122_150906/server-01/events.jsonl` | 1,424 s; 6.80 m/s sparse estimate; 2,986 stuck; 7 NPC-v-NPC kills; 9 plant starts; 3 plants; 8 defuse assignments; 1 defuse start interrupted; 0 defused. |
| `20261122_174842` gen507 | FCS, `dust2cyberiav4` | `logs/2026-11-22/20261122_174842/server-01/events.jsonl` | 8,194 movement samples; 2,792 stuck; 2 plants; assigned CT 32.1 m and 212.7 m from bomb; 0 defused. |
| `20261123_164421` gen564 | FCS, `dust2cyberiav4` | `logs/2026-11-23/20261123_164421/server-01/events.jsonl` | Phase 0 baseline: CT 11.83 m/s, dash 17.52/min, bhop 2.03/min, stuck 21.90/min; T 10.15 m/s, dash 10.85/min, stuck 15.46/min; 26 NPC-v-NPC damage kills; TTK mean 2.40 s; no completed defuse. |
| `20261123_172947` gen567 | FCS, `dust2cyberiav4` | `logs/2026-11-23/20261123_172947/server-01/events.jsonl` | Phase 1 proof: 4 T plants, 4 CT defuses, every sampled `objective.defuse-start` `operator_kind=npc`; CT 12.31 m/s, dash 15.56/min, bhop 2.46/min, stuck 19.72/min. |
| gen568 | FCS build | pending | Phase 2 elite profile + bhop hold. DLL `72ddbf4a81f99ba34e1bba4cb2eb403afa3744791b3f4172f5f27b6fb8f276d5`. Two test runs failed readiness under load. |
| gen571 | FCS build + run `20261123_205333` | proven for Phase 2 | Adds canonical `network.mode.active` and NPC debug-flag header split. DLL `11c1f7b7af6b1df5577246877de9f23efb96deca02850032aeb0d63271271005`; build 829.795 s. 3 CT defuses; TTK mean 1.08 s/median 0.45 s; CT dash 25.43/min, bhop 3.76/min, air 26.1%; stuck not yet reduced. |
| gen826 | TDM `20261128_181614` | proven for shared focus-fire | 274 `squad_focus` target changes vs 20 `module_scored`; 42/42 team-second samples with >=2 sampled members had 100% of sampled members on one `server_target_id`; enemy team targeted human actor 1 in 106 target changes (most-targeted); 76 NPC-vs-NPC damage events; focus hold cut changes from 4,248 to 630 in comparable 130 s windows. Persisted: `.dev/evidence/20261128-tdm-focus-fire.json`; proof `task-proof-a52f080c35e5`. |

## 4. Problem to owner map

| Symptom | First wrong or missing step | Owner candidates | Evidence |
|---|---|---|---|
| CTs do not defuse | Designated defuser could be a human; only one NPC was allowed | `src/network/server-gamemode.cpp:7795-7866`; `src/npc/npc-utility.cpp:142-152` | Fixed in gen567; proof above. |
| Plant/defuse interrupted by combat movement | Utility goal/state machine moved the operator | `src/npc/npc.cpp:651-667` (objective lock), `src/npc/npc.cpp:3536-3545` (hold) | gen567 proof. |
| CTs stuck at spawn/corners | Route commitment/area escape/local correction can loop; no blocked-edge memory | `src/npc/npc-navigator.cpp`; `src/npc/npc.cpp` wall avoidance/stuck recovery | gen478/gen500/gen507/gen564 stuck counts. |
| Passive or cornered NPCs in TDM/Juggernaut | Target retention/pursuit and route progress fail | `src/npc/npc-state-machine.cpp`; `src/npc/npc-navigator.cpp`; `src/npc/team-brain.cpp` | `issue-b18e63894585`. |
| Long or zero-distance Recast stalls | Route query runs even at the goal; long query can exceed 1.5 s | `src/npc/npc-navigator.cpp:1456`; `src/npc/recast-navigation.cpp:1500-1595` | 7,032 ms zero-distance query; 1,492 ms long query; zero-distance guard proven on gen500. |
| Aim/TTK too fast or too slow | FCS profile overrides elite aim/reaction; per-mode physical arm aim not mapped | `src/npc/npc-combat.cpp:166-180`, `:303`, `:781-783`; `src/npc/npc.cpp:4162-4163`; `config/behavior-profiles.json` | `issue-0a2a3fc00768`; see decision log. |
| Readiness/active-mode test failures | Runner waits only 45 s wall time; client can still be in intermission under tick stalls | `devscripts/dev_loop/parallel_test_runs.py:688-691`; `src/network/community-match-client.cpp:557-559` | Failed run `20261123_182722`: phase 4 intermission, server tick stalls to 1,494 ms. |
| Slow edit-to-evidence | `npc.h` fans out to 143 sources; `player.h` to 243; gen568 build 1,756 s | `src/npc/npc.h:72` (`Player body`); `src/entities/player.h:556` | `dependency_graph.py`; gen568 build summary. |

## 5. Decisions

### Difficulty and aim (2026-11-23, user confirmed)

- Start at maximum legal difficulty and tune down; nearly unbeatable is the
  starting point.
- Keep `reaction_delay: 0.2` (200 ms acquisition).
- Use true elite aim/turn (`0.06°`, `1800°/s`); remove the FCS aim handicap.
- Use a 3-tick (50 ms) accepted-shot floor.
- Keep `information_mode: memory`; elite FCS CTs split and report, no
  through-wall radar.
- This supersedes the older `issue-0a2a3fc00768` recommendation of a 2–4° aim
  error plus a 200 ms shot floor for FCS. Link both issues so agents do not
  restore the old handicap.

### Defuse policy

- Designated defuser prefers a living NPC.
- Keep the designation stable while defuse progress is active.
- Reassign on death or a meaningful reachability advantage.
- A human may always start or finish a defuse; the designation never blocks a
  human.
- The operator holds position; a hazard may interrupt.
- Other living CTs cover.

### Unit scale

The human's intention is one world unit = one metre. Current engine evidence
(capsule 3.6 units = 1.8 m; 20-unit block in 1 s at 20 unit/s) indicates a 2:1
convention. Metrics report raw units and provisional metres at 0.5 m/unit.
Correct only with a measured player probe; retain raw values.

### Header latency (2026-11-23, user confirmed)

- Start with NPC/FCS headers only; log a broader header-fanout issue for all
  headers.
- Use a hybrid boundary: hot simulation fields stay by value; cold
  animation/network/render/cosmetic state may move behind pointers or separate
  headers.
- Cold-header work (`structured-log.h`, `game-host-abi.h`) is a separate
  one-time phase after the NPC tranche is measured.

## 6. Solution frontier

These are deliberately broader than "change a constant." Each needs a metric
target and evidence plan before it becomes a task.

| Option | What it solves | Expected measure | Risk / first check |
|---|---|---|---|
| Journal-derived regression predicates + `npc-match-report.py --compare` | Repeatable behavior gates | TTK, kills/round, stuck, defuse completion | Low; needs thresholds. |
| Offline/config counterfactual search | Tune utility/difficulty without rebuilds | Same metrics; fewer live iterations | Needs deterministic replay or headless batch. |
| Human movement replay / behavior cloning | Real bhop, air-strafe, juke primitives | Air speed gain, dodge success | Spec sketches it; needs recordings. |
| Data-authored move library | New movement techniques as JSON | Technique use per minute | Executor still needed. |
| Stuck heatmap + blocked-edge memory | Bad map/route cells and repeated failures | Stuck/min, near-wall ratio | Use existing journals; low risk. |
| Single movement arbiter | Navigator/combat/avoidance fighting | Oscillation, net progress | Shared movement risk. |
| Recast query budget/cache/async | 1.5–7 s route stalls | Tick stall, snapshot gap | Existing evidence; high impact. |
| Warm pinned runtime + readiness gate | False test failures and startup cost | Edit-to-evidence time | Tooling only. |
| NPC header fan-out reduction | Rebuild time after edits | Affected sources, build seconds | Behavior-neutral; see header plan. |

## 7. Open questions

- Exact metric thresholds for "good enough" after the gen568 comparison.
- Whether elite air-strafe/rocket-jump should be visible in normal play or only
  in travel/escape contexts.
- Which map cells are actually bad geometry versus route-choice failures.
- Whether the engine scale should be changed to true 1:1 or only the metric
  calibration should change.
- Whether the cold-header split is worth the one-time EXE cost after the NPC
  tranche is measured.

## 8. Readiness and header progress (2026-11-23)

### Readiness

The failed gen568 readiness run was in `DUEL_PHASE_INTERMISSION` (phase 4), not
active. Its server tick stalls reached 4,494 ms, so a 45 s wall-clock wait was
invalid. Source now emits canonical `network.mode.active` on the client when
`DUEL_PHASE_ACTIVE` is applied (`src/network/community-match-client.cpp`). The
isolated-run runner (`devscripts/dev_loop/parallel_test_runs.py`) waits for that
event (or the legacy `network.mode.start` `stage=active`) with a simulation-time
budget and a server-health gate, and reports `server_stalled` with tick range,
maximum stall, and over-100 ms percentage instead of a false mode failure.

### Header tranche

First NPC-only split is built in generation 571:

- NPC debug flags moved from `src/config.h` to `src/debug/config-npc.h`.
- Consumers updated: `debug-visuals-scene.cpp`, `engine-tick-render.cpp`,
  `npc.cpp`, `render-player.cpp`, `debug-commands.cpp`,
  `terminal-debug-toggles.cpp`.
- `dependency_graph.py --affected src/debug/config-npc.h` = 6 sources, versus
  284 for `src/config.h`.
- Build: generation 571, success, 829.795 s, SHA-256
  `11c1f7b7af6b1df5577246877de9f23efb96deca02850032aeb0d63271271005`.
- This is compile/build evidence only; no behavior change is claimed.

Next header steps: `npc.h` metrics/state/system split, `physics/config.h`
player-dimension split, then the `player.h` hybrid boundary. Full fan-out
tracking is in `issue-196c3045b4cb` and
`docs/architecture/headers/header-hygiene.md`.

### Phase 2 result (generation 571)

Run `20261123_205333` proved the new readiness path (`network.mode.active`
phase 2) and measured the elite profile:

- 3 T plants, 3 CT defuses, CT winners 3/3.
- NPC-vs-NPC TTK mean 1.08 s, median 0.45 s (gen564: 2.40 / 0.84 s); kills per
  round 5.00 (gen564: 4.33).
- CT: 11.92 m/s, dash 25.43/min, down-dash 8.45/min, bhop 3.76/min, air 26.1%.
- T: 11.53 m/s, dash 22.04/min, down-dash 7.45/min, bhop 3.21/min, air 22.8%.
- Not yet met: air% 30%; stuck/min was flat for CT and worse for T; CT
  near-wall/blocked motion increased. These are Phase 3 targets.
- Defuse starts still restart repeatedly (18 starts, 15 interruptions, 3
  completions); reducing that churn remains open.

### Phase 3 trial status (2026-11-23/24)

- Measurement gold-behavior doc added:
  `docs/workflows/measurement-hypothesis-driven-improvement.md`.
- Phase 3B stuck gate: an episode now requires a static side-world contact.
  Gen572 reduced `npc.stuck-episode` 1,372→553 in a comparable early window,
  but the definition changed; real `npc.stuck-recovery` events stayed around
  31. Treat 553 as a cleaner wall-block signal, not a direct win.
- Phase 3A defuse churn trials:
  - Hold at 6 m (gen572): 41 starts / 38 interruptions / 2 completions.
  - Hold at 3 m (gen573): 19 starts / 19 interruptions in the sampled round.
  - Hysteresis latch (gen574): still 43 starts / 43 interruptions and an
    explosion.
  - Final source attempt: defuse travel lock in `src/npc/npc.cpp` — the
    designated defuser follows the navigation direction toward the bomb, stops
    inside 2.5 m, and suppresses jump/dash/down-dash/freeze.
- Gen574 movement metrics under load: CT 18.62 m/s, dash 266.97/min, bhop
  15.63/min, air 44.0%, stuck 10.26/min; T 18.25 m/s, dash 255.64/min, bhop
  13.65/min, air 40.2%, stuck 10.20/min. Not accepted as balance targets.
- Shared build blocker: `src/network/multiplayer-projectiles.cpp` uses
  `projectileTrailConfigFor` at :443 before its definition at :513. Build
  attempt `game-dll-20261123T234301570308Z-6c7a9c` failed there. The cross-agent
  signal tool (`devscripts/agent-signal.py`) found `melee-parity`, sent the
  question, and `hot-reload-latency-root` was told our files are READY but the
  shared batch is not.
- No gen575 runtime proof exists yet. Next: owner fixes the shared compile
  failure, root seals one Mode 3 batch with `src/npc/npc.cpp` and
  `src/network/server-gamemode.cpp`, then run pinned gen575 and measure defuse
  starts per plant, stuck, speed, dash/bhop/air, and TTK.

### Shared-build unblock and CPU evidence (2026-11-24)

- Opened `issue-d75f928fbd6f`: shared batch failed in
  `src/network/multiplayer-projectiles.cpp` at first use `:443` before the
  definition at `:513`; `melee-parity` did not reply.
- User authorized the minimal unblock. Applied a declaration-order-only
  forward declaration, build note `d57339951817`. Build attempt
  `game-dll-20261124T003901444604Z-3c8d4e` succeeded in 949.243 s and published
  generation 576, SHA-256
  `06f3229efcd629dd82e0ce017bd9ef4ddaa08ae92c24cf7e6e446cbb143c9eb5`.
- Two gen576 readiness attempts did not reach a usable proof:
  - `20261124_005506`: client exited before active mode after reaching phase 4
    and receiving NPC visuals.
  - `20261124_005656`: `server_stalled` gate reported tick 308→1083 in 51.8 s,
    max tick stall 409.3 ms, over-100 ms 5.7%. This confirms the new readiness
    classification works, but no gameplay proof exists.
- CPU coordination: `resource-profiler` reported on the preserved Juggernaut
  Horde run `20261123_195307` (67 NPC/1 player, server PID 22184):
  `performance.server-tick-window` sample 15, total avg 59.232 ms,
  p95/p99/max 131.759 ms, NPC avg 53.901 ms/max 129.369 ms, 26.7% of sampled
  ticks over 100 ms; `npc.phase-window` 1,027 updates, total 459.736 ms,
  physics 239.569 ms, collision 141.699 ms, decision 39.155 ms. Callsite
  estimates: `updateOneNpc` at `src/npc/npc.cpp:1580` 12.40% server CPU
  (~3.043% host); `doCollisions` at
  `src/physics/movement/physics-collision-dispatch.cpp:181` 5.33% (~1.308%
  host). These are partial 1/64 estimates, not full attribution. See
  `issue-be1adb51b6ac` and `issue-ed3ac775283d`.
- Cross-agent signal: `devscripts/agent-signal.py` and the protocol in
  `AGENTS.md`/`agent-coordination.md` were added so this kind of ownership
  question is automatic rather than guessed.

Additional gen576 attempt `20261124_010043`: client connected and received NPC
snapshots but stopped at `network.mode.start` phase 4; server reached
`round.active` at t=24.13. The runner reached 2407 ticks and reported the
client never observed active mode (max stall 781.6 ms). Opened
`issue-d2a8614db704` and signaled `melee-parity`, `hot-reload-latency-root`,
and `damage-lightning-backlog`. This is a shared network-mode replication
blocker.

Next: resolve the client phase-application blocker, then retry gen576 and
measure defuse starts per plant, stuck, speed, dash/bhop/air, TTK, and compare
to gen571.

### Human acceptance: Juggernaut Horde felt overwhelming (2026-11-24)

The user played against the NPCs in Juggernaut Horde and reported it was
overwhelming and “gold behavior.” This is positive human acceptance of elite
NPC pressure, not a numeric proof. Candidate pinned runtime:
`20261123_195307` (Juggernaut Horde, 67 NPC/1 player, server PID 22184,
`logs/2026-11-23/20261123_195307/server-01/events.jsonl`). See
`docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md`.

This becomes the positive baseline to preserve: future tuning must not make
Juggernaut Horde passive or easy again.
