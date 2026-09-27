# Hot body authority and C++/JSON movement source

Date: 2026-09-20
Status: implemented slice; full body-root/network/animation migration remains open

## Request

Restore the v2.0.6 movement feel while keeping movement and collision behavior
hot-reloadable, allow explicit C++ or JSON movement presets, and stop the helper
capsule from acting as an invisible body wall.

## Changes

- Added `behaviorSource: "cpp" | "json"` to `config/movement.json`.
- Extended the hot movement tuning owner to read either the compiled C++ preset
  table or the selected `config/movement/<preset>.json` into the same
  `GameMovementTuningV1` envelope.
- Marked the movement capsule as a helper collider and body socket colliders as
  authoritative colliders.
- Changed the hot collision package so the capsule may establish support but
  does not push against walls/ceilings when body colliders are available.
- Included leg proxies in the ground-support reference so visible feet can
  establish support instead of relying only on the root capsule.

## Evidence

- Hot DLL build: `python build_game_dll.py --output build/mimita-game-body-authority.dll`
- Result: success; output `build/mimita-game-body-authority.dll`.
- `mimita.exe --live-code-selftest`: skipped because `mimita.exe` is not
  present in this checkout.
- No executable was linked, replaced, restarted, or unlocked.

## Remaining required work

- Validate feet/body transforms against the live model and tune body shape data.
- Add deterministic v2.0.6-versus-current movement/collision scenarios.
- Carry body pose/contact state through client prediction and reconciliation.
- Move animation clip selection and tool pose references into a shared
  `animations.json` registry without breaking `weapons.json` compatibility.
- Trace and restore the exact v2.0.6 client ragdoll presentation path, then
  connect it to the current hot ragdoll policy.
- Perform live same-session hot-switch and human visual acceptance.

## Routing and review

- Routed through `docs/ROUTER.md`.
- Reviewed against movement, collision, weapons, live-code, logging, and
  efficiency guidance.
