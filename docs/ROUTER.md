// 09 03 2026, 15 40
/* purpose
* route AI agents and humans to the correct authoritative project documents
* explain the document order before code is inspected or changed
* prevent agents from guessing when the repository already has written guidance
* this file DOES NOT contain the full game specifications
* this file DOES NOT replace the documents it routes
* this file DOES NOT make archived material authoritative
*/

# MiMITA Documentation Router

This is the map for the project documentation.

## Document hierarchy

- `AGENTS.md`: project introduction and permanent working rules.
- `docs/ROUTER.md`: this map and the required reading order.
- `docs/specs/`: desired behavior; the main source of truth for what the game should do.
- `docs/architecture/`: ownership and structural rules.
- `docs/workflows/`: methods for common kinds of work.
- `docs/operations/`: build, deployment, assets, and completion procedures.
- `docs/skills/`: focused review checklists used for the task.
- `docs/regressions/`: confirmed failures and their permanent lessons.
- `docs/changelog/`: one final record for every AI work session.
- `docs/features/`: stable feature records linking goals, owners, tests, logs,
  changelogs, regressions, and human acceptance.
- `docs/gold/`: examples of complete evidence and changelog quality.
- `docs/doc-review-09-03-2026.md`: current documentation review notes.

The specification describes the desired behavior. If code disagrees with it,
the code is wrong unless a human changes the specification first. Quote the
specification, explain the disagreement, and align the implementation as far as
the task safely allows.

## Every task

1. Read `AGENTS.md`.
2. Read this router.
3. Classify the task.
4. Read the relevant specification first.
5. Read related architecture and workflow documents.
6. Read the required focused skills, including
   `docs/skills/spec-behavior-review-v1.md` for behavior changes or spec review.
7. Inspect the current code and pre-existing changes.
8. Compare the implementation with the specification.
9. Make the smallest correct change.
10. Run focused validation and build when required.
11. Write one changelog file immediately before completion.
12. Report evidence separately from human review still needed.

## Route priority

When several routes apply, check the dependency most likely to break other
systems first. Use this default order:

1. documentation and specification
2. logging and diagnostics
3. performance
4. terminal commands
5. in-game chat
6. moderation
7. assets

## Common routes

| Task type | Read first |
|---|---|
| Documentation | `docs/doc-review-09-03-2026.md`, the relevant docs folder, `docs/skills/documentation-checker-v1.md` |
| UI, HUD, menus, text | `docs/specs/gui/gui.md`, `docs/skills/documentation-checker-v1.md` |
| Damage, weapons, knockback | `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md` |
| Movement, physics, collision | `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md` |
| Multiplayer or server behavior | `docs/specs/networking/networking.md`, `docs/specs/gamemodes/gamemodes.md` |
| Logging or diagnostics | `docs/specs/debug-logging/debug-logging.md`, `docs/skills/logging-checker-v1.md` |
| Performance | `docs/specs/performance/performance.md`, `docs/skills/efficiency-checker-v1.md` |
| Terminal commands | `docs/architecture/terminal-commands/terminal-commands.md`, `docs/skills/terminal-command-checker-v1.md` |
| In-game chat | `docs/specs/ingame-chat/ingame-chat.md`, `docs/skills/chat-checker-v1.md` |
| Moderation | `docs/specs/moderation/moderation.md`, `docs/skills/moderation-checker-v1.md` |
| Assets or sound | `docs/operations/asset-management/asset-management.md`, `docs/skills/asset-checker-v1.md` |
| Build or EXE | `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md` |
| VPS inspection or repository-to-production comparison | `docs/operations/vps-audit.md`, `docs/operations/vps-deployment/vps-deployment.md`, `docs/operations/task-completion/task-completion.md` |
| Persistent progression, rewards, profiles, leaderboards | `docs/specs/data-saving/data-saving.md`, `docs/operations/vps-audit.md`, `docs/operations/persistence-recovery.md` |

Also read `docs/regressions/regressions-v1.md` for any bug or human-reported
break. Read JSON architecture when settings or hot reload are involved.

For active configuration, resolve the runtime-loaded path before inspecting or
editing candidates. Files marked `.bak`, `.archive`, `.experimental`, dated
copies, or free-form historical text are archive material unless an active
configuration manifest explicitly identifies them.

## Historical documents

Documents under historical or archive locations are background reference only.
They must not override current specifications, architecture, workflows,
operations documents, skills, or regression records.

## Regression rule

If human review discovers that previously working behavior broke, connect the
failure to the changelog, record the exact wrong and corrected code, explain the
cause, and add a careful append-only regression entry.

## Spec/doc TODOs

If there is a comment in any document or spec like "todo: explain this better", the agent notes this, and in it's response, it notes the exact file path and line number that has that todo, and then suggests an expansion or a fix for that specific todo, but does not edit the document/spec.
