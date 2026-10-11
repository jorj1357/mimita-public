# Privileged Transform Authority Documentation

Time: 2026-10-10 19:25:30 -04:00 (America/New_York)
Branch: `2026-10-10-Z-Tower`
Status: PASS_WITH_HUMAN_REVIEW

## Scope

Documentation-only follow-up to the server-position-error investigation. No
source, configuration, packet, or runtime behavior was changed.

## Files changed

- `docs/specs/networking/networking.md`
  - Added section `3.1 Privileged administrative transform operations`.
  - Defined `fly`, `unfly`, administrator teleport, transform epochs,
    authoritative handoff, and the rule that permission authorizes a request
    without making the client authoritative.
- `docs/specs/movement/movement.md`
  - Added section `16.1 Privileged movement and transform handoff`.
  - Defined one authoritative-transform owner and separated command permission,
    transform application, ordinary movement validation, and reconciliation.
- `docs/architecture/terminal-commands/terminal-commands.md`
  - Added `Privileged transform command contract`.
  - Defined the required server permission, validation, epoch, acknowledgement,
    and structured-result behavior for transform commands.

## Key wording change

Previous documentation grouped administrative movement with ordinary movement
validation and did not define a single transform handoff. The new contract
states:

> An administrator command is not ordinary client movement and must not reuse
> ordinary movement validation as if it were a normal input report.

and:

> The administrator's permission authorizes the request; it does not make the
> client authoritative. The server remains the one authority that applies the
> result.

## Reasoning

The documentation now gives the next implementation pass one target path:

1. authorize the command on the server;
2. apply the transform or flight-mode transition through one owner;
3. increment the transform epoch and reset stale movement/prediction state;
4. publish and acknowledge the authoritative transform; and
5. resume ordinary movement validation only after the handoff.

The intended exception is explicit server-authorized flight or teleport, not a
general trust rule for client-supplied positions.

## Documents and focused skill used

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/2026-09-16/2026-09-16-movement-authority-adopt-fixed-rubberband.md`
- `docs/regressions/2026-09-16/2026-09-16-server-client-generation-mismatch-spawn-lock.md`

## Validation

- `git diff --check`: passed.
- All three cross-referenced documentation paths exist.
- Documentation-only task; no build, executable, runtime journal, or human
  gameplay validation was required or performed.
- Existing unrelated worktree changes were preserved and are not claimed here.

## Documentation review notes

The documentation checker found pre-existing informal TODO markers throughout
`docs/`; none were introduced by this change and none were edited. The most
directly related existing marker is `docs/specs/networking/networking.md:15`,
which says `todo explain all console commands either in here or a centralized
document`. This change addresses that request for privileged transform
commands by linking the networking specification to the terminal-command
architecture document; the broader command inventory remains future work.

Human review is still needed before implementation: confirm the exact admin
permission model, whether ordinary administrator teleport destinations may be
inside geometry, and whether flight should be represented as a distinct
movement authority mode or as a transform-operation lease.
