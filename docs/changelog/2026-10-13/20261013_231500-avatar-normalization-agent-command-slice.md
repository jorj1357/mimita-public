# Avatar normalization and deterministic agent-command slice

Date: 2026-10-13

## Scope

Added an opt-in, player-model-only normalization path for authored GLB body
parts, structured avatar transform evidence, and the first deterministic
in-game command surface for live diagnostics.

## Documentation

- Expanded `docs/specs/avatar/avatar-editor.md` with the confirmed Boomer v21
  versus Stabber v6 transform differences, normalization invariants, export
  rules, and required `events.jsonl` evidence.
- Added the avatar evidence profile to
  `docs/specs/debug-logging/AI-fix-live.md`.
- Added deterministic command contracts and the current implementation
  boundary to `docs/specs/dev-loop/dev-loop.md`.

## Source changes

- Added `body_frame_policy` with the backward-compatible default
  `authored`.
- Added `player_canonical` normalization for custom player models. It
  canonicalizes the six direct body-part nodes, bakes the correction into
  mesh position/normal data, preserves the authored rest pose, and rejects
  malformed or shared-mesh layouts with a structured fallback event.
- Opted the Boomer test avatar and Stabber v6 test avatar into the policy.
  Existing avatars without the field retain the authored path.
- Added structured raw-node, normalized-node, transform-source, pose-sample,
  command-dispatch, and command-assertion evidence.
- Added `agent.move`, `agent.aim`, `agent.action`, `agent.shoot`, and
  `agent.snapshot` command handlers. Scenario files and automated assertion
  queues remain a later slice.

## Build evidence

- `python build_agent.py`: passed; one touched translation unit compiled and
  the executable linked successfully.
- The dev-loop published build 2214 at
  `C:\mimita-v9\.dev\builds\2214\mimita.exe`.

## Runtime evidence

- The temporary command exercise recorded startup arguments and
  `startup.command.dispatch` in
  `logs/2026-10-13/20261013_231220/events.jsonl`.
- The subsequent clean dev-loop run recorded startup arguments and readiness
  dispatches in
  `logs/2026-10-13/20261013_231319/events.jsonl`.
- Neither journal contains the expected `avatar.command.*`, `avatar.model.*`,
  or `avatar.pose.sample` events. Therefore command-handler execution,
  normalization execution, and visible arm-rotation acceptance are not yet
  proven. The next investigation should trace why startup dispatch reaches the
  command surface without producing the handler-level events.

## Regression and review status

- No confirmed regression was identified.
- Human visual/gameplay acceptance is still required for Boomer, Stabber, a
  legacy authored avatar, and a current player avatar.
- Pre-existing unrelated working-tree changes were preserved, including
  configuration files, `docs/jorj-docs/20261013plan.md`, untracked Stabber
  assets, and other pre-existing changelog/spec files. The existing trailing
  whitespace warning remains at `docs/jorj-docs/20261013plan.md:17`.

