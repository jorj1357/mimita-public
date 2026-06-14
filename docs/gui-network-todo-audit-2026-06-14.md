# GUI, Hot Reload, TODO, and Multiplayer Audit

Date: 2026-06-14

## Executive Status

- GUI JSON hot reload exists and is used by every menu, but most JSON files
  currently own only button geometry.
- The GUI is immediate-mode. Text, colors, visibility rules, media, spacing,
  font sizes, and dynamic layout are still mostly hardcoded in menu code.
- The GUI editor previously supported button selection and movement only.
- Multiplayer is an authoritative UDP client/server prototype, not P2P.
- Current protocol smoke now proves joins, unique names, movement, NPC spawn,
  shot damage, death, and respawn.
- Public multiplayer is not ready: packet reliability, reconnect identity,
  lag compensation, server-side raycasts, matchmaking, abuse controls, and
  bandwidth reduction are missing.

## Improvements Implemented

### GUI layout and reload

- Expanded `GuiElement` JSON load/save support:
  `x`, `y`, `width`, `height`, `textOffsetX`, `textOffsetY`, `fontSize`,
  `padding`, `margin`, `visible`, `hoverScale`, `hoverSound`, `clickSound`,
  `backgroundImage`, `backgroundVideo`, `anchorX`, `anchorY`, and `layer`.
- Preserved legacy absolute positioning by defaulting anchors to `left`/`top`.
- Changed file timestamps from whole seconds to nanoseconds so rapid edits are
  not silently missed.
- Made load transactional. Invalid or partially written JSON no longer clears
  the live layout; the last valid layout remains active until parsing succeeds.
- Updated the saved timestamp after `gui_save`, preventing a redundant
  hot-reload of the file the game just wrote.
- `GuiLayout::setElement()` now preserves the full property record during edits.

### GUI editor

- Bottom-right corner drag now resizes the selected widget.
- `Alt+Arrow` resizes by keyboard.
- `T+Arrow` edits text offsets.
- Existing arrow movement and Shift/Ctrl step sizes remain.
- Overlay now displays text offsets and the new controls.

### Verification and maintenance

- Removed one obsolete GUI TODO that described work already completed.
- Updated `tests/network-protocol-smoke.cpp` from the removed input-attack
  shortcut to the current `PACKET_SHOT_REQUEST` flow.
- Added movement replication coverage to the protocol smoke.

## GUI Hot Reload Audit

### What works now

- Layout files are lazily loaded through `GuiLayoutManager`.
- `guiMain()` polls active layouts while menus are open.
- The gameplay loop also polls loaded layouts.
- Geometry changes update widgets without restarting the game.
- GIF files detect their own file timestamp and replace cached frames.
- `gui_save`, `gui_reload`, and reset commands exist.

### Critical limitations

| Area | Current status | Required change |
|---|---|---|
| Static PNG/JPG reload | Cached forever by `TextureStore::getPath()` | Add renderer-owned timestamped replacement with stable handles |
| MP4 | Extension is discovered, but `uiDrawMedia()` falls through to image loading | Add a decoded-frame video backend or remove the false support claim |
| Visibility | Schema now persists it; menus do not consistently consume it | Route all draws through element-aware helpers |
| Anchors | Persisted only | Resolve anchors in one coordinate/layout layer before rendering and editing |
| Layers | Persisted only | Record draw commands and sort, or enforce layer-owned render passes |
| Hover/click sounds | Global click sound; hover is globally hardcoded | Add per-widget audio resolution with cached sound IDs |
| Font size/colors | Hardcoded at call sites | Add text/button style helpers backed by `GuiElement` |
| Padding/margin | Persisted only | Define layout semantics; immediate-mode calls currently bypass them |
| Media assignments | Persisted only | Element-aware media draw helper and editor property panel needed |
| Live JSON safety | Fixed in this audit | Keep last valid configuration on parse failure |

### Hardcoded GUI inventory

| Owner | Hardcoded behavior still present |
|---|---|
| `main-menu.cpp` | Background path and extension search, title/profile text positions, font sizes, colors, button colors |
| `play-menu.cpp` | Header/separator, descriptions, font sizes, colors, visibility |
| `practice-menu.cpp` | Header/separator, description, font sizes, colors |
| `online-menu.cpp` | All labels/inputs/columns/status geometry, colors, field rules, conditional Start/Stop/Join visibility |
| `duel-config-menu.cpp` | Title and value-label positions, fonts/colors, limits, row layout |
| `sandbox-map-menu.cpp` | Header/status/list rows, seven-row page rule, row spacing, colors, page text |
| `settings-menu.cpp` | Geometry is mostly migrated; fonts, colors, dropdown row gap, and visibility remain hardcoded |
| `server-info-menu.cpp` | Labels/input/status text geometry, fonts/colors, conditional button visibility |
| `sign-in-menu.cpp` | All fields and labels except buttons, font sizes/colors, warning text |
| `help-menu.cpp` | Entire text document layout, line heights, font sizes/colors |
| `debug-menu.cpp` | Title geometry/font/color and noisy per-frame `printf` |
| `game/duel.cpp` | HUD, countdown, result panel, stats, colors, timers, panel geometry, visibility rules |
| `ui-system.cpp` | Button text sizing, centering, border, hover/press colors, global click sound |
| HUD/music overlays | Nameplates, damage numbers, chat bubbles, replay HUD, network HUD, and music overlay remain code-configured |

### GUI editor findings

- Moving buttons: works.
- Resizing buttons: implemented in this audit.
- Editing text offsets: implemented by keyboard in this audit.
- Editing anchors: missing property UI.
- Editing layers: missing property UI and runtime layer ordering.
- Editing images/GIF/MP4: missing property UI; MP4 runtime is also missing.
- Saving layouts: works for every field represented by `GuiElement`.
- Selecting anything visible: not achieved. Tracking is button-centric;
  arbitrary text, rectangles, images, HUD elements, and some controls are not
  registered as editable widgets.

### GUI implementation plan

1. Add `uiElementButton`, `uiElementText`, `uiElementMedia`, and
   `uiElementRect`. These apply visibility, anchors, offsets, font, colors,
   media, sounds, hover scale, and stable IDs in one place.
2. Register every draw call in one editable-widget registry, including text,
   images, sliders, checkboxes, and non-interactive panels.
3. Add an editor property panel with keyboard-safe text entry for media paths,
   anchor dropdowns, visibility, layer, font size, padding, margin, and sounds.
4. Migrate one screen at a time: main, play/practice, online, duel config,
   sandbox, settings, sign-in/server, help, then gameplay HUD.
5. Add a renderer-owned media cache that can atomically replace PNG/JPG/GIF
   textures. Treat video as a separate decoder/player resource.
6. Introduce a draw-command list only when layer sorting is needed. Do not
   convert the whole GUI to a retained widget tree.
7. Add a layout round-trip test: load, mutate every field, save, reload, and
   compare. Add a malformed-JSON hot-reload test.

## C++ Hot Reload Architecture Audit

### Working

- The EXE owns persistent state and loads a unique temporary DLL copy.
- Candidate DLL/API validation occurs before replacing the current module.
- Old code remains active after build/load/API failure.
- Persistent STL, OpenGL, audio, network, GUI, and replay ownership stays in
  the EXE.
- The first reloadable slice is pure effect integration through POD state.

### Partial or risky

- Source rebuild is synchronous through `std::system`, causing frame hitches.
- Only `effect-part.cpp` and `game-api.h` are watched.
- There is no active-call barrier for future worker-thread use.
- No automated repeated reload/rollback stress test exists.
- GUI JSON reload is independent from the DLL system, which is correct, but
  the TODO claiming reloadable UI behavior is now too broad and should be
  split into data reload versus code reload.

### Recommended next slice

Keep state/resource ownership in the EXE. Add background compilation with
debounce and a main-thread swap point, then move only pure duel transition or
weapon calculation functions across the versioned POD API. Do not move GUI,
network sockets, replay storage, or renderer resources into the DLL.

## TODO Sweep

### Priority 1

| Item | Purpose | Risk | Effort |
|---|---|---:|---:|
| `hot-reload-system.h`: background compiler/live editor | Avoid frame stalls and expose failures | Medium | 2-4 days |
| `hot-reload-system.h`: texture/animation replacement | Required for real media hot reload | High, GPU lifetime | 3-7 days |
| `hot-reload-system.h`: shader watching | Faster renderer iteration with rollback | High, GL state | 2-4 days |
| `hot-reload-system.h`: explicit map reload transaction | Prevent partial world replacement | High | 3-5 days |
| `main.cpp`: extract loop and feature command registration | Main owns too many systems and blocks safe iteration | Medium | 3-7 days, staged |
| `debug-commands.h` and `replay-commands.h` | Complete subsystem-owned command registration | Low-Medium | 1-2 days |
| Replay timeline/browser placeholder headers | Main still owns replay UI/workflow logic | Medium | 2-4 days |
| `impact-damage.cpp`: real impact damage | Current path logs TODO instead of gameplay behavior | High, balance/physics | 1-3 days |

### Priority 2

| Item | Purpose | Risk | Effort |
|---|---|---:|---:|
| `main.cpp`: shared ray utilities | Remove duplicate ray/triangle logic | Medium | 0.5-1 day |
| `main.cpp`: move editor-only helpers | Restore GUI ownership | Low | 0.5 day |
| `main.cpp`: move duel startup to game-mode owner | Decouple mode lifecycle from orchestration | Medium | 1-2 days |
| `replay-scene.h`: centralize replay constants | Prevent export/import drift | Medium | 0.5-1 day |
| `camera.h`: move globals to config and define FOV limits | Remove duplicated commented settings | Medium | 0.5-1 day |
| `physics-freeze.cpp`: freeze sound | Complete feedback | Low | 0.5 day |
| `texture_manager.cpp`: remove or replace legacy hardcoded manager | Eliminate duplicate texture ownership | Medium | 0.5-1 day |
| `texture.h`: replace vague placeholder TODO | Document loader contract/error behavior | Low | 0.25 day |
| `skybox/readme-howto.txt`: memory-efficient skybox | Avoid excessive texture memory | Medium | 1-2 days |
| Replay importer `IMPORT_MAP` TODO | Comment conflicts with currently validated importer behavior; revalidate then delete or fix | Medium | 0.5 day |

### Priority 3

| Item | Purpose | Risk | Effort |
|---|---|---:|---:|
| Debug-log migration comments in collision/death/NPC/weapon code | Centralize categories and rate limits | Low | 1-2 days total |
| `physics/config.h`: shared epsilon and file organization | Consistency/readability | Medium if behavior changes | 1 day |
| `world.h`: explain `glm::ivec3` hash | Documentation/cross-platform clarity | Low | 0.25 day |
| `world.h`: stale TEMP/per-face texture and slope comments | Remove dead notes | Low | 0.25 day |
| `world.cpp`/archive comments | Historical cleanup only | Low | 0.25 day |
| Asset migration/organization readmes | Consolidate duplicated asset trees | Medium due path churn | 1-3 days |
| `audio.cpp` ownership note | Possible file split, no current defect | Low | 0.5 day |
| `enemy.*` placeholders | Dormant duplicate entity path | Medium | Delete after reference audit, 0.5 day |

Completed automatically: removed the obsolete `gui-label.cpp` font-splitting
TODO. Other vague comments were not converted into speculative behavior.

## Network Duel Status

### What works

- UDP server/client startup and explicit host/join UI.
- Server-assigned IDs and duplicate-name disambiguation.
- 60 Hz authoritative snapshots with players and NPCs.
- Remote replica creation, rendering registration, and interpolation.
- Client movement reporting with finite/distance/speed validation.
- Server world collision and basic player separation.
- Explicit shot request/event replication.
- Server ownership checks, serial deduplication, geometry bounds, target
  proximity validation, health, death, and respawn.
- Remote muzzle/tracer/sound/world impact/debris/blood/hit effects.
- Snapshot health and overhead remote healthbars.
- Ping display, packet counters, snapshot age/loss, and fake-lag tools.
- Disconnect packets and five-second server timeout cleanup.
- Rebuilt protocol smoke passes movement, spawn, combat death, and respawn.

### Partial

- Shooting is server-validated but not server-raycast. The client supplies the
  target and hit point; proximity checks reduce abuse but do not prevent it.
- Movement is hybrid client-state acceptance plus server simulation, not full
  server-authoritative input replay/prediction.
- Knockback is event-driven and partly client-applied.
- Ragdoll is reconstructed locally from a kill event. Pose/limb simulation is
  not replicated.
- Blood is reconstructed from shot events. Persistent decals/particles are not
  authoritative or synchronized.
- Replay captures reconstructed network effects, but there is no multiplayer
  replay contract proving all peers produce the same replay.
- Host UI starts a separate local dedicated process, but there is no NAT
  traversal, public listing, matchmaking, or managed lifecycle.

### Broken or missing for public multiplayer

- No reconnect token/session identity or state resumption.
- No reliable/ordered channel for critical events.
- No packet sequence acknowledgements, resend, congestion control, or MTU
  fragmentation strategy.
- No lag-compensated hit validation or server rewind.
- No encryption/authentication; UDP source address is the main ownership check.
- No rate limiting, bans, moderation, server browser backend, or account auth.
- No match isolation/duel session manager on the server.
- No authoritative duel score/round/map/replay state in the network protocol.
- No deployment supervision, metrics, crash restart, version rollout, or
  regional routing.

### Bandwidth blocker

`SnapshotPacket` is sent at its full fixed size (`9244` bytes) at 60 Hz to
every client, even when few entities are active. Continuous average concurrency
therefore implies approximately:

- 10 players: 44 Mbps and 14.4 TB/month outbound.
- 100 players: 444 Mbps and 144 TB/month.
- 1,000 players: 4.44 Gbps and 1.44 PB/month.

Before scale work, send only the used entity bytes, lower snapshot frequency
to 20-30 Hz, add delta/baseline compression, quantize transforms, and use
reliable events separately from snapshots.

## Hosting Options

Prices checked on 2026-06-14. Estimates assume optimized snapshots and multiple
small duel server processes per node. They are planning ranges, not quotes.

### Option A: player-hosted/listen server

- Cost: approximately $0 platform hosting.
- Practical scale: ideal for one 1v1 duel; 2-4 players is the realistic target.
- Problems: NAT/firewalls, host advantage, host migration, cheating, unstable
  upload, match disappearance, exposing player IPs.
- Architecture mismatch: current server is already a separate authoritative
  process. Turning it into true peer mesh would add work and reduce security.

### Option B: dedicated authoritative servers

| Average concurrent players | Estimated monthly infrastructure |
|---:|---:|
| 10 | $5-$15 |
| 100 | $25-$100 |
| 1,000 | $300-$1,200 |
| 10,000 | $3,000-$12,000 plus operations/DDoS/control plane |

Provider order:

1. OVHcloud: strongest first public-host choice because low-cost VPS plans
   advertise unlimited traffic and included anti-DDoS.
2. Hetzner: excellent price/performance and US/EU locations; verify included
   traffic for the selected region before committing.
3. DigitalOcean: easiest operations and predictable pricing, but materially
   more expensive per CPU than OVH/Hetzner.
4. AWS Lightsail: acceptable for a control plane or temporary region, but not
   justified for the primary low-budget game fleet.

Reference prices:

- OVH VPS: $4.54, $8.50, $12.32, and $23.37/month tiers.
- DigitalOcean Basic: $6/month for 1 GiB, $24/month for 4 GiB; CPU-optimized
  starts at $42/month.
- AWS Lightsail public IPv4 Linux: $5/month for 0.5 GiB, $12/month for 2 GiB,
  $24/month for 4 GiB.
- Hetzner cloud offers US and EU regions, API/firewalls, and dedicated-vCPU
  plans intended for game-server-like sustained workloads.

Sources:

- https://www.ovhcloud.com/en/vps/
- https://www.hetzner.com/cloud/
- https://www.digitalocean.com/pricing/droplets
- https://aws.amazon.com/lightsail/pricing/

### Option C: hybrid

Feasible and recommended if "hybrid" means:

- player-launched dedicated server process for private/LAN/unranked matches;
- official dedicated servers for public/ranked matches.

Do not build peer mesh. Keep one wire protocol and one authoritative server
binary. The difference should be who launches and supervises the server.

## Final Recommendation

Build now:

1. Keep the current authoritative dedicated-server architecture.
2. Finish one networked 1v1 duel loop: match creation, two-player assignment,
   server-owned score/round/respawn/map state, and clean match teardown.
3. Fix snapshot bandwidth before adding player count.
4. Add reliable critical events, reconnect tokens, and server-side raycasts.
5. Deploy one cheap OVH or Hetzner node in the region nearest current testers.
6. Continue GUI migration screen-by-screen through element-aware helpers.

Delay:

- multi-region orchestration;
- automatic fleet scaling;
- ranked matchmaking;
- full DLL migration of gameplay systems;
- MP4 UI backgrounds;
- generalized retained-mode GUI/layout engine.

Ignore for now:

- pure P2P mesh;
- AWS game fleet services;
- 32-player public matches;
- perfect anti-cheat;
- large GUI abstraction rewrites.

The highest progress-per-hour path is one authoritative 1v1 server, one public
region, one compact protocol, and one reusable GUI element rendering API.
