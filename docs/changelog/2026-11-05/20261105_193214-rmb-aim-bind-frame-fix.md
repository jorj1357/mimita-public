# RMB Weapon Aim Bind-Frame Fix

- Time: `2026-11-05T19:32:14Z`
- Branch: `20261011zt`
- Scope: implement fixed-tick `agent.hold`, diagnose weapon-specific RMB aim, fix the shared arm-frame mapping, and prove the change in live Fun Counter Strike.
- Pre-existing work: preserved the unrelated Blender AI submodule change. The repository had prior user edits in AGENTS, dev-loop, gameplay specs, loadout logging, and regression documentation; those were not rewritten for this task beyond the focused command-contract and RMB regression updates.

## Changes

- Added `agent.hold <primary|secondary> <duration_ticks>` for 1–3600 fixed ticks, with request-correlated accepted, applied, and released events. Kept `agent.holdprimary` as a primary-input alias.
- Routed scheduled secondary input through the existing gameplay input frame and a shared gameplay mouse-button resolver used by camera aim FOV and weapon input. Scenario playback keeps priority. GUI input and desktop mouse state are not synthesized or modified.
- Corrected `RagdollModeSystem::applyRightArmPointMotor` to compose the right-arm `meshLocal` bind transform with `player.weaponLocalToArm`, since the motor rotates the physics-body frame while the weapon transform is relative to the rendered arm-node frame.
- Added the composed transform and physics-arm-local barrel axis to the bounded `AIMBODY_LIVE_SAMPLE` diagnostic.
- Extended the existing `weapon.attack.local_request` event with the actual outgoing fire start, direction, end, camera forward, and direction dot product.
- Updated the dev-loop command contract and appended Attempt 2 to the RMB regression tracker. The arm-position spring and per-weapon offsets were not changed.

## Findings

The first runtime divergence was in the shared transform composition. The weapon-local-to-arm matrix described the weapon relative to the rendered arm node, but `applyRightArmPointMotor` treated it as relative to the canonical physics arm. The omitted right-arm bind rotation made the solver's measured barrel axis disagree with the model-derived barrel and collision axis. The viewmodel offset theory was not borne out: no Revolver or Deathball offset values were changed.

`agent.hold secondary 120` produced the expected held-RMB samples and correlated automatic release events. The live mode's spawn path occasionally re-equipped the default Revolver during respawns; those attempts were excluded unless the held sample confirmed the requested weapon. A later re-equip and held sample completed each affected case.

## Build and runtime evidence

- Dev-loop build: `BUILD SUCCESS`, source generation 4, one changed C++ translation unit compiled.
- Build ID: `2026-11-05T19:22:50.002621Z:1c821a2565f74d67f68ea032ae2aa3ef4b401985fff94654c89c1e1db97ae3bb`.
- Launch: mode 45, Fun Counter Strike, run `20261105_192259`; client and server used the selected build. Server `actor.loadout.spawned` events recorded weapon set 30.
- Client journal: `logs/2026-11-05/20261105_192259/client-01/events.jsonl`.
- Server journal: `logs/2026-11-05/20261105_192259/server-01/events.jsonl`.
- All 12 combinations of Revolver, Deathball, Shotgun, and Rocket Launcher at level/up/down produced four held-RMB aim samples each. The minimum settled barrel-to-camera dot was `0.999563`; the minimum collision-capsule-to-camera dot was `0.999563` (required: at least `0.98`).
- A normal Revolver attack request was accepted by the server. The client's outgoing fire-direction dot against camera forward was `0.999993`.
- Human visual acceptance remains pending; the live window was available, but a human has not confirmed visible arm/barrel alignment.

## Validation and review

- Followed `docs/workflows/runtime-scenario-validation.md`: build and live run used the dev loop and the run's own client/server journals.
- Reviewed `docs/skills/spec-behavior-review-v1.md` and `docs/skills/terminal-command-checker-v1.md`; the command uses the shared gameplay action path and validates action and duration.
- No agent gate or synthetic test suite was run. The dev-loop build and live journal were the requested proving path.
- Computer-use review selected the live `mimita.exe` window but did not produce a visual screenshot; visual and human acceptance remain pending.
