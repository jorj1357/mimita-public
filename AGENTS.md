# MiMITA agent instructions — proposed compact version

Favor deleting code over adding code when both solutions achieve the same result.

## MANDATORY GATES

Read these gates before doing anything else:

0. AGENT BUILD AUTHORITY: An agent edits and observes; it never builds. Agents
   must not run `build.py`, `build_agent.py`, `buildv2.py`,
   `module-build-worker.py`, `devloop --request build`, or any direct
   compiler/`ccache` command. The required target is an isolated mode 2 lane
   (`devscripts/agent-lane.py create/ensure/status/run`) rooted under this
   checkout's ignored `.dev/agent-lanes/` directory. Agent-created worktrees,
   temporary files, caches, scheduler state, builds, and journals must resolve
   inside the project directory; do not use sibling folders, `%TEMP%`,
   `%LOCALAPPDATA%`, or another checkout as an agent work area. The current
   lane manager does not yet honor this root, so do not create or launch lanes
   until the implementation is migrated. If an operation cannot stay inside
   the project, stop and report the blocker instead of falling back outside.
   The lane dev-loop auto-detects edits and builds. Only a human may authorize
   an emergency outside-path exception or use
   `MIMITA_BUILD_OVERRIDE=1`. Read
   `docs/specs/dev-loop/agent-lanes-build-authority.md` before any edit.

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

4. RUNTIME EVIDENCE BEFORE NEW TESTS: For gameplay or user-visible behavior,
   do not create a new synthetic self-test as the first response. As of
   2026-11-05 16:45 EST (jorj1357), the preferred and primary test method is
   dev-loop trial/error: implement the change, make it compile, publish and run
   the exact executable through the dev loop, exercise the real behavior, and
   inspect its live `events.jsonl` journal. Iterate until the journal shows
   beyond reasonable doubt that the function worked as intended. Use bounded
   owner-level StructuredLogger diagnostics rather than a new test harness.
   Keep pure unit/component self-tests only for genuinely isolated logic such
   as serialization, hashing, or a pure mathematical invariant; never treat a
   self-test as proof of runtime or user-visible behavior.

5. DEV LOOP IS THE LIVE BUILD/RUN AUTHORITY: For live gameplay or debugging
   fixes, run `python devscripts/dev-loop.py` from the repository root using
   the appropriate profile. Use its status/control path to reuse an existing
   loop; do not launch a competing loop, manually run a stale executable, or
   substitute `build_agent.py` for live runtime evidence. If a cold build is
   genuinely required by the change, record why, preserve unrelated running
   processes, and still validate the resulting behavior through the dev-loop
   runtime journal.

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
- `devscripts/quality/agent-gate.py` is optional and is not required to finish
  work. For gameplay behavior, use the live debug loop in
  `docs/specs/dev-loop/dev-loop.md`: make the change, compile it, launch the
  executable in the target mode, exercise the behavior, and inspect that
  executable's `events.jsonl`. Repeat with better owner-level evidence when
  the journal does not prove the expected result. A gate or synthetic check
  does not replace this live evidence.
- Record source, build, test, runtime, deployment, and human-review evidence
  as separate claims.
- Every repository-touching AI session creates exactly one final changelog.
- Confirmed regressions are recorded append-only; ordinary work history is not
  a regression.

## Runtime safety invariants

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

## What belongs elsewhere

The router is the single source for relative paths to detailed documentation.
Detailed specifications, architecture rules, network tracing, build and EXE
procedures, deployment commands, asset rules, JSON inventories, focused review
checklists, regression history, release instructions, and examples belong in
the routed documents, not in this file.

This compact instruction set includes the live dev-loop rule above so that
build identity, runtime ownership, and user-visible evidence remain coupled.
