# Human melee claims rejected after session tick epoch changes

- Recorded: 2026-11-21
- Status: ATTEMPTED FIX; current patch has no successful build or runtime proof.
- Issue: `issue-adfeaafa1136`
- Affected area: client snapshot clock, reconnect teardown, melee contact claims.

## Observed behavior

In run `20261121_144350`, a focused human-controlled client emitted Mace contact claims against NPCs. The client claim at `client-01/events-0010.jsonl` line 55468 named NPC 200005 and `contact_tick=43683`; the server rejected it at line 55482 with `reason=future_contact`, server tick 8998, and delta 34685. Across the journal, 254 of 264 melee claims were rejected as `future_contact`. The client retained a high cached server tick while incoming snapshots and an apparent replacement server used lower ticks.

This run was Payload/funworld3, not the requested Fun Counter Strike run. Its process manifest and journal PIDs also disagree, so it is strong evidence for the failure mechanism but does not prove the user's exact session or build identity.

## First failing stage and source explanation

The failure occurs at authoritative server validation, before damage application. `src/combat/melee-client.cpp` stamps a claim using the cached server tick; `src/network/server-packet-handlers.cpp` correctly rejects contacts more than 120 ticks in the future. `teardownPreviousSession` in `src/network/multiplayer-packets.cpp` cleared interpolation state but retained the session's monotonic tick high-water marks. When a replacement server starts at a lower tick, its snapshots can be discarded as stale and client claims continue using the prior epoch.

The server future-contact guard must remain. The patch clears `latestServerTick`, `lastSnapshotTick`, per-session snapshot timing/counters, and identity-mismatch state during teardown so a new session can seed a fresh tick epoch.

## Build and proof status

The root dev-loop attempt `game-dll-20261121T165408308526Z-55ebed` failed before compilation at `packet_registry_codegen.py`; the metadata was regenerated afterward and `python devscripts/packet_registry_codegen.py --check` passes standalone. The root dev-loop now reports revision 2 failed and its CLI request path does not expose the internal forced retry. A previous attempt also recorded unrelated unresolved contact-owner linker symbols in the shared tree. No current pinned EXE/DLL or isolated FCS runtime exists for this change.

The user's shared server run `20261121_144350` remains online and was not replaced; it currently has no client and cannot prove melee. The fix is therefore not yet proven.

## Required follow-up proof

On an exact pinned Fun Counter Strike artifact, capture client input and claim, server receive and acceptance, exactly one authoritative damage application, and decreased target health for human-to-NPC and human-to-enemy-player contacts. Also prove NPC-to-local-player and enemy-player-to-local-player damage. Repeat over three independent runs and three reconnects that enter a lower server tick epoch; after each reconnect require successful connection, fresh snapshots, and subsequent melee. Include a five-minute post-reconnect soak with no crash or abnormal exit. Record EXE/build ID, DLL generation, config revision, mode/map/loadout, process identities, run ID, and journal paths. Human visual/gameplay acceptance remains separate.
