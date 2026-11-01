# Actor-support world contact, AI match validation, and journal rotation

Time created: 2026-11-01T19:49:06Z

## Problem

Dust2 TDM reports connected NPC geometry, player/NPC sinking after actor
contact, and enemies that stop firing. The actor-pair solver could mark a
supported actor as grounded and the player/NPC merge paths could treat that as
static-world contact while skipping the world solver. TDM actors also resolved
to an oversized `default` capsule, spawn poses were not validated after the mode
avatar replaced a placeholder, and the canonical journal dropped records at the
100 MB cap instead of rotating.

## Changes

- `src/physics/movement/actor-pair-collision.*`: added explicit
  `hasWorldContact`; support and carry set grounded support only; losing support
  no longer preserves actor-derived ground; added focused support self-test.
- `src/sim/simulate-tick.cpp`, `src/network/server-npcs.cpp`: re-run the
  static-world solver after any actor-pair correction, push, or support; keep
  world contact separate from actor support; emit bounded
  `actor.collision.support-world` evidence.
- `src/physics/movement/actor-final-pose.cpp`: clear-pose ground is recorded
  from real world contact only.
- `config/actor-collision.json`: `default` restored to player-like 0.7 m radius
  and documented push tuning.
- `src/network/server-npcs.cpp`, `src/npc/npc.*`: spawn-pose containment before
  first AI movement/combat; placeholder avatars are cleared before a mode's
  first avatar policy selection; `npc.spawn-pose.checked` evidence.
- `src/actor/player-ai.*`, `src/input/input-poll.cpp`,
  `src/engine/engine-tick-net.cpp`, `src/terminal/player-commands.cpp`:
  `player_control ai|human`; AI uses the normal input/ActorIntent boundary,
  prefers hostile teams, approaches/strafes, aims, fires, reloads, and avoids
  local walls. Human mode clears held input.
- `devscripts/dev-loop.py`: `--ai-match --ai-seconds` runs a bounded TDM match
  on Dust2 and requires contact/combat journal assertions.
- `src/debug/structured-log.*`, `src/devtools/dev-log-commands.cpp`: rotate
  `events.jsonl` to sealed `events-NNNN.jsonl` before 100 MB; active path stays
  `events.jsonl`; readers and dev-loop cursors read segments chronologically;
  pruning recognizes segmented runs; focused rotation self-test.
- `src/npc/npc.cpp`, `src/npc/npc-combat.cpp`: sensor time advances; target/LOS
  evidence; accepted-shot evidence; stale LOS reset on target change.
- `config/debuglogger.json`: NPC combat category enabled at important for the
  required shot/attack evidence.
- `collision_probe` command for distinguishing missing map collision from actor
  penetration.

## Evidence

- Exact executable: `C:\mimita-v9\.dev\builds\0152\mimita.exe`.
  `--versioninfo` reported `EVENTS_JSONL_PATH=logs/11-01-2026/20261101_144943/events.jsonl`
  (`run_id=20261101_144943`, PID 13896).
- Build 139: dev-loop build success.
- Runtime AI match 2026-11-01T19:29:44Z on `dust2cyberiav4` TDM, build 139:
  passed; 44 actor contacts, 258 accepted NPC shots, 4 NPC damage events, 324
  applied damage events.
- Build 152: `--actor-pair-support-selftest` PASS, including support is not
  world contact and lost support clears fabricated ground.
- Build 152: `--journal-rotation-selftest` PASS, including sealed segment,
  active truncation, generation, and segmented-run recognition.
- Build 152: `--actor-final-pose-selftest` PASS.
- Runtime AI match 2026-11-01T19:46:29Z, build 152:
  `logs/2026-11-01/20261101_194629/events.jsonl`; passed; 41 actor contacts,
  163 accepted NPC shots, 7 NPC damage events, 130 applied damage events.
  Avatar selection now shows empty previous for first mode selection; remaining
  non-empty previous entries are permitted keep-per-NPC updates. Player final
  pose resets dropped from 10 (`no_valid_pose`/`last_clear`/`safe_point`) to one
  `last_clear` in the shorter 45 s run.
- `collision.probe` at points near CT spawn on `dust2cyberiav4` recorded inside
  and ray results; the reported block still needs human visual confirmation and
  map-collision ownership review.

## Specs and records

- `docs/architecture/collision/collision.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/regressions/2026-11-01/actor-support-world-contact-sink-REG.md`
- `docs/regressions/2026-11-01/npc-spawn-avatar-placeholder-REG.md`

## Remaining acceptance

Human visual review of sinking, corner crowd behavior, the CT-spawn block, and
AI match presentation is still required. The support-distribution improvement
for simultaneous corner contacts remains a follow-up if visual review still
shows actors buried by a crowd.
