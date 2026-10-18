# Melee authored-pose policy and spec audit

- UTC: `2026-10-18T03:52:59Z`
- Scope: clarify how Melee's avatar JSON poses override ordinary procedural pose layers and separate user-reported runtime issues from requirements.

## Specification changes

- The Melee spec now requires its dedicated movement profile, `hold_scaled` jump, and no legacy freeze/down-dash behavior. It records that the current profile configuration still enables those legacy abilities, so the requirement is not yet met by config.
- Added the Melee exclusive authored-pose contract: per-avatar Captain Falcon clips drive visible part transforms, procedural/aimbody/weapon overlays are suppressed in this mode, actor-root movement remains with shared physics, and attached hitboxes use the same sampled pose.
- Replaced the informal session dump at the end of the Melee spec with categorized Melee presentation requirements, runtime reports requiring reproduction, and dev-loop observations that must not be mistaken for confirmed game state.
- Added the reusable `authored_only` composition policy to the animation spec. Other modes retain the default layered policy.

## Source audit

- `src/entities/player-animation.cpp` binds and samples avatar-specific clips, then still composes weapon/reload, freeze/dash, idle motion, and AimBody transforms. The current pipeline therefore does not implement the new Melee exclusive-pose contract.
- `assets/avatars/captainfalcon/animations.json` exists but contains only a partial clip set with sparse keyframes; presence of the file does not prove a complete Melee animation-state implementation.
- `docs/specs/animations/blender-animation-authoring.md` remains a proposal; this change does not claim that a Blender exporter exists.
- `config/movement/movement-smash.json` currently enables dash, down-dash, and freeze. Runtime behavior was not exercised for this documentation update.

## Evidence boundaries

- No code, config, build, or runtime behavior was changed or validated in this documentation pass.
- The crash/tick-stall, reconnect, shield-effect, hitbox-visualization, and map/dashboard reports remain un-reproduced reports and need fresh runtime journal/process evidence.
