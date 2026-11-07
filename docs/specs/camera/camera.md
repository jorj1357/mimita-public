# Camera

## Purpose

Define camera behavior and the presentation settings owned by
`config/camconfig.json`.

## Does not own

This specification does not own gameplay collision, physics, or fixed-tick
simulation. Camera presentation changes run during rendering and do not alter
world or actor state. It is written for engine and rendering contributors.

## Camera proximity fade

Nearby 3D surface geometry fades according to its distance from the active
camera. Distance is evaluated per rendered surface point, so a large surface
fades when its nearest visible part enters the fade range rather than when its
origin does. The rule applies in every camera mode and in every direction.

`config/camconfig.json` owns the local presentation settings:

```json
"proximityFade": {
  "enabled": true,
  "startDistanceMeters": 1.0,
  "endDistanceMeters": 2.0
}
```

At or inside `startDistanceMeters`, the fade multiplier is zero. Between the
two distances it increases linearly from zero to one. At or beyond
`endDistanceMeters`, the multiplier is one. The multiplier is applied to the
surface's existing alpha, preserving authored texture transparency. Faded
surfaces use alpha blending; fully faded fragments are discarded so they do
not occlude geometry behind them.

The defaults are enabled, 1 meter, and 2 meters. The start distance must be
finite and nonnegative; the end distance must be finite and greater than the
start. An invalid proximity-fade block rejects the candidate camera config and
keeps the last valid camera settings. Valid updates use the existing camera
config reload path.

The fade is a rendering rule only. It must not change collision, damage,
physics, network state, or fixed-tick behavior. It affects world and actor
surface geometry that uses the shared scene shader. Non-surface overlays such
as screen UI and sky rendering are outside this rule.
