```md
/* purpose
 * review recent code for GUI presentation that bypasses MiMITA's JSON-driven GUI system
 * ensure every GUI element and presentation property remains JSON hot reloadable
 * detect new bespoke GUI/rendering paths before they become permanent architecture
 * preserve the rule that feature/gameplay code provides state/actions, not presentation
 */

# GUI Hardcoding Checker v1

Use this skill after changes that add, modify, or interact with GUI, HUD,
world-space UI, menus, notifications, damage numbers, or other presentation.

MiMITA's rule is:

> If it is a GUI element, its presentation must be defined by JSON and hot reloadable.

Do not accept a GUI implementation merely because it visually works.

## 1. Hardcoded GUI presentation

Search the actual diff and surrounding callers for presentation defined directly
in C++ or feature/gameplay code.

Flag hardcoded:

- visible text or labels;
- positions, offsets, anchors, sizes, or scales;
- colors, alpha, borders, shapes, or fonts;
- images, image transforms, or image presentation;
- Z-order/layering;
- animation/effect parameters;
- GUI timing/lifetimes;
- screen-space or world-space GUI appearance.

Example suspicious code:

`drawText("YOU HAVE THE BOMB", 500, 30);`

The feature should expose state such as:

`localPlayerHasBomb`
`bombTicksRemaining`
`bombHolderName`

JSON decides how that state looks.

## 2. Bypassing shared GUI primitives

Search before accepting a new GUI helper, renderer, widget, or draw path.

Flag code that:

- creates a bespoke renderer for one feature;
- directly draws GUI instead of using approved primitives;
- duplicates Text, Image, Shape, Container, input, or layout behavior;
- creates separate live/replay/world GUI implementations unnecessarily;
- makes a GUI property impossible to change through JSON.

Prefer composition of the smallest existing primitives.

Do not add `BombTagTimerRenderer` when the same result can be composed from
shared world-space Text/Container primitives.

## 3. Hot reload violations

Every JSON-controlled presentation change must be able to update while MiMITA
is running without rebuilding or restarting.

Flag code where:

- JSON is only read at startup;
- a presentation value is copied into a permanent hardcoded default;
- added/removed elements cannot hot reload;
- reload requires reconnecting or restarting;
- feature code overrides a JSON value after loading it.

Hot reload should parse and validate changes, then apply a valid update at a
safe client/UI tick.

## 4. Performance regressions

GUI must remain inexpensive on low-power devices.

Specifically check for the known regression class where text or another logical
element is split into many individually rendered GUI objects.

Flag:

- one GUI object per character;
- unnecessary per-element allocations every frame/tick;
- rebuilding unchanged GUI trees every frame;
- repeated JSON parsing every frame;
- duplicate layout/text measurement work;
- effects tied to rendered FPS instead of the client/UI tick.

Per-letter styling/effects must not require one independent GUI object/draw path
per letter.

Prefer batching and persistent GUI state.

## 5. Allowed implementation code

Do NOT flag hardcoded implementation details required inside the canonical GUI
engine itself merely because C++ contains numbers or rendering logic.

The shared primitive renderer, JSON parser, layout engine, text renderer,
hot-reload system, and backend need implementation code.

The violation is when a feature bypasses that system or makes presentation
non-JSON-editable.

Trace ownership before reporting a finding.

## Evidence rules

For every finding include:

1. exact file and line range;
2. the GUI element/property being hardcoded or bypassed;
3. the existing shared primitive/config path that should own it, if one exists;
4. whether the value can currently hot reload;
5. the smallest corrective direction;
6. confidence: `confirmed`, `likely`, or `needs investigation`.

Do not claim something is hardcoded merely because a literal exists.
Trace how the visible GUI value is actually produced.

## Required output

Report findings in severity order:

`[P0/P1/P2/P3] title — file:line`

Then provide:

`Evidence`
`GUI owner`
`Why it violates the GUI rules`
`Smallest fix`
`Confidence`

Finish with `No finding` for each checked category with no supported issue.

Do not automatically rewrite GUI code unless separately requested.
Preserve unrelated edits.

A new GUI feature that cannot be controlled through JSON is a regression.
A change that introduces hardcoded GUI presentation must not be accepted.
```