# Sandbox NPC role movement fix

- Date: 2026-10-20 20:04 EDT
- Request: Make Sandbox NPCs use the Juggernaut movement preset through their team role, instead of relying on the global `npc-difficulty.json` movement preset.
- Finding: The live Sandbox runtime emitted `npc.movement-decision` for actor 1000 with an empty movement preset and high horizontal velocity. The movement code falls back to the global NPC movement config when an NPC has no role movement ID. Sandbox startup NPCs were adopted before a match roster existed, and already-simulated NPCs were skipped after a mode switch, so no team role was assigned or reapplied.
- Change: In `src/network/server-npcs.cpp`, startup NPC adoption now assigns an NPC-only team's `npc_role` (or its regular `role`) when an NPC has no roster descriptor. The assignment also runs for already-simulated NPCs when a mode switch clears their prior roster. The role's team is applied to the NPC body, and the resolved role profile always replaces the NPC movement ID, including clearing a prior ID when the new role has none.
- Existing configuration: `config/gamemodes/sandbox.json` assigns its NPC-only team to `sandbox_juggernaut_npc`; `config/roles.json` gives that role `movement_preset: juggernaut`. These user-worktree changes were preserved.
- Source validation: `git diff --check -- src/network/server-npcs.cpp` passed.
- Build: `python devscripts/dev-loop.py --request build --wait-for-build --json` published build 2702 successfully. An earlier attempt failed because the new role type header was not included; the include was added, and the final build passed.
- Executable identity: `.dev/builds/2702/mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_200448/events.jsonl`.
- Runtime limitation: The live Sandbox log confirmed the missing role on the pre-fix build. The dev-loop is currently running build 2701 while 2702 is published; the rebuilt behavior has not yet been exercised in a fresh runtime scenario, and human visual speed acceptance remains open. The running game was left alive.
- Tests: None added or run.
- Documentation: Read the routed actor-preset, gamemode, NPC behavior, NPC movement, JSON configuration, runtime scenario, debug logging, networking, build, and task completion guidance. `docs/regressions/regressions-v1.md` is absent from this checkout.
