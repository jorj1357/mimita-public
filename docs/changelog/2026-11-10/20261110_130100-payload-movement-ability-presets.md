# Payload movement ability preset correction

- Recorded: 2026-11-10 08:01 EST (2026-11-10T13:01:00Z)
- Branch: `checkpoint/pre-live-kernel-20261108`
- HEAD before edits: `bc9343887c1b22850e0b055b5e36459a7680df2b`
- Pre-existing worktree edits preserved: `config/maps/infinite_xxity.json`,
  `config/worldgen/infinite_xxity.json`, `devscripts/blender-ai`, and
  `src/network/server-infinite-collision.cpp`. During this session additional
  unrelated edits appeared in `src/modules/api/network-policy-api.h`,
  `src/network/movement-validation.cpp`, and
  `src/network/network-policy-module.cpp`; they were left untouched.

## Finding and change

The user's screenshot shows `movement_print` reporting the global `source`
preset. That command did not print dash/down-dash/freeze switches and therefore
did not identify a role-specific movement preset. In Payload,
`config/gamemodes/payload.json` includes `payload_scout` in its class pool.
`config/roles.json` assigns `payload_attacker` and `payload_defender` to
`source`, but assigns `payload_scout` to `sourcefast`. Before this change,
`config/movement/movement-source-fast.json` enabled all three abilities. The
shared movement kernel checks the effective preset's flags, so Scout could
still use them while the base `source` preset had them disabled.

Changed `config/movement/movement-source-fast.json`:

- `dash_enabled`: `true` -> `false`
- `down_dash_enabled`: `true` -> `false`
- `freeze_enabled`: `true` -> `false`
- Updated the nearby comment to say these optional abilities are disabled to
  match `source`.

This change also disables those abilities for other roles using `sourcefast`.
Payload's other listed classes use `source` or `heavy`, whose three switches
were already false.

## Evidence and limits

- Static source/config trace: Payload class pool -> `payload_scout` role ->
  `sourcefast` preset -> previously enabled ability flags.
- The owner code gates dash, down-dash, and freeze activation on the effective
  `MovementConfig` flags in `src/physics/movement/movement-step.cpp`.
- Config hot reload is polled for the active player preset and the role
  movement cache. The game must still be running the relevant current code and
  the changed file must be saved successfully for live behavior to update.
- No build or live Payload run was performed. The user's screenshot is not
  runtime journal evidence. Human gameplay confirmation remains needed.
- The existing Payload run `20261110_031305` has the prior executable build
  `2026-11-09T21:44:40.463103Z`. Its server journal contains zero
  `npc.roster-profile-resolved`, `npc.dash`, or `npc.movement-decision`
  records, so it cannot establish the role or ability outcome from the user's
  observed run.
- Useful existing journal events: `npc.dash` records an NPC dash that actually
  activated; `npc.movement-decision` gives sampled NPC movement snapshots but
  does not include the movement preset or ability switches. Player dash,
  down-dash, and freeze attempts do not currently have a dedicated success or
  rejection event in the inspected path. `movement.server-position-corrected`
  is not an ability event and should not be used to prove this toggle.

## References reviewed

- `docs/ROUTER.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/movement/movement.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

## Validation and review

- Inspected the Payload class pool, referenced role presets, source/fast/heavy
  ability values, shared activation gates, and reload owners.
- No tests were requested or run. No build was run. No live runtime validation
  was performed.
- Human review should enter Payload as Scout and verify Shift dash, Q
  down-dash, and freeze no longer activate; repeat with an NPC Scout if that
  role is spawned. If a live journal is available, inspect `npc.dash` and
  `npc.movement-decision`; the current journal does not directly record
  disabled-ability rejections or the player's effective role preset.

## Follow-up: active isolated Payload run (2026-11-10 08:15 EST)

The exact user-provided run `20261110_124755` is running from
`.dev/agent-lanes/hot-session-20261109`, with the Payload game server in
`server-gui-81098652` (`mode:payload`, map `funworld3`). The base checkout
config is not the config copy read by that process. At inspection, the lane's
`config/movement/movement-source.json` and
`config/movement/movement-source-fast.json` both had dash, down-dash, and
freeze enabled. I changed those six lane-local values to `false` at 08:13:49
EST; no build was run.

The server journal segment began at 13:13:53Z, immediately after that save,
and continued through 13:14:56Z with 34,272 records. It contains zero
`npc.dash` events in this post-save segment, while NPC movement/decision
records continue. Earlier segments contained 300 `npc.dash` records. This is
consistent with the lane config taking effect for NPCs, but absence of a
once-per-life event over this short interval is not conclusive live gameplay
acceptance. Existing log fields also show dead NPC dash/down-dash serials being
cleared for snapshots; those historical serials do not prove a new activation
after the save. The journal does not provide a player ability success/rejection
event, so it cannot prove that player dash/down-dash/freeze inputs were
blocked. The user should retry in-game; if player controls still activate,
trace the active input path (including developer flight) separately.

## Run manifest follow-up (2026-11-10 08:19 EST)

The supplied `run.json` was updated at 13:18:33Z and reports lane client/server
PIDs 25336/26876 online in `mode:current`, with current journal activity. A
local process lookup did not find those PIDs, so the manifest's `online` state
is not independently confirmed. The separate Payload `server-gui-81098652`
journal last wrote at 13:14:56Z and its process PID 25984 was also absent at
lookup. Do not treat those process states as current or claim live acceptance
from the stale Payload journal.
