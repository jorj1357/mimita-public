# Task

- Task ID: client-server-only-source-audit
- Summary: Trace standalone/offline gameplay paths beginning at `src/engine/engine-tick-state.cpp`.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-23T04:25:45Z (2026-10-23 00:25:45 EDT)
- Branch: 20261011zt
- Base commit: cbfa4b0df4e56b6f217c45dbe986b49df599b50a
- Final commit: unchanged

# Pre-existing changes

- `git status --short` showed pre-existing edits in config/analytics.json, devscripts/dev-loop.py, docs/specs/dev-loop/dev-loop.md, docs/specs/open-gameplay-issues.md, multiple src/debug, src/engine, src/main.cpp, src/network, src/npc, src/perf files, untracked devscripts/perf-watch.py, many changelog files, and docs/regressions/2026-10-22/.
- This audit did not modify those files.
- Repository file added by this session: this changelog only.

# Requested behavior

Remove standalone/offline gameplay so live play uses client/server authority. Trace references from `engine-tick-state.cpp` and identify the dependent local-only paths.

# Specification alignment

- Current specification: `docs/specs/networking/networking.md`, section 1, lines 121-161.
- Exact requirement: “There is NO!!!!!!!!!! separate local-play gameplay architecture. There is only client/server gameplay.” Each session has one authoritative server and one or more clients, including solo play. “The server may run” on the same computer, another player's computer, a dedicated server, or through a relay. The spec also requires one gameplay simulation, with local input prediction and server authority.
- Interpretation: remove offline gameplay simulation and offline-only gameplay branches. Preserve client-local input, prediction, presentation, reconciliation, and a same-machine server when it still uses the client/server protocol.
- Decision still needed before a repo-wide deletion: whether same-machine/local-server launch and server visibility controls are in scope. The current spec permits same-machine servers. It does not specify whether all solo launches should auto-start a server preset or require an explicit server choice.
- The router refers to `docs/regressions/regressions-v1.md`, but that file exists only under `docs/regressions/archive-ignore/`; the current regression index is `docs/regressions/README.md`. No behavior regression was reported in this source audit.

# Exact implementation changes

## File: `src/engine/engine-tick-state.cpp`

- Lines/functions: `engineTickState`, game-state transition block around lines 116-243.
- Existing behavior: sandbox selection loads a world, directly resets/positions the player, then starts a `startLocalServer` and connects to `127.0.0.1`; if play begins without a loaded world and without `mpContext.active`, a default map is loaded and gameplay can proceed without an authoritative server; pending `DuelConfigResult` starts `gDuelManager` directly against local `Player`, `NpcSystem`, and `World` state.
- Desired behavior: all live gameplay entry points establish/use the authoritative server and connect as a client; no default-world-only gameplay or local DuelManager simulation.
- Exact content change: no source change in this audit.

## File: `src/network/net_mode.cpp`

- Lines/functions: usage text around lines 120-124 and no-argument process mode.
- Existing behavior: no arguments retain a normal single-player/menu launch flow; source comments mark it `TODO-DELETE (single-player)`.
- Exact content change: no source change in this audit.

## Dependent offline gameplay owners found by repository search

- `src/game/duel.cpp`, `src/game/duel-state.cpp`, `src/terminal/duel-commands.cpp`: local `DuelManager` lifecycle, scoring, HUD, and command entry.
- `src/engine/engine-tick-combat.cpp`, `src/engine/engine-tick-state.cpp`, `src/engine/engine-tick-ui-overlays.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`, `src/engine/engine-tick-camera.cpp`, `src/engine/engine-tick-replay.cpp`, `src/engine/engine-tick.cpp`, `src/game/game-cli.cpp`, `src/gui/menus/pause-menu.cpp`: update, end-state, UI, camera, replay, quit cleanup, and respawn hooks for that manager.
- `src/combat/death-system.cpp`: local DuelManager death accounting and respawn gates (lines 218-234, 408-412, 443-447).
- `src/combat/weapon-system.cpp` and `src/combat/weapon-rocket-launcher.cpp`: explicitly documented single-player projectile branches; comments state client prediction must remain while offline simulation is removed.
- `src/network/local-gameplay-readiness.h` and `src/network/multiplayer-context.h`: non-networked single-player/replay readiness bypass; the comments already say to remove the gameplay bypass while retaining replay support.
- `src/network/net_mode.cpp`: no-argument standalone flow and single-player usage string.
- `src/network/server.cpp` and `src/network/server-packets.cpp`: loopback-only listen-server binding, `LOCAL-*` room identity, and coordinator/ICE bypass. These are still server/client protocol paths, but are local-reachability-only.
- `src/engine/engine-tick-state.cpp`: the sandbox server path is localhost client/server (not offline simulation) despite being labeled “local-only”; it still contains direct client-side spawn/world setup that should be reviewed against authoritative spawn ownership.

# Diagnostics

- Not applicable; source ownership audit only.

# Validation

- Focused skill paths: `docs/skills/spec-behavior-review-v1.md` (read; source/spec comparison performed); `docs/skills/terminal-command-checker-v1.md` (read for command ownership implications).
- Repository search: `rg` across `src` for `TODO-DELETE (single-player)`, `local/offline`, `single-player`, and offline gameplay comments; call sites for `gDuelManager`; gameplay state assignments in `src/gui/gui-main.cpp`, `src/main.cpp`, and `src/engine/engine-tick-net.cpp`.
- Build status: not run; no source code changed.
- Runtime: not run; this was a source-only trace.

# Regression review

- Regression entry appended: no.
- Reason: no newly observed behavior failure; this is an architecture cleanup audit.

# Human acceptance

- No gameplay, multiplayer, or visual behavior was changed or accepted.
- Still unverified: the desired replacement for no-argument/solo entry and whether loopback local-server presets remain available under “nothing local.”

# Related feature record

- None found for this broad architecture cleanup request.
