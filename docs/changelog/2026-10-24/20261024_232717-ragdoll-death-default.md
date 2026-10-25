# TDM death ragdolls enabled

- **Time:** 2026-10-24T23:27:17Z (2026-10-24 19:27:17 EDT)
- **Branch / HEAD:** `20261011zt` / `0472121b`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Finding

The working diff at task entry did not remove death-ragdoll spawning. In
`src/combat/death-system.cpp`, `DeathSystem::kill` still calls
`RagdollModeSystem::spawnCorpse` for the first death presentation of each life.
The working renderer diff in `src/render/render-player.cpp` removes a cap on
trail histories, not corpse rendering. The actual TDM suppression was an
existing explicit `"ragdoll_enabled": false` in `config/gamemodes/tdm.json`.
History identifies commit `bfa4b7bc` (`2026-09-10 01:13:30 -0400`) as the
change that introduced that false override. It added the JSON setting; it did
not remove the death-ragdoll spawn call.

In `logs/2026-10-24/20261024_231251/events.jsonl`, event
`client.gamemode.presentation.applied` at line 3417 reports
`mode=tdm, ragdoll_enabled=false`. The journal has six
`ragdoll.corpse.spawned` records before that TDM override and 14 subsequent
`ragdoll.corpse.spawn.rejected` records with
`reason=death_config_disabled` (first at line 14740). A corpse rejection also
records `ragdoll_enabled=true`, distinguishing the corpse solver setting from
the death-presentation override. The global `config/ragdolldeath.json` remains
enabled.

## Changes

### `config/gamemodes/tdm.json:19`

Before: `"ragdoll_enabled": false,`

After: `"ragdoll_enabled": true,`

Reason: TDM should show death ragdolls, so the mode must not suppress the
global default.

### `docs/specs/gamemodes/teamdeathmatch.md:31-37`

Added that TDM enables death ragdolls and that death ragdolls are shown by
default unless a gamemode explicitly opts out with `ragdoll_enabled: false`.
This preserves explicit opt-outs such as Melee.

## Review and validation

- Reviewed `docs/ROUTER.md`, `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md`,
  `docs/specs/graphics/graphics-presentation.md`,
  `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/architecture/live-development/ragdoll-live-network.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`, and
  `docs/regressions/regressions-v1.md`.
- `config/gamemodes/tdm.json` JSON parse: PASS.
- `git diff --check` for the two changed source-of-truth files: PASS (Git only
  reported line-ending conversion notices).
- No build or tests run; only JSON and specification files changed.
- The supplied journal is evidence of the prior behavior, not a runtime check
  after this edit. A TDM death on the updated configuration and human visual
  confirmation are still needed.

## Workspace state

The shared workspace already contained unrelated edits at task entry. They
were preserved. The initial dirty paths included account, analytics, audio,
GUI, hit-effect, player-visual, and video configs; dev-loop scripts;
collision/build/dev-loop docs; gameplay intake and regression docs; engine,
GUI, input, main, networking, physics-collision, and renderer sources; and
existing changelog/gold artifacts. Additional avatar and entity rendering
paths were also dirty on the final status check and were left untouched.
