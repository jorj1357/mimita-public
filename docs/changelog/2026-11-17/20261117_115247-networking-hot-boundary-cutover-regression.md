# Networking hot boundary, performance target, dependency graph, and Cutover-1 regression

- UTC timestamp: 2026-11-17T11:52:47Z
- EST timestamp: 2026-11-17 06:52:47 EST
- Scope: specifications, regression record, devscript, roadmap, and handoff
- Build: no build requested; no runtime acceptance claimed

## Summary

Corrected the documentation that let Cutover-1 look complete while networking
policy was still resident. Added the compiler-derived dependency graph tool and
documentation, fixed the performance acceptance target to per-frame 99.99% at
240 FPS with 999 FPS as aspirational, made `ROADMAP.md` the authoritative
priority order, and recorded the confirmed Cutover-1 networking gap as a
regression.

Expanded the networking split handoff into the confirmed Agent A execution plan:
this agent takes Agent A plus integration, one combined cold boundary is
accepted, and `ice-server`/`udp-echo` are recorded as diagnostic tools rather
than production policy. Added the function/line audit and the ABI checkpoint
proposal: extend the existing transport, ICE, and transport-session capabilities
with opaque attach/detach, polling, maintenance, and swap state instead of
adding a parallel network API. Added the source-conversion plan: split mixed
translation units first, add the resident core, move ICE/server policy hot, then
update ownership and prove the live-fixable boundary.

No network source split was implemented in this change. The networking owner
must still move ICE negotiation/listener policy and server session policy out of
resident files and prove connection survival across a DLL swap.

## Files

### Added

- `devscripts/dependency_graph.py`
- `docs/operations/devscripts/dependency-graph.md`
- `docs/regressions/hotreload/README.md`
- `docs/regressions/hotreload/cutover-claimed-complete-REG-2026-11-17.md`
- `docs/handoffs/networking-hot-boundary-split.md`

### Updated

- `ROADMAP.md`: authoritative current order, performance target, shared data
  format, later creator/OS work.
- `docs/specs/performance/performance.md`: per-frame 99.99% at 4.167 ms;
  999 FPS / about 1 ms labeled aspirational; one-second windows demoted to
  diagnostics; VSync target corrected.
- `docs/specs/networking/networking.md`: normative hot/cold boundary table and
  ICE negotiation/listener policy definition; swap reattachment requirement.
- `docs/specs/hotreload/hotreload.md`: explicit ICE negotiation/listener hot
  policy, cold primitive exceptions, and no swap-only reconnect.
- `docs/specs/hotreload/hotness-matrix.md`: ICE row, current-gap note, room/
  join/reconnect target updates, and corrected cold-only mechanism list.
- `docs/regressions/README.md`, `docs/regressions/master/README.md`: hot-reload
  regression domain index.
- `docs/specs/gamemodes/rpg.md`: creator work follows the roadmap priority.
- `docs/specs/open-gameplay-issues.md`: current performance priority and
  superseded Zombie Tower/240 FPS priority notes.
- `devscripts/README.md`, `docs/operations/devscripts/script-inventory.md`,
  `docs/operations/devscripts/cold-change-guard.md`: dependency graph entry and
  cross-link.
- `docs/handoffs/hot-reload-agent-focus.md`: pointer to the split handoff.

## Verification

Source inspection and report-only commands:

```text
python -m py_compile devscripts/dependency_graph.py
python devscripts/dependency_graph.py --top 10
python devscripts/dependency_graph.py --write
python devscripts/dependency_graph.py --dependents src/host/abi/transport-session-capability.h
python devscripts/dependency_graph.py --focus src/host/abi/transport-session-capability.h
python devscripts/dependency_graph.py --affected src/network/packets.h
python devscripts/cold_change_guard.py --report-only ...
```

Observed:

- `dependency_graph.py` parsed the latest game object directory plus debug and
  release `.d` files and wrote `build/reports/dependency-graph.json`.
- `--focus` and `--dependents` on
  `src/host/abi/transport-session-capability.h` reported
  `src/game/transport-session-bridge-module.cpp` and
  `src/host/host-runtime.cpp`.
- `--affected src/network/packets.h` reported 187 affected sources.
- Cold guard reported `COLD` for `server.cpp`, `server-ice.cpp`,
  `ice-agent.cpp`, `ice-server.cpp`, and `net_common.cpp`; `HOT` for
  `multiplayer-packets.cpp` and `coordinator-client.cpp`.

Packet codegen was verified after the handoff:

```text
[PACKET CODEGEN] unchanged packets=78 header=src/host/generated/packet-registry-v1.h ledger=config/hot-reload/packet-ledger.json
[PACKET CODEGEN] ok packets=78
```

The dev loop then reported successful game-generation builds; generation 320
was published with `[PACKET CODEGEN] ok packets=78`. No generator change was
needed. No build was requested by this session.

This is source/documentation evidence only. It is not runtime proof.

## Remaining work

- Split resident networking policy from primitive socket/libjuice mechanisms.
- Retain the resident ICE/session handle across generation unload and reattach
  without a reconnect.
- Prove same-PID live ICE/join/reconnect/session edits and connection survival
  across repeated swaps.
- Rerun `source_manifest.py`, the cold guard, and the dependency graph after the
  source split updates ownership.
- Do not claim Cutover-1 networking complete until the regression’s acceptance
  evidence exists.
