# Opening the terminal typed the grave key

- Recorded: 2026-11-18 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — live input validation pending
- Owner: `src/game/game-input-module.cpp` host-dispatched keyboard and character callbacks

## Changelog

- [Console grave toggle input](../../changelog/2026-11-18/20261118_093329-console-grave-toggle-input.md)

## Symptom

Pressing grave to open the in-game terminal also placed a grave character at the
start of the command input.

## Cause

The host-dispatched GLFW key callback handles the grave key first. In
`mimita::game::handleGameInputKey`, it toggles `Terminal` open and returns.
GLFW then sends the character callback for the same physical key press.
`mimita::game::handleGameInputChar` forwards that character to
`Terminal::handleChar`; because the terminal is already open,
`Terminal::handleChar` inserts it into the command line. The ordering changed
the meaning of the single physical press between the key and character event:
the key opened the terminal, while its paired text event was interpreted as
user-entered command text.

## Fix saved

The key handler now arms a one-character suppression when grave toggles the
terminal. The next character event consumes that suppression; grave or tilde
(the shifted form) is discarded, and any other character is routed normally.
This keeps the toggle key out of terminal text without suppressing subsequent
typing.

## Evidence

- Source trace confirmed the paired event path and that `Terminal::handleChar`
  accepts characters only while open.
- `git diff --check` passed for this change.
- No build or runtime validation was performed. Source fix is saved; live
  behavior remains pending and is not claimed fixed.

## Runtime acceptance still required

On a client running the generation containing this change, verify that grave
opens and closes the terminal without adding either `` ` `` or `~` to its
input, and that ordinary command typing still works immediately afterward.
Record the active generation and journal separately from human visual review.
