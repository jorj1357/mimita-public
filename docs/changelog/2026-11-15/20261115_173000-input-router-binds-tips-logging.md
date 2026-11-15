# Input router, canonical binds, hot-reload tip verbosity, logging catalog

## Task

- Task ID: agent-b-20261115-input-router
- Owner: agent B
- Focus: one hot input owner, focus-free agent testing, binds JSON, hot-status tip controls
- Status: input-router/binds/agent-input verified live in generation 200/202/205/206/208; physical mouse-look, avatar textures, map/cart, NPC collision/animation remain open
- Date/time: 2026-11-15T17:30:00-05:00

## Exact changes

### Input ownership

- New `src/input/input-router.h/.cpp` (game-owned):
  - single normalized owner for host keys, Win32 pointer, agent, scenario, and
    future gamepad/touch/VR sources;
  - frame/tick metadata;
  - immediate look delta application;
  - no allocations, locks, filesystem, or extra thread in the sample path.
- `src/input/input-commands.cpp/.h` feeds host keys into the router and reads
  held keys from it.
- `src/input/input-poll.cpp` and `src/main-systems.cpp` load
  `config/input/binds.json`.
- `src/engine/engine-tick-net.cpp` begins the router frame, applies normalized
  mouse buttons/keys, and exposes frame/client/server tick state.
- `src/engine/engine-tick-camera.cpp` applies pending normalized look.
- `src/engine/engine-tick-combat.cpp`, `src/input/scenario-input.cpp`,
  `src/gui/*`, `src/notifications/notifications.cpp` route through the router.
- `agent.input` terminal command added:
  `look`, `key`, `action`, `mouse_button`, `terminal_toggle`, `menu_toggle`,
  `equip`.

### Binds

- New `config/input/binds.json` with `version` and `input_version`.
- Terminal `input.bind`/`input.save`/`input.load` use the canonical file.
- Account-specific binds remain reserved under `accounts`.

### Hot-status tips

- `config/notifications.json` gains `hot_reload`:
  `enabled`, `verbosity` (`all|minimal|none`), `show_files`, `max_files`.
- `hot-status-tips` registers:
  `hotreload tips <all|minimal|none>`, `hotreload files on|off`,
  `hotreload save`, `hotreload status`.
- JSON is authoritative; terminal commands are runtime convenience and `save`
  writes JSON.

### Rendering/logging

- Body avatar texture binds emit `avatar.texture.bind` or
  `avatar.texture.missing`, falling back to `assets/textures/default.png`.
- Debug logging spec now has a live input/rendering logging catalog.

### Documentation

- `docs/specs/input/input-router.md`
- `docs/gold/2026-11-15-live-input-and-cosmetic-hot-fixes.md`
- `docs/regressions/gameplay/input-hot-client-REG-2026-11-15.md`
- `docs/regressions/gameplay/avatar-textures-missing-REG-2026-11-15.md`
- `docs/regressions/gameplay/map-object-activation-REG-2026-11-15.md`
- `docs/regressions/gameplay/npc-collision-stuck-REG-2026-11-15.md`
- `docs/regressions/gameplay/npc-authored-animation-REG-2026-11-15.md`
- `docs/specs/debug-logging/debug-logging.md` catalog
- `docs/specs/open-gameplay-issues.md` NPC/animation/effects/cart gaps

## Runtime evidence

Run `20261115_172313`, PID `24736`:

- generation 200 launched; 201 through 208 activated in the same PID 24736.
- `agent.input look 120 0` delivered; `input.normalized` recorded the agent source.
- `agent.input equip 2` completed; `equipslot2` resolved to `shotgun`.
- `agent.input terminal_toggle` delivered; `input.state.terminal_open=true`.
- `hotreload status` command delivered.
- Avatar body logs show `source_texture_id=0` and
  `avatar_atlas_texture_id=0` for NPC parts, so the fallback is active but the
  real avatar atlas is not bound.
- Host key/W movement and terminal evidence were previously verified in
  generations 194/199.

## Open

- Physical mouse-look and all UI buttons need human acceptance.
- Avatar body textures still appear missing despite fallback/logging.
- Map objects still shift on activation; cart GLB missing.
- NPC collision stuck; authored animation/effects not visible.
- `CAP_INPUT` remains the proper cold replacement for the Win32 fallback.
