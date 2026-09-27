# Force Punch debug sphere live-code slice

## Scope

Focused only on the sphere-shaped Quick Hit weapon path used by `force_punch`.
The ordinary `quick_hit` entry remains a capsule and is not changed into a
sphere by this session.

## Source and configuration

- `config/weapons.json` already hot-reloads through `WeaponData::reloadBuiltinWeaponsIfChanged()`.
- `force_punch.custom_params.hitboxRadius` controls the collision/debug sphere radius.
- `force_punch.custom_params.debugHitboxEnabled` controls whether the new visual is emitted.
- `force_punch.custom_params.debugHitboxAlpha` controls the requested alpha.
- `src/combat/weapon-quick-hit.cpp` supplies the live position/radius and performs the EXE-side draw call.
- `src/hot-reload/quick-hit-debug-visual.cpp` supplies the wireframe/color presentation policy through the hot DLL.

## Live C++ path

The versioned `GameAPI` now exposes `updateQuickHitDebugVisual`. The DLL build
includes the focused source file, the watcher observes it, and the existing
Ctrl+S-style source timestamp path rebuilds and swaps the DLL without
restarting the EXE/world/session. The current hot policy is a green-cyan
wireframe sphere.

## Validation

- `weapons.json` parse: passed.
- `git diff --check`: passed; only line-ending warnings were reported.
- Hot DLL build: not proven because this machine has no C++ compiler available
  on PATH (`build_game_dll.py` stopped with the toolchain error).
- Full EXE build: not run.
- Live visual acceptance: not run.

## Remaining acceptance

Install/configure the repository's C++ compiler, build the DLL and EXE, start a
game with the hot-reload gate enabled, equip `force_punch`, and confirm:

1. the green-cyan wireframe sphere appears only while the attack is active;
2. changing `hitboxRadius` or `debugHitboxEnabled` in JSON updates live;
3. changing `quick-hit-debug-visual.cpp` and saving causes a DLL generation
   change and updates the color/wireframe policy without restarting.
