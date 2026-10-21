# Stretched fullscreen resolution specification

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-20 19:26:08 EDT (2026-10-20T23:26:08.638Z)
- Branch: `20261011zt`
- HEAD before this documentation change: `0b5145b26796b94af55c1744c9b00f86cc44bd94`
- Scope: document the user's clarified Counter-Strike-style stretched fullscreen behavior. No runtime code, settings, or UI implementation changed.

## Source change

File: `docs/specs/graphics/graphics-resolution.md` (untracked before this task; its informal notes were pre-existing user content).

Prior content, verbatim:

```text
2026 10 20 1920 

fullscreen 

hmmmmmmmm 

need like gui scaling to be good
butthat is in the other spec

but 
also

need borderless fulslcrnee
ALSO

SCTRETCHED

SCTRETCHED PLZ

fullscreen 0 = windowed
fullscreen 1 = fullscreen totally
fullscreen 2 = fullscreen borderless
fullscreen 3 = fullscreen totally stretched
fullscreen 4 = fullscreen totally borderless
```

New content, verbatim (source lines 1-45):

```markdown
# Graphics Resolution and Fullscreen

## Purpose

Define the available window and fullscreen modes, including Counter-Strike-style
stretched game resolution.

## Audience

This specification is for the graphics settings, window-management, and
rendering owners.

## Scope

This specification covers the game image and display mode. GUI scaling and
layout are defined by [the GUI specification](../gui/guiv2.md).

## Does not define

This specification does not define GUI scaling, GUI layout, or UI aspect-ratio
behavior.

## Fullscreen modes

The `fullscreen` setting uses these values:

| Value | Mode | Behavior |
| --- | --- | --- |
| `0` | Windowed | Run in a normal window. |
| `1` | Exclusive fullscreen | Use exclusive fullscreen at the selected game resolution. |
| `2` | Borderless fullscreen | Fill the display with a borderless window, without the stretched-resolution behavior below. |
| `3` | Exclusive fullscreen stretched | Use exclusive fullscreen and stretch the game image to fill the display. |
| `4` | Borderless fullscreen stretched | Fill the display with a borderless window and stretch the game image to fill it. |

## Stretched fullscreen

In modes `3` and `4`, render the game image at the selected game resolution and
scale it to fill the display, even when their aspect ratios differ. For
example, render the game at 4:3 on a 16:9 monitor, then stretch the image
horizontally to fill the 16:9 display. Characters and other objects therefore
appear wider, like stretched-resolution play in Counter-Strike.

Stretched modes fill the display without adding black bars or cropping the
game image. Modes `3` and `4` use the same stretch behavior; they differ only
in exclusive versus borderless fullscreen.
```

Reason: the original mode list assigned modes 2 and 4 the same borderless meaning and used shorthand for stretching. The user clarified that a 4:3 game view on a 16:9 monitor should stretch horizontally so characters appear wider, and asked to ignore UI. The spec now maps mode 3 to exclusive stretched fullscreen and mode 4 to borderless stretched fullscreen, describes the no-bars/no-crop result, and leaves GUI scaling to `docs/specs/gui/guiv2.md`.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/graphics/graphics-resolution.md`, `docs/specs/gui/guiv2.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/skills/documentation-checker-v1.md`: PASS for the edited spec's purpose, audience, scope, explicit `Does not define` section, and relative GUI-spec link. The GUI target exists. Existing `docs/ROUTER.md` points to `docs/doc-review-09-03-2026.md`, which is absent in this checkout; no equivalent doc-review file was found.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The user's clarification resolves the intended 4:3-to-16:9 stretch result and allows modes 3 and 4 to be distinguished as exclusive and borderless. No implementation behavior was inferred or changed.
- The documentation checker's repository-wide TODO scan found older unrelated TODO comments in active docs (including GUI, code ownership, weapons, networking, and gamemode documents) and archive material. They were not edited as part of this focused graphics specification change.

## Validation and review still needed

- Validation: inspected the complete edited Markdown and verified the linked GUI specification exists. No tests or builds were run because this was a documentation-only request.
- Runtime behavior: not validated; this change only records desired behavior.
- Human review: the user clarified the stretch result. Any later implementation still needs visual confirmation that a 4:3 scene fills 16:9 and appears horizontally wider, with mode 3 exclusive and mode 4 borderless.

## Pre-existing working-tree changes preserved

At task start, these unrelated files were already modified and were left untouched: `config/accounts/default.json`, `config/actor-collision.json`, `config/aimbody.json`, `config/analytics.json`, `config/gamemodes/sandbox.json`, `docs/jorj-docs/20261020plan.md`, `src/config/actor-collision-config.cpp`, and `src/pch.h.d`. `docs/specs/actor-presets/actor-presets.md` was also modified in a later status observation and was left untouched.
