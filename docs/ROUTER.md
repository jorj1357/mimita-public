# MiMITA Documentation Router

This file is the map for humans and AI agents.

## If the task is a bug

1. Read `C:\mimita-priv-v8\docs\regressions\regressions-v1.md`.
2. Read the workflow that matches the bug in `C:\mimita-priv-v8\docs\workflows\`.
3. Read the relevant behavior specification in `C:\mimita-priv-v8\docs\specs\`.
4. Read the relevant architecture note in `C:\mimita-priv-v8\docs\architecture\`.
5. Search the code for the current owner and trace the event from input to visible output.
6. Make the smallest fix and validate the exact user-visible result.

## Common routes

| User says | Read first |
|---|---|
| UI, HUD, menu, text, damage numbers do not show | `docs/workflows/fix-repeated-bug.md`, `docs/specs/gui/gui.md` |
| Damage, hit, weapon, knockback | `docs/workflows/fix-repeated-bug.md`, `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md` |
| Movement, physics, collision | `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md` |
| Multiplayer or server behavior | `docs/specs/networking/networking.md`, `docs/specs/gamemodes/gamemodes.md` |
| JSON or hot reload | `docs/architecture/json-configuration/json-configuration.md` |
| Build, EXE, or deployment | `docs/operations/build-and-exe/build-and-exe.md` |

## Permanent rule for regressions

`docs/regressions/regressions-v1.md` is append-only.

When a previously working behavior breaks, add a new dated entry describing the
bad behavior, what was learned, and what fixed it. Never delete or rewrite an
older entry. In the final report, explicitly say whether a regression entry was
added. If no entry was added, say why.

