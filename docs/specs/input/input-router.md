# Input Router

## Purpose

`InputRouter` is the single hot owner for local input normalization. Every
keyboard, mouse, agent, scenario, and future gamepad/touch/VR source feeds the
same snapshot. Gameplay, UI, camera, terminal, and bindings read that snapshot;
they do not call the platform backend directly.

The OS/window event pump remains resident. The router is hot.

## Source adapters

- `Host`: resident-dispatched key/char/scroll/focus events.
- `Win32`: hot fallback for the resident window's mouse/pointer state.
- `Agent`: `agent.input` commands used by live development.
- `Scenario`: recorded/replayed input.
- Future: `Gamepad`, `Touch`, `VRHead`, `VRHand`.

Adapters only produce normalized tokens. They do not own actions, gameplay, or
camera policy.

## Versioning

- `input_version` covers the normalized event/action contract.
- `binds_version` covers `config/input/binds.json`.
- Both are explicit and may evolve independently.
- New source types must not change gameplay action semantics.

## Frame and tick contract

Render frame:

1. `beginFrame(frameId, clientTick, serverTick)` samples each source once.
2. Mouse look is applied immediately to the camera.
3. UI pointer/hover state is updated immediately.

Fixed 60 Hz tick:

1. Action edges are consumed exactly once.
2. Held movement/combat state is snapshotted.
3. `MpInput` is built and sent to the server.

The server never receives render-frame input. Look stays immediate on the
client; the latest yaw/pitch travel with the next fixed-tick packet.

## Bindings

`config/input/binds.json` is authoritative:

```json
{
  "version": 1,
  "input_version": 1,
  "default": {},
  "binds": { "walkforward": "W" },
  "accounts": {}
}
```

Tokens remain backend-neutral: `key:W`, `mouse:left`, `pad:...`, `touch:...`,
`vr:...`. Account-specific overrides are future work; the structure is reserved
now. JSON reload wins. Terminal `input.bind` changes runtime state;
`input.save` writes the JSON explicitly.

## Latency

The frame path performs no allocations, locks, filesystem work, or extra thread
hops. Input sampling is measured separately and should remain near 0 ms. A
frame budget target is not satisfied by hiding work in a queue.

## Logging

All router events use existing `StructuredLogger` categories. AI/contributors
enable them in `config/debuglogger.json`; do not create a parallel log file.
See the Logging Catalog in `docs/specs/debug-logging/debug-logging.md`.


## Pointer lock and L behavior

- L toggles gameplay pointer lock, matching the established behavior.
- Lock uses the router/Win32 window owner, not the game DLL's static GLFW.
- Locked: cursor is clipped to the game window, hidden, and recentered for
  unlimited look.
- Unlocked: clipping and hiding are removed, and `firstMouse` is reset once.
- The router is the only look owner. Do not apply host callback deltas and Win32
  deltas in the same frame.
- Host callback deltas are ignored while pointer lock is off.
