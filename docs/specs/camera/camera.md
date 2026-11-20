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

## Keyboard camera and movement controls

In normal gameplay, Left Arrow turns the camera left and Right Arrow turns it
right at 120 degrees per second while held. The turn is integrated using the
frame delta so it remains smooth across render rates. Turning changes camera
yaw and the existing camera-to-player facing path; it does not directly move
the actor.

Up Arrow supplies the same camera-relative horizontal forward movement as W;
Down Arrow supplies the same horizontal backward movement as S. The existing
movement owner normalizes combined directions and preserves its fixed 60 Hz
gameplay simulation rules.

Shift+Comma (`<`) and Shift+Period (`>`) each apply once per key press. They
snap camera yaw immediately to the adjacent 45-degree world-yaw alignment:
`<` steps left and `>` steps right. These are world-aligned yaw steps, not
camera-relative easing. Keyboard camera controls are inactive while the
keyboard is disabled, the game window is unfocused, the map editor is active,
or replay playback is active.

The camera category emits `camera.keyboard_control` when a smooth turn begins
and when a 45-degree alignment is applied. The record includes the control,
before/after yaw, turn rate, and whether the result is on the world 45-degree
grid. This event proves that input reached the camera owner and changed yaw;
it does not by itself prove that movement or visual presentation was accepted
by a human.
