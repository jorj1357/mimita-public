# Fun Counter Strike NPC team retaliation

## Changes

- A player-caused NPC kill now adds the killer as a full-confidence enemy report to the victim's existing team brain in Fun Counter Strike.
- NPC target scoring temporarily prioritizes a player with a fresh team report while preserving opponent-team filtering and NPC candidates.
- Added the bounded `npc.team-retaliation` event to identify the killer, victim, and affected team.

## Evidence

- Reproduced the pre-fix behavior in isolated run `20261122_210337`: player 1 killed opposing NPC 100010 at 1206.0578; the respawned NPC selected hostile NPC 100008 at 1206.20 instead of player 1.
- The root dev-loop accepted a DLL build request, but the candidate stopped during the packet registry codegen check because `config/hot-reload/packet-ledger.json` was stale relative to current packet dispatch sources. It never compiled the changed C++ translation units; there is no fresh DLL or post-fix runtime proof.
- A direct read-only `packet_registry_codegen.py --check` confirmed the stale packet ledger. `src/network/server-packets.cpp` has an unrelated pre-existing 32-line change; it was preserved.

## Next step

- After the packet registry owner regenerates the ledger through the canonical loop, request a fresh DLL, launch a run pinned to the exact EXE and DLL, and verify a player kill followed by the NPC squad selecting and engaging the killer.
