# Map editor keyboard and text-field shortcuts were not routed in the hot client

- Recorded: 2026-11-18 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — live input validation pending
- Owner: `src/game/game-input-module.cpp` host input dispatch and
  `src/gui/ui-text-input.cpp` shared text editing

## Changelog

- [Map editor keyboard and text editing](../../changelog/2026-11-18/20261118_091958-map-editor-keyboard-and-text-editing.md)

## Symptom

In the Map Editor, keyboard shortcuts and text-field editing keys were not
reliable in the running hot client. The requested controls include typing,
Ctrl+A/C/X/V, Ctrl+Z/Y, map copy/cut/paste/undo/redo, and Enter to apply a
focused property field.

## Specification

`docs/specs/entity-editor/entity-editor.md`, “Map Editor v1 viewport mode”:

> Ctrl+C/V/X       copy / paste / cut
> Ctrl+Z           undo
> Ctrl+Shift+Z/Y   redo
> Enter            apply the focused position field

Focused text editing is specified there as field typing and Enter-to-apply;
this record clarifies that text editing shortcuts are field-local while the
same shortcuts operate on map objects when no text field is focused.

## Source trace and cause

- The normal `InputCommandSystem::keyCallback` routes focused map-editor text
  first, then editor shortcuts.
- The hot-client host-dispatched path enters at
  `mimita::game::handleGameInputKey` in `src/game/game-input-module.cpp`, which
  previously handled menus/chat but never called either Map Editor key owner.
  Thus key events could reach the DLL while editor shortcut/navigation handling
  was skipped.
- `MapEditor::handleTextKey` routes keys to the focused field, and the shared
  `uiTextInputHandleKey` already supports Ctrl+A/C/X/V, but not text Ctrl+Z/Y.
  Since the focused-field handler consumes the event, the map undo handler
  cannot act as a text-edit undo fallback.

## Fix saved

- Route host key events to `MapEditor::handleTextKey` and then
  `MapEditor::handleKey` after terminal/chat ownership. Focused fields consume
  their editing/navigation shortcuts before object editing shortcuts.
- Add bounded 64-step text undo/redo snapshots to the shared text-input owner.
  Ctrl+Z undoes; Ctrl+Y and Ctrl+Shift+Z redo. Typing, cut, paste, backspace,
  and delete participate in the history. Programmatic field value changes reset
  that field's history so undo cannot restore text from a previous selection.
- Let Ctrl+S pass through a focused Map Editor field to save the map.
- Clarify field-local versus map-object Ctrl shortcuts in the editor spec.

## Evidence

- Source manifest marks the edited `src/game/game-input-module.cpp`,
  `src/gui/ui-text-input.cpp`, and `src/mapeditor/map-editor.cpp` as `game`
  owned, so the root dev-loop can publish them as a game generation.
- `git diff --check` passed for the source and spec edits.
- The last observed client generation 338 predates these keyboard edits. Build,
  activation, and live text-field/map-shortcut behavior for this fix are still
  pending; generation 338 is not evidence for this change.

## Runtime acceptance still required

On the client that activates this change, verify in the Map Editor:

1. Type in the outliner search and a property field; cursor movement,
   selection, Backspace/Delete, and Enter behave correctly.
2. In a focused field, Ctrl+A/C/X/V and Ctrl+Z/Y/Ctrl+Shift+Z edit the field
   text without changing map-object selection/history.
3. With no field focused, Ctrl+C/V/X, Ctrl+Z/Y, Delete, and Enter retain their
   documented map-editor meaning.
4. Ctrl+S saves while a field is focused.

Record build generation, same-PID activation, exact client journal, and visual
acceptance separately. Do not mark this fixed until those checks pass.
