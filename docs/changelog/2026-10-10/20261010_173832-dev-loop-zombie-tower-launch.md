# Dev-loop Zombie Tower launch-mode fix

- EST timestamp: 2026-10-10 17:38:32 -04:00
- UTC timestamp: 2026-10-10T21:38:32Z
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Finding

Launch mode 8 was selected in the run metadata, but the live pair used
`--map atdm --mode sandbox`. The journal confirms the first divergence:
`run.started` recorded launch mode `8` with map `atdm`, followed by
`map.server-load-success` for `assets/maps/atdm.glb`; the NPC then reported the
`sandbox` profile. The mode's `zombietower4` map was rejected by
`select_dev_map()` because it was absent from `config/gamemode-good-maps.json`,
and the already-running durable server was reused because the dev-loop compared
only executable and map, not the effective launch arguments.

## Changes

- `devscripts/dev-loop.py:191-202`: an explicit launch-mode map is accepted
  when its `assets/maps/<map>.glb` exists, so adding `"map": "zombietower4"`
  to a launch-mode entry is sufficient and does not require adding it to the
  generic rotation pool.
- `devscripts/dev-loop.py:1004-1024`: added normalized server-argument
  comparison, ignoring only the temporary room-file path.
- `devscripts/dev-loop.py:1074-1079`: durable server reuse now stops and
  replaces the server when the selected mode/profile/map launch contract
  differs.

## Documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/regressions/README.md`

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Focused Python checks: `zombietower4` resolves to itself when its GLB exists;
  missing maps fall back to `atdm`; temporary room-file paths normalize
  correctly. Passed.
- Build: not run; only the Python launch orchestration changed.
- Runtime: the supplied run remains evidence of the old behavior. The active
  dev-loop/game pair was not terminated or restarted during this session.
- Human review still required: restart the dev-loop normally, select mode 8,
  and verify the new journal has `map.server-load-success` for
  `assets/maps/zombietower4.glb`, Zombie Tower mode state, and the expected
  persistent NPC behavior. The reported ATDM cylinder/FPS issue is separate.

## Pre-existing work

The existing modified `config/accounts/default.json` was preserved. The branch
already contained the launch-mode and Zombie Tower changes from earlier work;
this session only corrected the dev-loop map/reuse behavior above.
