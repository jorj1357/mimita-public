# state domain coverage and PostFX modes

# Task

- Task ID: agent-b-20261114-state-postfx
- Owner: agent-b
- Focus: resident state domains, PostFX mode gate, ICE bridge audit
- Status: complete for source, build, and bounded runtime evidence
- Date, time, timezone: 2026-11-14T19:35:00-05:00 (2026-11-15T00:35Z)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `2f2cd0ff277272717187e67923ead9abe2d113fa`

# Pre-existing changes

`git status` already contained changes from concurrent work. This session did
not author or claim the packet-schema split, client retirement, `net_common`
split, dev-loop changes, owner-map changes, or the existing changelog edits.
Those remain owned by the agents that made them.

# Requested behavior

1. Continue moving hot-reload-owned behavior into the generation.
2. Add the remaining player/NPC state domains and stop depending on the wide
   `EntitySnapshotV1` for them.
3. Move ICE behavior behind the host capability boundary.
4. Add an explicit PostFX `full` / `minimal` / `off` gate with JSON authority.

# Exact implementation changes

## Player and NPC state domains

Before:

- `EntityDomainCapabilityV1` exposed only `player_core` and `player_session`.
- The adapter and game bridge had typed accessors only for those two domains.
- The parity consumer checked only those two domains.

After:

- `kEntityDomainCapabilityVersion` is `2`.
- `DomainId` covers `player_core`, `player_session`, `player_chat`,
  `player_vip`, `player_combat`, `player_movement`, `npc_core`, and
  `npc_combat`.
- `EntityDomainCapabilityV1` adds generic `read_domain` and `write_domain`
  functions while keeping the existing typed core/session accessors.
- `HostServerEntityAdapter::readDomain` and `writeDomain` translate every
  domain record to/from `ServerPlayer` and `ServerNpc` fields, honoring
  `fieldMask` on writes.
- `src/game/entity-domain-bridge-module.*` exposes generic and templated
  `readDomain` / `writeDomain`.
- `validatePlayerDomains` in `src/game/server-runtime-module.cpp` now reads all
  eight domains and includes NPC records in the parity check.

Files:

- `src/host/abi/entity-domain-capability.h`
- `src/host/host-server-entity-adapter.h`
- `src/host/host-server-entity-adapter.cpp`
- `src/host/host-runtime.cpp`
- `src/game/entity-domain-bridge-module.h`
- `src/game/entity-domain-bridge-module.cpp`
- `src/game/server-runtime-module.cpp`

## PostFX mode gate

Before:

- PostFX always allocated its scene FBO and ran the post pass.
- There was no JSON `mode`; `MIMITA_DISABLE_POSTFX` was the only bypass.
- Disabling the pass produced a raw/dark image because brightness/gamma were
  lost.

After:

- `PostFX::Mode` is `Full`, `Minimal`, or `Off`; default is `Full`.
- `config/postfx.json` `"mode"` is authoritative; `"enabled": false` maps to
  `Off`.
- `Full`: scene FBO plus the configured full effect pass.
- `Minimal`: scene FBO plus a display-only pass using
  brightness/contrast/saturation/gamma/hue/color-temperature. Optional screen
  effects are forced off.
- `Off`: no PostFX FBO bind and no post pass; the frame renders directly to the
  default framebuffer and is intentionally raw/dark.
- `postfx_mode <full|minimal|off>` is a temporary runtime override and emits
  `postfx.mode`; the next JSON reload wins.
- `MIMITA_DISABLE_POSTFX=1` is now a test-only alias for `Off`.

Files:

- `src/render/post-fx.h`
- `src/render/post-fx.cpp`
- `src/render/postfx-commands.cpp`
- `config/postfx.json`
- `docs/specs/graphics/graphics-post-processing.md`

## ICE wrappers

The current `IceAgent` wrapper is already the generation-side implementation:
`src/game/ice-session-bridge-module.cpp` implements every `IceAgent` method
through `IceCapabilityV1`. No duplicate direct-libjuice call path is needed for
the game module.

Resident ICE pieces remain intentionally resident:

- `src/network/ice/ice-agent.cpp` is the host implementation behind the
  capability.
- `src/network/ice/ice-server.cpp` is a standalone ICE harness.
- `src/network/server-ice.cpp` retains raw `sendto` and the coordinator thread.

# Specification alignment

- `docs/specs/hotreload/hot-cold-split.md`: resident shell owns sockets,
  threads, and ICE mechanism; the generation owns behavior. The state-domain
  and ICE results match that boundary.
- `docs/specs/hotreload/hotreload.md`: generation state is migrated, not
  implicitly reset.
- `docs/specs/networking/networking.md`: one packet/session authority; state
  domains are per-owner records rather than a wide snapshot dependency.
- `docs/specs/graphics/graphics-post-processing.md`: new authoritative PostFX
  mode, settings, cost, hot-reload, and acceptance rules.

# Build evidence

Build success only proves compilation and linking.

- EXE artifact `2026-11-15T00:36:17.831201Z` built successfully.
- Whole-game generation **167** built successfully:
  `build\game\generation-167\game-g167.dll`.

# Runtime evidence

Run `20261115_003100`, launch mode 57, map `funworld3`, 19 configured NPCs.
The run loaded generation 166; generation 167 only adds a bounded
`account_role` copy, so the feature evidence below still applies.

- Server journal:
  `logs/2026-11-15/20261115_003100/server-01/events.jsonl`
- `game.state.domain-parity` at ticks 3900 and 4200:
  `mismatches: 0`, checked records reported as `players: 29`.
- Client journal:
  `logs/2026-11-15/20261115_003100/client-01/events.jsonl`
- `postfx.mode` events:
  - `minimal` at sequence 21254
  - `off` at sequence 21772
  - `full` at sequence 22374

This proves the commands reached the generation and changed the mode without a
crash. It does not by itself prove the visual quality of each mode; that remains
a human review item.

# Human review

- Confirm `full` preserves the approved look.
- Confirm `minimal` is usable and visibly applies the display transform.
- Confirm `off` is expectedly raw/dark and has no PostFX pass.

# Remaining work

- Runtime visual acceptance for PostFX modes.
- Further handler migration away from any remaining `EntitySnapshotV1` usage.
- Decide whether `server-ice.cpp` or the ICE harness needs a separate hot
  boundary; neither is required for the current game-module `IceAgent` path.
