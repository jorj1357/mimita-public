# Dev-loop full launch-mode number entry

- EST timestamp: 2026-10-10 18:04:17 -04:00
- UTC timestamp: 2026-10-10T22:04:17Z
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Updated `devscripts/dev-loop.py` so launch-mode selection is line-based rather
than single-key based. The picker now accepts all numeric characters, waits for
Enter, supports multi-digit IDs such as `10`, supports Backspace while typing,
and keeps `Q` as cancel. Numeric mode listings are also sorted numerically so
mode 10 appears after mode 9.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Build: not run; this change is confined to the Python dev-loop UI/input path.
- Runtime: not exercised interactively because an existing user-owned
  dev-loop/game session is active and was not interrupted.
- Human review: restart the dev-loop normally, press `2`, type `10`, press
  Enter, and confirm the status reports mode 10 before pressing `1` to launch.

## Pre-existing work

All unrelated modified and untracked files were preserved, including the prior
Zombie Tower launch fix and existing changelogs.
