# Hot movement presentation bridge — session record

UTC: 2026-09-23T17:25:00Z  
Display timezone: America/New_York  
Branch: `8292026stash`

## Result

`PASS_WITH_HUMAN_REVIEW` for the hot-DLL slice available in the working tree.

The repository already contained the generic hot effect/audio contracts and the
hot movement presentation recipes for ground jump, air jump, dash, down-dash,
landing, freeze, freeze trail, and footsteps. This session preserved those
existing changes and added the NPC dash caller migration plus matching hot
combat self-test coverage.

## Changes made in this session

- `src/npc/npc.cpp`: NPC dash now submits the generic
  `effect.movement.dash` request so the hot presentation owner composes both
  the dash visual and sound. The old `EffectPart`/audio path remains only as a
  compatibility fallback when the active hot generation does not handle it.
- `src/network/hot-combat-selftest.cpp`: air-jump now tests the combined hot
  visual/audio request; dash and landing hot ownership are asserted; unknown
  movement facts remain unhandled so cold fallback ownership cannot duplicate.

## Existing hot implementation confirmed

The current working tree already contains the hot movement presentation bridge
in `src/entities/player.cpp` and the recipe implementation in
`src/hot-reload/modules/presentation/effect-composition.cpp`. Those files were
not claimed as new edits in this session.

## Validation

- `python devscripts/live-build.py`: generation 24 `Status: ok`; candidate:
  `build/hotreload/mimita-live-g000024.dll`.
- The live build did not write or replace `MiMITA.exe`.
- `git diff --check`: no whitespace errors; Git reported only existing
  line-ending normalization warnings for the two edited files.
- `python build_agent.py`: failed before producing a usable new executable due
  to an unrelated pre-existing declaration conflict in
  `src/network/server-players.cpp` (`debug::Event lifecycleEvent` conflicts
  with `ActorSpawnEvent lifecycleEvent` around lines 443–470). The existing
  running process was not closed or restarted.
- Existing executable `--hot-combat-selftest` was run, but it loaded the older
  active DLL rather than generation 24. It therefore is not proof of the new
  recipe assertions; its unrelated pre-existing animation failures remain.

## Human review still required

- Activate generation 24 in the already running game and confirm the NPC dash
  visual/audio path live.
- Build a new timestamped executable after the unrelated
  `server-players.cpp` conflict is resolved or its pre-existing edit is
  integrated.
- Perform in-game visual/audio acceptance for dash, down-dash, jump, air jump,
  landing, freeze, footsteps, replay, and multiplayer presentation.

## Routed documents and focused review

- `docs/ROUTER.md`
- `docs/specs/effects/effects.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
