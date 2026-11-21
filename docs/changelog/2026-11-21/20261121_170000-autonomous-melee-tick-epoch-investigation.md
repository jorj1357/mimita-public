# Autonomous melee tick-epoch investigation

- Recorded: 2026-11-21
- Issue: `issue-adfeaafa1136`
- Status: source patch attempted; build/runtime proof pending.

## Changes

- Added a tick-epoch reset during prior multiplayer session teardown in `src/network/multiplayer-packets.cpp`, clearing retained snapshot tick, timing, counters, and session identity mismatch state.
- Regenerated the packet ledger and confirmed `python devscripts/packet_registry_codegen.py --check` passes.
- Added the reusable gold investigation/evidence workflow and an append-only attempted-fix regression record.

## Evidence and limits

- In run `20261121_144350`, client Mace claim line 55468 had `contact_tick=43683`; server line 55482 rejected `future_contact` at tick 8998. 254 of 264 melee claims were rejected for this reason. This was Payload/funworld3, not the reported FCS run, and process identity metadata conflicts.
- The root dev-loop attempt `game-dll-20261121T165408308526Z-55ebed` failed at packet registry codegen before compilation. Metadata was regenerated afterward; standalone check passes, but the loop latches revision 2 as failed. No pinned runtime proof was obtained.
- The user's server PID 7556 remains alive and untouched; it has no client attached, so it cannot validate melee.

No behavior completion or human acceptance is claimed. Follow-up acceptance is in the issue and regression record.
