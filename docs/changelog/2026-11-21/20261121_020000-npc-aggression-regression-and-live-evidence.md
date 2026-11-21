# NPC aggression regression and live evidence

## Source
- `src/npc/npc.cpp`: restrict optional recovery dashes to usable, progressing objective routes; avoid repeatedly dashing into a failed/stuck corner recovery.
- `config/behavior-profiles.json`: make elite squad aim, weapon rotation, jitter, and switching more aggressive.
- `devscripts/dev_loop/parallel_test_runs.py`: replace slow PowerShell/WMI process identity lookup with the Win32 process APIs so isolated test launches do not time out.

## Build
- Root dev-loop game DLL build succeeded: attempt `game-dll-20261120T234259888678Z-a5067c`, generation 417, SHA-256 `95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644`.

## Runtime evidence
- Pinned FCS isolated run `20261121_010727` came online with generation 417. Its journal contains CT objective routing and target-sensing, and T objective routes; it also contains no-progress and stuck episodes, so the navigation regression is not proven fixed.
- Pinned Juggernaut Horde run `20261121_011815` came online. Its server journal shows accepted shots by both teams, but no player-target selection in the sampled window; this does not prove the requested swarm behavior.
- Dev-loop stop requests for both isolated runs returned `stop_partial` because process identities did not match their manifests. No process was forcibly terminated.

## Tracking
- FCS regression: `issue-be1adb51b6ac`.
- Juggernaut Horde player targeting: `issue-b95574b63f29`.
- Elite NPC objective/aggression defaults and independent review task: `issue-b18e63894585`.
- Isolated-run process identity and cleanup evidence must remain open for follow-up; the failed stop proof is recorded here and should be added to the identity issue.

Build success is separate from live behavior proof. Human acceptance remains outstanding.
