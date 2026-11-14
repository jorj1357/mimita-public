# Hot-reload agent claims (2026-11-13)

> **LEGACY MANUAL CLAIM TABLE — DO NOT ADD NEW ROWS HERE.**
> New work must be declared in `docs/handoffs/hot-reload-work-plans.json`.
> Exact paths, generated ownership, and the human claim view will come from the
> source manifest. This file is retained temporarily for history and current
> active A/B/C claims, then deleted when the generated registry covers them.

**Protocol:** add one row before editing; claim the smallest stable prefix.
Release only after build and verification. Never edit an unclaimed shared path.
No branches, worktrees, lanes, or second checkouts.

| Agent | Path or prefix | Task | Started | Finished |
|---|---|---|---|---|
| A | `src/host/**`, `src/game/*-module.*`, `docs/specs/hotreload/**` | integrator/resident boundary | 2026-11-13 | — |
| B | `src/network/server-*.cpp`, `src/network/server.h`, `src/network/multiplayer-*.cpp`, `src/network/client.cpp`, `src/network/udp-*`; owner-authorized `src/host/abi/transport-capability.h`, `src/host/transport-adapter.h`, `src/host/host-*-transport-adapter.*`, `src/host/host-runtime.*`; `src/game/transport-bridge-module.*`, `src/game/network-compat-bridge-module.cpp` | server packet send → CAP_TRANSPORT (incl. endpoint send extension) | 2026-11-13 | — |
| C | `src/engine/**`, `src/render/**`, `src/entities/player-render*.cpp` | whole-game tick/render + CAP_RENDER | 2026-11-13 | — |

## Agent C boundary note (2026-11-13)

`renderContext` is the resident `Renderer*` (CAP_RENDER_DEVICE `acquire` returns
it; `Engine::init` uses `acquireHostRenderer` under `MIMITA_GAME_DLL`). C is
providing the whole-game entry in `src/engine/whole-game-entry.*`; Agent A owns
the `src/game/game-module.cpp` wiring and the host `driveTick`/`driveFrame`
dispatch. C does not touch `src/host/**`, `src/game/*-module.*`, or `main.cpp`.

Render-contract handoff (2026-11-13): `src/render/render-command.h` is C's
authoritative missing-operation list for `CAP_RENDER`. `render-command-legacy.cpp`
is the `!MIMITA_GAME_DLL` raw-GL implementation; the DLL bridge is added only
after A implements the declared operations. No call sites have moved yet.

Owner-requested spec append (2026-11-13): C appended the render hot-coverage
worklist to `docs/specs/hotreload/hotreload.md` at the owner's explicit request.
A's spec sections were not modified.

Owner-directed lane extension (2026-11-13): at the owner's explicit request, C
implemented the `CAP_RENDER` version-2 surface in
`src/host/abi/render-types.h`, `src/host/abi/render-capability.h`, and
`src/host/host-runtime.cpp`, plus the DLL bridge in
`src/game/render-bridge-module.cpp`. A remains the owner of those paths for
review; this note records the temporary extension.

Owner-directed world sweep (2026-11-13): C also migrated the shader/uniform
family in `src/physics/physical-entity.cpp`,
`src/impact/destructible-render.cpp`, and `src/world/streamed-world-layer.cpp`
because they call the shared world shader. Their bind/draw/state calls remain
raw for the next family sweep.

Owner-directed blocker fix (2026-11-13): C changed
`getServerMatchMode()` to `MimitaNet::getServerMatchMode()` in
`src/game/server-command-dispatch-module.cpp` to unblock the whole-game DLL
rebuild. A owns that file; this one-line fix is recorded for review.

Owner-directed combat sweep (2026-11-13): C migrated the shader/uniform family
in `src/combat/weapon-viewmodel.cpp`, `src/combat/projectile-render.cpp`,
`src/combat/revolver-system.cpp`, and `src/render/procedural-cylinder.cpp` to the
game-owned world shader.

## Agent B status (2026-11-13)

Source pass complete. Owner authorized B to extend `CAP_TRANSPORT` with a
plain-integer endpoint send (`send_to_endpoint`) so pre-auth replies no longer
use `sendto` in game-owned sources. All 27 raw `sendto` sites are now accounted
for: 23 game-owned sites route through `serverSendToPlayer` /
`serverSendToEndpoint`, and 4 remain inside resident `server.cpp` /
`server-ice.cpp` transport ownership. Client outbound (`mpSendPacket`,
`mpShutdown`, `sendReconnectRequest`) now prefers `CAP_TRANSPORT::send` under
`MIMITA_GAME_DLL` with the resident path as fallback. Packet IDs, schemas,
ordering, and retransmit logic are unchanged. Inbound command uses
`on_event(722)` and inbound `PACKET_PING` uses
`on_event(721)` + `CAP_TRANSPORT::send(peer)`; the remaining handlers still use
`SOCKET`/`sockaddr_in`. CAP_ENTITY v2 adds enumeration/snapshot fields and
CAP_WORLD v1 adds read-only world info/raycast; legacy chat and typing now use
them. Session identity/password, match/community mode, and transport session
binding and reliable-event identity/enqueue now have bounded resident
operations; CAP_ENTITY v5 adds bounded record creation, authoritative transform
reset, NPC spawn, teleport, and disconnect removal; CAP_WORLD v2 adds resident
safe-spawn selection. Reconnect, HELLO, and the local/free JOIN path are
and chat v2 are migrated, along with coordinator/VIP JOIN, VIP style broadcast,
avatar-manifest delivery, and godball state. Input/movement, shot/attack/melee,
ragdoll, vehicle, and avatar asset handling remain resident. Separately, `tools/escrow/` is a tested
standalone C++17 escrow CLI outside `src/`. Build and runtime verification
remain A-owned;
B holds the claim until A confirms the host and whole-game builds, then fills
`Finished`.

## Reserved shared paths

Only by explicit request to Agent A:

- `src/main.cpp`, `src/main-init.cpp`, `src/main-systems.cpp`
- `src/game/game-objects-module.cpp`
- `src/terminal/terminal-state.h`
- `docs/handoffs/*` evidence files
- `config/source-owners.json`

## Build lock

Only one agent may run the dev-loop host build or the whole-game worker at a
time. Check `python devscripts/dev-loop.py --status` first and wait for the
current build to finish.

## Owner-directed writes (2026-11-14)

The owner directed Agent C to write the hot-reload decisions and the hot/cold
split, which live under Agent A's nominal `docs/specs/hotreload/**` claim:

- `docs/specs/hotreload/hotreload.md` — new append-only
  `## Decisions (2026 11 14 1008)` section (D1–D12).
- `docs/specs/hotreload/hot-cold-split.md` — new absolute hot/cold file.
- `docs/specs/dev-loop/quality-gates.md` — static-gate subsection.
- `docs/specs/debug-logging/debug-logging.md` — raw-sink rule.

The owner also directed Agent C to own the audit-tooling consolidation
(`audit_triage.py` absorbs the four folded scripts) and to edit
`src/host/host-runtime.cpp` for the shared render mapping. These are explicit
owner exceptions to the lane split, recorded here.

### B append-only spec/docs pass (2026-11-14)

Owner authorized B to add the hotness/state/input spec material. Existing spec
lines were not rewritten. Added:

- `docs/specs/hotreload/status.md`
- `docs/specs/hotreload/hotness-contract.md`
- `docs/specs/hotreload/state-domains.md`
- `docs/specs/hotreload/input-layer.md`
- appended a companion/no-fake-hot/anti-debt/live-evolution section to
  `docs/specs/hotreload/hotreload.md`;
- appended packet-schema split/live-evolution to
  `docs/specs/networking/networking.md`;
- appended hotness/generation events to
  `docs/specs/debug-logging/canonical-jsonl.md`.

No `docs/ROUTER.md` edit; companion docs are linked from `hotreload.md`.

## Legacy status (2026-11-14)

This manual table is now a compatibility view only. The authoritative plan,
ownership, claim, and build-lock registry is
`docs/handoffs/hot-reload-work-plans.json`. Do not add rows here. Do not use
this file to reserve new paths. Overlapping claims fail validation in the
generated registry.
