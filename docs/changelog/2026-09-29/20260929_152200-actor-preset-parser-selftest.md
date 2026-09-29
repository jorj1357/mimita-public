# Actor preset parser self-test

- Added `--actor-preset-selftest`, a headless check using the same actor-preset registry as the game.
- Fixed schema parsing so object-shaped `team.allowed` data is not passed to the legacy numeric team reader.
- The test now verifies file discovery, preset count, `counter_strike` lookup, FOV 70, forced first person, movement preset, and weapon set.
- Validation: build 441 passed from both `C:\mimita-v9` and `.dev\builds\0441`, including directory resolution from the published EXE folder.
