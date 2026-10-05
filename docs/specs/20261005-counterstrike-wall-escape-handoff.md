# Counter-Strike wall-escape recovery — HANDOFF

Date: 2026-10-05 (UTC)
Branch: `afad20a-rebuild`
Commit at handoff: `a1bcea51` (working tree is dirty; see §6)
Status: **PARTIAL** — Option A implemented and building. Option B deferred to
the human. Diagnostics added but not runtime-verified. No changelog or feature
attempt entry written yet.

Read `AGENTS.md` and `docs/ROUTER.md` first. This document does not replace them.

---

## 1. Mission

The human plays `counterstrike` on `dust2cyberiav4`. Terrorist NPCs eventually
leave spawn, but **Counter-Terrorist NPCs run straight into the wall directly in
front of spawn and stay there**. Sandbox NPCs navigate the same map well.

The human chose **Option A only**: the wall-escape recovery must be a physical
escape that BYPASSES the actor movement policy. **Option B** (authoring real
bomb-site positions so the CT objective target exists) is left to the human.

Human constraints for this task (do not re-litigate):

- "just do A" / "it should bypass" — the escape must not be gated by policy.
- Do **not** observe the game live. Report build + tests only.
- Add diagnostics that write to a `.jsonl` events file, and read that file
  while the `.exe` runs.
- Record the regression in the **existing** Counter-Strike feature doc
  (`docs/features/gamemodes/counterstrike.md`), not a new `docs/regressions/`
  file. (Note: a regression file DOES already exist for a prior, related issue —
  see §7. The human's instruction was about not creating a *new* file this time.)

---

## 2. Root cause (evidence-backed)

Counter-Strike NPCs have `actorPresetId="counter_strike"`, which resolves BOTH a
navigation block and a movement policy (`activeNavigationSettings()` and
`activeMovementPolicy()` are non-null). Sandbox NPCs have neither (both return
`nullptr`).

Two escape paths in `src/npc/npc.cpp` were gated by `policy != nullptr`:

- Stuck-jump recovery (`if (!policy) dash = ...` and the jump branch).
- Wall backtrack (`if (stillBlocked && navCfg.wallBacktrackEnabled)` was behind
  a `!policy` guard).

So a policy actor that spawned facing a wall only **turned** and never backed
away or chose an open escape direction. Sandbox actors (no policy) escaped.

Contributing findings:

- CS CT spawn yaw is hard-coded `0.0f` (T is `π`) in
  `src/network/server-gamemode.cpp` around the `:1607` region, independent of
  the GLB spawn node's forward. This seeds `patrolDir` into the wall.
- `config/maps/dust2cyberiav4.json` had **no** bomb-site positions, so
  `TeamBrain::objectiveTargetPosition()` returned false and
  `ctx.objectiveKnown=false`. But `ctx.onDefense=(team==0)` is set
  unconditionally (`server-gamemode.cpp` ~`:2969`), so the CT goal became
  `DefendSite` (utility 0.7) which beats the target-less `KillTarget`; and
  `makeNavGoal` does not map `DefendSite`, so it falls through to the legacy
  state machine (`Patrol`).
- `updatePatrolHeading` commits a heading for
  `searchHeadingCommitSeconds=10.0s` (`config/npc-difficulty.json`), so a bad
  initial heading persists for a long time.
- Map collision bounds (from `logs/map_spawn_debug.txt`):
  min=(-971.14,-147.56,2295.31) max=(-571.14,257.20,2441.31).
  T spawn=(-604.895,28.294,2366.225); CT spawn=(-878.37,122.62,2349.30).
- "Wall in front of CT spawn" is supported by the human report and three
  changelog attempt notes, **not** by an extracted GLB triangle read (the wall
  triangles were not extracted).

---

## 3. Work completed (DO NOT REDO)

All edits are in the working tree and **compile** (`BUILD SUCCESS`,
`mimita.exe` relinked). They have **NOT** been runtime-verified and there is
**NO** changelog / feature-attempt entry yet.

### A. Option A — policy-bypassing wall escape (`src/npc/npc.cpp`)

- Added `#include "debug/structured-log.h"` (line ~24).
- Backtrack escape (line ~1567) is now:
  ```cpp
  if (stillBlocked && navCfg.wallBacktrackEnabled)
  ```
  i.e. the `!policy` guard was removed. The comment above it states the escape
  BYPASSES policy on purpose, and that `blocked_behavior` still shapes steering
  (turn vs hold) while only the physical escape is unconditional.
- Stuck recovery (line ~1596) still has `if (!policy) dash = npc.dashCooldown...`
  (dash stays policy-gated on purpose), but the open-direction turn + jump +
  repath now run for every actor including policy actors.
- `policy` is still used elsewhere in the tail block, so it is not unused.

### B. JSONL diagnostics (`src/npc/npc.cpp` + `config/debuglogger.json`)

- Two `StructuredLogger::instance().writeEvent(...)` calls emit event
  **`npc.wall-escape`** with reason `"backtrack"` (line ~1579) and
  `"open_turn_repath"` (line ~1624).
  Fields per event: `actor`, `preset`, `team`, `policy` (the resolved
  `blockedBehavior` or `"none"`), `pos` (xyz), plus `blocked_dir` for backtrack
  and `stuck_seconds` for stuck. `tick` is `(uint32_t)(npc.sensors.time * 60.0f)`
  (there is no `movementSimulationTick` field on `Npc`). Correlation id is the
  actor id string.
- The stuck event fires **once per episode** via
  `if (npc.stateMachine.stuckTimer - safeDt <= 0.3f)` so it cannot flood the
  file.
- `config/debuglogger.json`: `npc_movement` level changed `off` → `important`.
  **This is required** because `writeEvent` begins with
  `if (!mInitialized || !mConfig.enabled || !shouldLog(category, level)) return;`
  (`src/debug/structured-log.cpp:836`). Without this the events are silently
  dropped.
- Where to read it live: `logs/<date>/<run>/events.jsonl` (the canonical JSONL
  sink; the per-category files are separate and `npc_movement` file_output is
  false).

### C. Bomb-site visuals (completed earlier in the session, before handoff)

`config/maps/dust2cyberiav4.json` was rewritten with **placeholder** site
positions A=(-700,60,2400), B=(-820,90,2300), `radius:4.0`,
`visible_debug:true` (all inside the collision bounds above). A comment notes
they still need `site_debug` verification. **These are guesses** and must be
refined by the human (Option B). Setting positions incidentally makes
`objectiveKnown=true`, which changes Option B scope (see §5).

---

## 4. Build / test evidence so far

- Build: `python build_agent.py` → `BUILD SUCCESS`, `Compiled: 1`,
  `mimita.exe` relinked.
- `--npc-navigation-selftest` → PASS.
- `--npc-movement-executor-selftest` → PASS (5 checks, incl. "CS NPC runs the
  shared Sandbox executor").
- `--npc-movement-policy-selftest` → **FAIL**:
  `FAIL NPC made a local lateral correction at the wall`
  (`lateral=0.49`). **This is almost certainly caused by Option A**: the test
  expects a policy actor NOT to make the local lateral correction, and we just
  un-gated that path. The assertion (in `src/game/game-cli.cpp`, ~line 456/462
  region, report string "NPC made a local lateral correction at the wall") must
  be reviewed and either (a) updated to expect the correction now that the
  behavior is intended, or (b) shown to be testing a different thing. **This was
  not resolved before handoff.**

Remote/human acceptance: not performed (human explicitly said do not observe
live).

---

## 5. Remaining work (ordered)

1. **Resolve `--npc-movement-policy-selftest` FAIL.** Decide whether the
   "local lateral correction" assertion is now obsolete. Quote the spec/intent,
   then update the assertion or the code — do not just loosen it blindly. This
   is the first thing a new agent should look at; it is the only failing
   automated check.
2. **Verify the `npc.wall-escape` events actually land in `events.jsonl`.**
   The selftests do not exercise the live NPC tick, so the events are unproven.
   Per the human: run the `.exe`, then read `logs/<date>/<run>/events.jsonl` and
   confirm `npc.wall-escape` records appear for CT actors at spawn. The human
   permits reading the JSONL; they do not want a live visual observation.
3. **Write exactly one session changelog** under
   `docs/changelog/2026-10-04/` (or `2026-10-05/` — use the UTC date of the
   final change), filename `YYYYMMDD_HHMMSS-<slug>.md`.
4. **Append one attempt entry** to
   `docs/features/gamemodes/counterstrike.md` (append-only `## Attempt log`).
   Include: the root cause, the Option A change, the JSONL diagnostic, the
   bomb-site placeholder note, and the regression record requirement.
5. **Regression record:** the human asked to record the regression in the
   existing Counter-Strike doc. A regression file already exists
   (`docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md`);
   append a follow-up there if a durable file is wanted, and/or summarize in the
   feature doc. Do not create a redundant new file without checking with the
   human.
6. **Do NOT do Option B** (authoring real bomb-site positions) unless the human
   asks. Note that placing real sites will flip `objectiveKnown=true` and change
   CT goal scoring, which interacts with this fix.

---

## 6. Repository state / environment warnings

Working tree is **dirty with unrelated concurrent work**. Do not attribute it to
this task and do not revert it:

- Modified (not all mine): `config/actor-presets/counter_strike.json`,
  `config/analytics.json`, `config/debuglogger.json`, `config/maps/dust2cyberiav4.json`,
  `config/npc-difficulty.json`, `devscripts/dev-launch-modes.json`,
  `docs/features/gamemodes/counterstrike.md`, `src/game/game-cli.cpp`,
  `src/gamemode/match-roles.cpp`, `src/network/server-npcs.cpp`,
  `src/npc/npc-movement-policy.*`, `src/npc/npc-navigator.cpp`, `src/npc/npc.cpp`,
  `src/npc/npc.h`.
- Untracked (not mine): `assets/maps/zombietower3.glb`, several
  `docs/changelog/2026-10-04/*` files, the regression file, and new
  `src/gamemode/*` + `src/npc/npc-movement-context.*` + `tests/*` files.

Environment notes:

- A background live-build watcher (python) may auto-recompile; a cold build can
  report `[SKIP]` / `NOTHING_CHANGED`. If an edit is not recompiled, delete the
  stale `.o`/`.d` under `build/obj-debug/` to force it. For this task the file is
  `build/obj-debug/npc_npc.o` / `.d`.
- Canonical build: `python build_agent.py` (cold build is allowed).
- OSCursor attribution: `WriteEvent` requires the category level gate (see
  §3B). This bit the first attempt.

---

## 7. Key files and line anchors

- `src/npc/npc.cpp` — Option A edits: backtrack at line ~1567, stuck at
  ~1596; `npc.wall-escape` events at ~1579 and ~1624; `structured-log.h` include
  at ~24.
- `src/npc/npc-movement-policy.h/.cpp` — `NpcMovementPolicy`,
  `npcPolicyAllowsJump`, `blockedBehavior`.
- `src/npc/npc-navigation-settings.h/.cpp` — navigation block schema/parser
  (`wallBacktrackEnabled`, `allowWallJump`, etc.).
- `src/npc/npc-navigator.cpp` — `bestTurnDirection` (~178),
  `startBacktrack` (~432), blocked handling (~650-663),
  `wallAvoidDirection`/`obstacleInDirection`.
- `src/npc/team-brain.cpp` — `objectiveTargetPosition` (~148), CT assignment
  DefendSite (~110-134).
- `src/npc/npc-utility.cpp` — `scoreUtilityGoal` DefendSite (107-111),
  Patrol reclassify (191-195).
- `src/network/server-gamemode.cpp` — CT spawn yaw (~1607), objective context
  push (~2957-2982), `writeEvent` example (~297-302).
- `src/debug/structured-log.cpp` — `writeEvent` (825), level gate (836).
- `config/debuglogger.json` — `npc_movement` (line 35) now `important`.
- `config/maps/dust2cyberiav4.json` — placeholder bomb sites + `visible_debug`.
- `src/game/game-cli.cpp` — `--npc-movement-policy-selftest` and its
  "local lateral correction" assertion (search the report string).
- `docs/features/gamemodes/counterstrike.md` — append attempts here.
- `docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md` —
  existing related regression file.

---

## 8. Commands

```text
# build
python build_agent.py

# tests
.\mimita.exe --npc-navigation-selftest
.\mimita.exe --npc-movement-executor-selftest
.\mimita.exe --npc-movement-policy-selftest
.\mimita.exe --counterstrike-acceptance-selftest

# read diagnostics during a run (per human instruction; do not eyeball visuals)
Get-Content logs\<date>\<run>\events.jsonl | Select-String "npc.wall-escape"
```
