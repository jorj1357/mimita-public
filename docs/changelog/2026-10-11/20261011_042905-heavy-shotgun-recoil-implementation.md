# Heavy Shotgun recoil implementation

Date: 2026-10-11

## Scope

Implemented the behavior described by `docs/specs/recoil/recoil.md` for the
Heavy Shotgun. This session did not add or pursue 240 FPS behavior or any
Juggernaut-map work.

## Source changes

- Added a typed `recoil_definition` to weapon definitions and JSON loading.
- Added uncapped-by-default accumulation for typed recoil. Recovery begins
  only after the weapon's configured recovery delay.
- Added separate camera, first-person viewmodel, and physical right-arm recoil
  channels. ADS scaling is explicit and configurable.
- Replaced the old recoil-value-is-nonzero animation trigger with a discrete
  shot notification, so automatic fire adds another kick instead of holding
  one repeated pose.
- Added bounded `weapon.recoil.shot` structured events with accumulated,
  camera, and arm values.
- Added recoil reset on weapon switch and player reset.
- Kept gameplay self-impulse and knockback on the existing fixed-tick path.
- Heavy Shotgun now uses a 1.0 second fire delay, 2.5 second reload, 8-shell
  magazine, and 888 reserve ammunition. Its legacy duplicate recoil custom
  parameters were removed; `recoil_definition` is the owner.
- Existing Heavy Shotgun model, sound, tracer, and `weaponcollisions.json`
  definitions remain active.
- `equipslot19` resolves through community weapon set 5 to the Heavy Shotgun
  native definition slot 24 when that active set is present.

## Build evidence

- First parallel build failed from compiler out-of-memory errors.
- Canonical serial build passed with `MIMITA_BUILD_JOBS=1`.
- Result: `BUILD SUCCESS`, 10 compiled, 539 skipped, return code 0.
- Build record: `C:\mimita-v9\build\changelog.txt`.
- `git diff --check` reported no whitespace errors; only existing line-ending
  warnings were reported for unrelated working-tree files.

## Runtime evidence

- Fresh executable identity was captured with `--versioninfo`:
  `C:\mimita-v9\logs\10-11-2026\20261011_042743\events.jsonl`.
- Fresh server launch created:
  `C:\mimita-v9\logs\10-11-2026\20261011_042806\events.jsonl`.
- The server registered `heavy_shotgun (slot 24)` and reported a fixed-tick
  performance window at approximately 59.8 Hz.
- A separate fresh client/server command exercise connected, but the startup
  equip command did not dispatch because the direct client did not reach the
  required ready weapon-set state before the bounded test ended. The run did
  not produce a valid `weapon.recoil.shot` event, so live recoil behavior is
  not claimed as verified here.

## Acceptance status

Source and build gates pass. Runtime registration and fixed-tick evidence pass.
Human visual acceptance of the camera kick, viewmodel movement, physical arm
response, muzzle flash, sound, and repeated automatic accumulation remains
pending an interactive playtest with the active sandbox weapon set.
