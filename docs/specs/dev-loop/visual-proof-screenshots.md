// 2026-11-20
/* purpose
* let an agent request a screenshot at an exact client tick as visual proof
* pair the image with build, run, generation, camera, actor, and event identity
* prevent a screenshot from being presented as server or gameplay proof alone
* this file DOES NOT change fixed-tick simulation or rendering authority
* this file DOES NOT require screenshot capture in every test
*/

# Visual proof screenshots at a tick

## Command surface

Supported forms:

```text
screenshot_at_tick <tick> [name]
screenshot_at_tick now [name]
```

The command may be delivered through the existing agent command queue or a
scenario step. It captures the client framebuffer after the render/UI pass at
the requested client simulation tick.

The output is written under:

```text
logs/<date>/<run-id>/client-01/screenshots/<tick>-<name>.png
logs/<date>/<run-id>/client-01/screenshots/<tick>-<name>.json
```

The sidecar JSON contains:

- run ID, instance ID, PID, build ID, game generation/code hash;
- requested tick, captured client tick, nearest server tick;
- executable and DLL identity;
- camera position/yaw/pitch/FOV;
- local actor position, velocity, grounded state, health, and equipped weapon;
- visible actor/effect counts;
- UI-visible flag;
- screenshot SHA-256 and dimensions;
- `result: captured | missed | failed`;
- the exact journal path and the next expected event.

## Timing and correctness

- Wait for the exact tick or the nearest safe render boundary after it.
- Never fabricate a tick; if the tick is missed, write `result=missed` with the
  actual nearest tick and do not label it as an exact capture.
- Capture after the UI pass when the intent is to prove what the human sees.
- Headless/offscreen capture must use the same simulation and render path and
  record that it is headless.
- GPU readback may stall; record capture duration and do not use a screenshot
  run as the 240 FPS frame-consistency benchmark.
- Paths must be inside the run directory; no arbitrary filesystem path.

## Evidence rules

A screenshot proves only what is visibly present in the image at the recorded
tick and identity. It does not prove server authority, accepted damage, network
delivery, or a gameplay rule. Pair it with the relevant `events.jsonl` event and
sidecar. The proof record links image, sidecar, journal, build, generation, and
run.

## Implementation outline

- Add the terminal/agent command and scenario step.
- Reuse the existing render framebuffer readback path; do not create a second
  renderer.
- Add bounded capture queue and a single in-flight screenshot.
- Write UTF-8 JSON sidecar and PNG atomically; preserve the last image on
  failure.
- Add `screenshot_at_tick` to the proof ledger as visual evidence.
- Test exact tick, missed tick, invalid path, headless, and UI-visible modes.

## Acceptance

- An agent can request a screenshot at tick N and gets an identity-matched
  image or a truthful `missed` result.
- The sidecar and journal prove the tick/build/run identity.
- A screenshot is never accepted as the sole proof of server or gameplay
  behavior.
