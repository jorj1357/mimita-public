# GUI button click-edge investigation and fix

- Timestamp: 2026-10-13T20:50:00-04:00
- Result: PASS_WITH_HUMAN_REVIEW

## Change

Added bounded StructuredLogger diagnostics for UI mouse transitions and shared
button decisions. Fixed click-edge ownership so rendering-only UI passes no
longer consume the left-button edge before the interactive pass processes it.
Only the pass with `ownsHoverInput=true` now calculates and commits the edge.
Documented that ownership invariant in `docs/specs/gui/guiv2.md`.

## Evidence

- The prior journal showed `player-architecture-labels` with
  `owns_hover_input=false` observing `click_edge=true`, followed by the pause
  menu seeing `mouse_down=true` but rejecting `leaveButton` with
  `mouse_click_edge_missing`; `dropdown_modal_active` was false.
- This identifies the first incorrect step as shared mouse-edge consumption,
  not the Leave Room action or stale dropdown modal state.
- Dev-loop published build 2203 from the changed source and launched mode 10
  (`zombietower4`). Status confirmed `builds_match=true`, `client_ready=true`,
  and executable `C:\mimita-v9\.dev\builds\2203\mimita.exe`.
- Fresh journal: `C:\mimita-v9\logs\2026-10-13\20261013_201101\events.jsonl`.
  It contains no button records because the computer-control bridge was
  unavailable before a real click could be performed.
- Source whitespace validation passed with only normal LF/CRLF warnings.

## Remaining acceptance

Human-visible Leave Room acceptance remains required: click Leave Room, then
Confirm, and verify the view exits the room. The computer-control bridge
reported `Trusted RPC service is not configured`, so no UI click was performed
in this run.

## Preserved unrelated edits

Existing changes in `config/gamemodes/zombie_tower.json`,
`devscripts/dev-loop.py`, `docs/specs/dev-loop/dev-loop.md`, and
`src/network/server-gamemode.cpp` were preserved.
