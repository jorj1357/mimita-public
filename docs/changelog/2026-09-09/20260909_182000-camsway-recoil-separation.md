# Camera sway/recoil separation

Time: 2026-09-09T18:20:00Z
Status: implementation attempted; human acceptance pending

Implemented the requested narrow separation in the current working tree. Camera sway now has independent pitch/roll spring offset and velocity, uses finite bounded integration, and is applied only to the render view. Generic punch decay no longer reads camera-sway configuration. Landing sway uses fixed-tick pre-collision downward speed and emits once per landing tick. Configuration uses `landingPitchImpulse`, `landingRollImpulse`, `springStiffness`, `springDamping`, `maxPitch`, and `maxRoll`.

Relevant files: `src/camera.h`, `src/camera.cpp`, `src/config/camera-config.h`, `src/config/camera-config.cpp`, `config/camconfig.json`, `src/engine/engine-tick-camera.cpp`, `src/physics/physics-mini.cpp`, `src/physics/movement/movement-step.cpp`, `src/physics/movement/movement-step.h`, `src/physics/movement/movement-types.h`, `src/physics/movement/movement-conversion.cpp`, and `src/entities/player.h`.

Validation: `git diff --check` passed for the implementation files; the canonical build was started through `python build_agent.py` and final status must be read from `build/changelog.txt`. Runtime and human visual acceptance were not performed.

Regression: `docs/regressions/2026-09-09/camera-sway-recoil-coupling-REG.md` remains `ATTEMPTED FIX (1)` until human confirmation.
