# Shared logging and temporary NPC movement corrections

Date: 2026-10-05

## Implemented

- The GUI server launch now initializes and exports the client's canonical
  `MIMITA_EVENTS_FILE` before `CreateProcessA`, so the client and dedicated
  server append to the same `events.jsonl` path.
- NPC wall and stuck responses are temporary local corrections. They preserve
  the existing navigator commitment, avoid forcing a new route for every local
  correction, and resume the committed direction when the correction clears or
  its short hold expires.
- Visible combat movement can still take priority over a temporary correction.
- Jumping remains available for obstacle, climb, and stuck recovery cases.

## Evidence

- Build: `BUILD SUCCESS` from `python build_agent.py`.
- `--npc-shared-log-path-selftest`: PASS.
- `--npc-movement-commitment-selftest`: PASS.
- `--npc-navigation-selftest`: PASS.
- `--npc-movement-decision-selftest`: PASS; 3 snapshots over 180 ticks with
  the complete movement-decision field set.

## Human validation remaining

- Start a fresh GUI/dev-loop Counter-Strike session and confirm `log_open`
  reports one `events.jsonl` containing both `process=client` and
  `process=server`.
- Observe CT and T NPCs on Dust2 and confirm jumps/wall turns return to the
  same travel commitment instead of causing repeated circling or needless
  replanning.

## Unrelated working-tree changes

Pre-existing edits to `config/accounts/default.json` and
`config/npc-difficulty.json` were preserved.
