# Arrow camera controls and 45-degree alignment

- Added Arrow Up/Down as aliases for camera-relative forward/backward movement.
- Added smooth Left/Right Arrow camera yaw at 120 degrees per second.
- Added Shift+Comma (`<`) and Shift+Period (`>`) one-shot world-aligned 45-degree yaw snaps.
- Updated camera, movement, and logging specifications. The owner emits `camera.keyboard_control` for turn starts and snap applications.
- Source was edited, but runtime behavior was not built or exercised in this session. No live `events.jsonl` proving the new behavior exists yet; the repository's shared-checkout authority prohibits agent builds and requires the root dev-loop to observe a saved compiler-input change.
