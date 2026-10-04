# NPC radar target information

Date: 2026-10-04 16:15 EST
Branch: `afad20a-rebuild`
Result: PASS_WITH_HUMAN_REVIEW

## Request

Give NPC behavior profiles configurable target information through cover while
keeping weapon line of sight authoritative. Rage2 should use perfect radar,
remember target movement history, and pursue players and NPCs.

## Changes

- Added profile settings for `information_mode`, radar delay, radar error,
  radar memory mode, radar memory duration, remembered path points, and
  predicted-path continuation.
- Added direct comments and possible values to
  `config/behavior-profiles.json`.
- Configured Rage2 with `perfect_radar`, zero delay, zero error, persistent
  memory, 32 recent target samples, and predicted pursuit.
- Reused the existing per-NPC target history ring for delayed radar and path
  history, avoiding a duplicate history subsystem.
- Added radar knowledge to perception memory without marking the target as
  visible, so radar can guide movement but cannot authorize shooting through a
  wall.
- Unified hidden-target navigation around the visual/radar memory position and
  reset target history when the authoritative target changes.
- The server's existing mirror target path means the same radar behavior
  applies to NPC-vs-player and NPC-vs-NPC targets.
- Added `--npc-radar-selftest` to verify profile loading, Rage2 settings, and
  the radar-knowledge-versus-wall-vision separation.

## Validation

- `python build_agent.py`: BUILD SUCCESS; executable linked.
- `mimita.exe --npc-radar-selftest`: PASS.
- `mimita.exe --npc-perception-selftest`: PASS.
- `mimita.exe --npc-search-behavior-selftest`: PASS.
- `mimita.exe --npc-movement-policy-selftest`: PASS.
- `git diff --check`: no new whitespace errors in the task files.

## Human review still required

Run Sandbox with `npc_behavior_profile` set to `rage2`. Hide behind one or
more walls and verify the NPC routes toward the hidden target, does not fire
through cover, reacquires line of sight, and behaves the same when its target
is another NPC. Confirm that changing the profile JSON hot-reloads the radar
settings for living NPCs.

## Pre-existing work preserved

Existing unrelated worktree edits were preserved.
