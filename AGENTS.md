# MiMITA agent instructions — proposed compact version

Favor deleting code over adding code when both solutions achieve the same result.

## MANDATORY GATES

Read these gates before doing anything else:

1. ROUTER CHAIN: This file is not the complete instruction set. Read
   `docs/ROUTER.md` immediately and follow its routes to the relevant
   specification, architecture, workflow, operation, regression, and focused
   review documents. Do not copy the router's document-path table into this
   file; the router is the single routing map.

2. BUILD DOES NOT EQUAL FEATURE: A successful build proves compilation and
   linking only. It does not prove that configuration loaded, a packet was
   sent or received, state was applied, or behavior became visible. Report
   build evidence separately from runtime evidence and human acceptance.

3. TRACE BEFORE BLAMING THE BINARY: When behavior appears inactive, trace the
   complete data path before assuming a stale executable. Network features and
   packet changes require the network-feature workflow selected by the router.

## Authority and scope

- The specification describes desired behavior. Code implements it.
- When code and specification disagree, quote the specification, explain the
  disagreement, and make the smallest safe alignment.
- Search the repository before reasoning from assumptions or adding code.
- Preserve unrelated edits and never claim pre-existing changes as your own.
- Work locally first. Do not make production-only fixes.
- Stop and ask when a required decision would materially change scope or
  behavior.
- Do not claim visual, multiplayer, deployment, or human acceptance without
  directly performing and observing it.

## Evidence and completion

- Follow the router's complete task flow and read every document it selects.
- Use the smallest focused review set that covers the task.
- For bugs, identify the first missing or incorrect step in the input-to-output
  chain and record the exact owner, function, and evidence.
- Improve or verify an appropriate diagnostic at the feature owner. Keep
  repeated diagnostics categorized, useful, and rate-limited.
- Run relevant validation. If code changed, use the canonical build process
  and inspect its reported status.
- Record source, build, test, runtime, deployment, and human-review evidence
  as separate claims.
- Every repository-touching AI session creates exactly one final changelog.
- Confirmed regressions are recorded append-only; ordinary work history is not
  a regression.

## Runtime safety invariants

> ## ⚠ HARD RULE — NO COOLDOWNS, NO BUFFERS, NO TIMER BALANCING
>
> If the user presses a button, do the thing on the tick it is sampled. There is
> **no input buffer** (no 150 ms or any press buffer), **no cooldown**, and **no
> timer-based gating** anywhere in the movement path (dash, down-dash, jump,
> freeze, walk, air movement, contact reset). Abilities are gated only by their
> availability and a fresh input edge. Do not add a timer as a substitute for a
> contact, an edge, or an availability flag. The hot movement owns the input
> edge; the input layer only samples raw key state. Any buffer or cooldown here
> is a regression. See
> `docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`.

- Gameplay collision, damage, and physics run at fixed 60 Hz, never directly
  once per render frame.
- VSync remains forced off globally. Do not add a setting, command, config
  path, or code path that enables it.
- Collision changes must preserve the cached broadphase path and avoid
  per-query allocations in hot loops.
- Use centralized categorized logging. Do not create unmanaged debug files.
- Established user-visible configuration remains owned by the configuration
  system and hot-reloadable where its specification requires it.
- Players and NPCs share gameplay systems wherever practical; input source is
  the primary distinction.
- Gameplay actions should use the shared command/action surface where
  practical so they can be tested without duplicating behavior in the GUI.

## Code and architecture

- One concept has one owner, one source of truth, and one clear responsibility.
- Reuse an existing owner before expanding or creating another function,
  state variable, file, packet, or parallel subsystem.
- Before adding a state variable or source of truth, search for an equivalent.
- Prefer the smallest patch and delete duplicate or obsolete code when safe.
- Keep main/orchestration files focused on orchestration.
- Add no abstraction unless its ownership, testability, and performance cost
  are justified.

## Production and secrets

- Never expose or commit credentials, tokens, private keys, cookies, database
  connection strings, or secret environment values.
- Never edit production source or configuration directly.
- Before deployment, verify the exact reviewed branch and commit.
- Deploy only committed local code, preserve pre-existing untracked files, and
  restart only the affected service.

## Live-runtime direction (from 2026-09-13)

- Governing principle: aim to make as much of the repo as possible hot
  reloadable so it can be edited live. Prefer a generic runtime mechanism over a
  new cold seam.
- Do not add a subsystem-specific field, enum, switch case, or call site to
  `mimita.exe` when an existing generic primitive (package, system, event,
  component schema, capability, resource, command) can express it.
- New gameplay/editor concepts should be registered packages/systems/schemas/
  events/behaviors/commands, not new kernel slots. See
  `docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`.
- Only a genuinely new OS/hardware/kernel primitive may require kernel
  evolution; then add a generic provider, not a feature-specific slot.
- `modecreate` is the first proof: its controls, selection, copy/paste,
  free-fly, notifications, commands, and source structure must be changeable
  while the same EXE/world/session/EntityIds stay alive.

## What belongs elsewhere

The router is the single source for relative paths to detailed documentation.
Detailed specifications, architecture rules, network tracing, build and EXE
procedures, deployment commands, asset rules, JSON inventories, focused review
checklists, regression history, release instructions, and examples belong in
the routed documents, not in this file.

The live-runtime direction and the generic "no new slot" bootstrap plan are in
`docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`.
