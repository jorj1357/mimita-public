# Idle arm pose: centered alternating front/back motion

- Changed `src/hot-reload/hot-animation-clips.h` so idle arms use X only for
  their fixed center pose and oscillate only their Z front/back rotation around
  zero. Added inline tuning comments for axes, speed, range, and alternation.
- Removed the previous fixed opposite Z biases that kept one arm permanently
  on each side of the torso. Weapon-specific poses remain selected only when
  the equipped-tool state provides a nonzero weapon key.
- Validation: hot DLL generation `1000004` built successfully; `mimita.exe`
  was not written.
- Follow-up: walking arms now use a live 30-degree alternating swing. The old
  animated values were immediately overwritten by fixed `5.0f` and `-5.0f`
  rotations. Inline comments document cycle speed, amplitude, phase, and axis.
- Follow-up validation: hot DLL generation `1000005` built successfully.
- Follow-up: `kDashFrames` now places its dash rotation values in the Z slot
  of `HA_PART(tx,ty,tz,rx,ry,rz)` instead of X, with a comment documenting
  the argument order.
- Follow-up validation: hot DLL generation `1000006` built successfully.
- Follow-up: documented the six `kDashFrames` entries by body part and marked
  that each Z rotation is the final number in its `HA_PART`.
- Follow-up validation: hot DLL generation `1000007` built successfully.
- Follow-up: converted `kDownDashFrames` to Z-only rotations; all `rx` and
  `ry` values are now zero and the prior X rotation values occupy `rz`.
- Follow-up validation: hot DLL generation `1000008` built successfully.
- Runtime acceptance still requires observing the active player after the new
  generation is loaded.
