# MiMITA agent instructions — proposed compact version

Favor deleting code over adding code when both solutions achieve the same result.

## MANDATORY GATES

Read these gates before doing anything else:

0. SHARED CHECKOUT BUILD AUTHORITY: Work in this repository root, on its
   currently checked-out branch, and use its existing dev-loop and executable.
   Do not create or use mode 2 lanes, extra worktrees, branches, or a second
   checkout for normal agent work. Preserve existing lane data; do not delete
   or merge it as part of an unrelated task. Agents may edit different files
   concurrently, but must coordinate ownership so no two agents edit the same
   file.
   Agents request builds and isolated test runs through the root dev-loop; they
   never invoke a compiler, `build.py`, a module worker, or a second loop
   directly. The root dev-loop remains the only builder. Mode 3 batch triggering
   is the default: a saved compiler input joins the visible pending batch and
   does not start compilation. After the compiler-input owners report ready,
   the root coordinator issues one `dev-loop.py --request batch-build` to seal
   that batch. An explicit `--request build` or `--request game-dll` also seals
   a pending batch. `MIMITA_BUILD_TRIGGER_MODE=auto` is the deliberate Mode 1
   automatic-on-save fallback.
   Forbidden: `build.py`, `build_agent.py`, `buildv2.py`,
   `module-build-worker.py`, `ccache`, a compiler, and starting a second
   dev-loop. Do not set `MIMITA_BUILD_OVERRIDE=1`; that emergency variable is
   human-only.
   Allowed through the root dev-loop: `dev-loop.py --request batch-build`,
   `--request build`, `--request game-dll`, `--request cold-link-trial`,
   `--start-test-run`, and the one-shot agent test path. In Mode 3, issue one
   build request only after the batch is ready; do not request a build on each
   save. Every request records the agent, task, permit, build ID, and test run.
   One EXE build runs at a time. Candidate game-DLL builds may be requested as
   often as needed and run concurrently only within configured CPU/memory
   limits. Isolated test runs pin one EXE artifact and one game-DLL generation
   or path; they have their own processes and journals and do not replace the
   shared runtime.
   One checkout and one source tree are current policy. Per-agent source
   isolation is deferred; do not create worktrees or source overlays until the
   specification changes. Read
   `docs/specs/dev-loop/agent-lanes-build-authority.md`,
   `docs/specs/dev-loop/change-to-evidence-latency.md`, and
   `docs/specs/dev-loop/agent-coordination.md` before any edit.

0b. AGENT COORDINATION: Before editing, run
   `python devscripts/agent-coord.py begin --agent <task-name> --task <description>`.
   At the start of every work activation, run
   `python devscripts/agent-coord.py sync`; during long work, check
   `python devscripts/dev-loop.py --status`. Before writing a shared or leased
   file, use `agent-coord.py edit begin`; `edit commit` and `edit abort` release
   the file immediately. At each phase boundary, update
   `agent-coord.py activity set` with the task, issue, paths, build, run,
   blocker, next step, and wait reason. Run `agent-coord.py board` to see every
   agent's reported work. At the end of the task, run
   `python devscripts/agent-coord.py end`. The shared brain, directives,
   decisions, review documents, and optional intent announcements are advisory
   except for active edit leases, halt, and freeze. Read
   `docs/specs/dev-loop/agent-coordination.md` before relying on it.

   At every activation, inspect the `brain_check` returned by `sync`. Every
   coordination `send`, `ask`, or `reply` also returns a fresh brain check;
   review its current issue and claimable work. When a message contains useful
   evidence, a suggestion, question, or objection about an issue, pass
   `--issue <id>` so it is appended to that issue's discussion. Use
   `agent-coord issue suggest <id> "<suggestion>"` for a standalone proposal.
   Do not create duplicate issues or append unrelated status chatter. Seeing an
   unowned issue does not assign it; choose it with `agent-coord next --claim`.

   RESOURCE EVIDENCE HANDOFF: Whenever an agent observes MiMITA CPU, memory,
   disk, or build-resource pressure, append the evidence to the appropriate
   existing issue (or create one complete issue if none fits) before or with
   the handoff. Include UTC and sample duration; host logical CPU count; process
   PID/role and CPU normalized both per logical CPU and to whole-machine
   capacity; exact live mode from the runtime journal (the dev-loop GUI launch
   mode may differ after an in-game switch); player/NPC and workload state;
   frame cap/FPS or server tick data when available; concurrent build phase
   and top competing processes for build-pressure claims; and journal/artifact
   paths. Label direct observations, inferences, and unknowns separately.
   Cross-link a related feature/build issue when ownership lies elsewhere, and
   leave follow-up unowned unless accepting it with `next --claim`. Do not
   duplicate the report or post status-only chatter.

   CROSS-AGENT SIGNAL: When a build failure, dirty file, lease, runtime, stale
   proof, or resource-pressure symptom looks owned by another agent, do not
   guess or overwrite it. Send one bounded question with the exact evidence
   before continuing:
   `python devscripts/agent-signal.py --agent <me> --path <path>
   --build-id <id> --run-id <id> --question "..."`.
   The tool matches reported activity, asks the likely owner, and broadcasts to
   all when no owner is found. Then run `sync`/`inbox`; record the answer or
   timeout in the issue. Never stop, kill, or edit another agent's work to
   unblock yourself.

   USER CONTEXT IS SHARED: Every user message must enter the brain exactly once
   with its verbatim text and a stable source reference. The OpenCode prompt
   plugin does this automatically; other runtimes must call
   `agent-coord context add --source user --session ... --source-ref ...`.
   Review `user_context`/`brain_check` on every sync. If a context is linked to
   the wrong issue, use `context move` with a reason and notify the affected
   owners; if ownership is ambiguous, open a context-routing issue. Never
   delete or rewrite the original quote. High-priority context wakes active
   agents; normal context is delivered at the next activation.

   PLAN-MODE COORDINATION: Plan mode remains read-only for repository files,
   builds, tests, and runtime processes. Reading coordination state and, when
   the host permits, sending/replying to coordination messages or appending
   issue suggestions are allowed and encouraged as part of planning. Use them
   to resolve ownership or design questions; do not edit code or start/stop
   builds in plan mode.

0c. ISSUES AND SELF-IMPROVEMENT: Preserve user reports verbatim in an
   append-only issue record. Every new issue must include title, exact quote or
   observation, summary, reproduction, expected behavior, acceptance, paths,
   evidence, risk, proof expectation, and any relevant context. Use
   `devscripts/agent-issue.py` or the equivalent full `issue add` fields;
   incomplete creation is rejected. Triage into owned tasks and steps;
   use `issue assign`, `task dep`, `issue defer`, and `issue clarify`. Do not
   edit a spec or TODO to resolve a report without user clarification.
   When a build or test waits, record why; a wait above the measured budget or
   an avoidable wait creates a `build-friction` issue. A behavior task cannot
   be marked done or its issue closed without a verified `runtime_proven` proof
   or an explicit proof exemption with a reason. Use
   `agent-coord proof task-add`, `proof task-verify`, `proof exempt`, and
   `issue exempt`; the board must show unproven and stale changes. Agents are expected to propose safe
   improvements to coordination, build, test, documentation, and capability
   surfaces, with evidence and a metric target. Agents may review and iterate;
   a human makes the final decision on changes to shared authority. Read
   `docs/specs/dev-loop/agent-proof-and-verification.md`,
   `docs/specs/dev-loop/change-to-evidence-latency.md`,
   `docs/specs/dev-loop/agent-self-improvement.md`, and
   `docs/workflows/issue-intake-and-triage.md`.
   Automatically create or update one complete issue when an agent observes
   actionable recurring friction, a defect, a missing capability, or a
   user-facing workflow improvement; do not wait for the user to ask for issue
   intake. Preserve exact user wording when applicable, distinguish observed
   evidence from inference, and link the user context. Leave follow-up work
   unowned unless explicitly claimed so future agents can discover it through
   `agent-coord next --claim`. Issue creation records work for review; it does
   not authorize a scope-changing fix or a change to shared authority.
   While waiting, run `agent-coord next --mode waiting`; repair missing issue
   fields, missing evidence, and stale proofs instead of idling. A woken agent
   must inspect proof state and may not claim behavior completion without
   verified evidence or a reasoned exemption.

0d. GENERIC OWNERS AND TECH DEBT: Before editing networking, combat, contact,
   damage, build, launch, test, or coordination code, review whether the change
   creates a duplicate packet, handler, formula, state variable, name, or
   workflow. Prefer the existing generic owner. File duplicate, hardcoded,
   misleading-name, unused-file, disk/build-waste, or inefficient-workflow
   findings as `tech-debt`/`duplication` issues with evidence and a suggested
   fix; leave them for review. Do not silently refactor a shared path. The
   target nodes/connections knowledge contract is
   `docs/specs/architecture/nodes-and-connections.md`.

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

   LIVE GAME ON ISSUE INTAKE: When a user reports MiMITA gameplay, rendering,
   UI, networking, or other behavior that could be observed in a running game,
   first discover matching checkout processes with
   `python devscripts/evidence-watch.py list --json`. If a relevant live run is
   present, inspect its exact PID/run, executable and build identity, mode/map,
   and journal with `evidence-watch show`/`tail` before asking the user to
   relaunch or treating the report as source-only. Record the observed event
   and any identity gaps in the existing issue. This is read-only discovery:
   do not adopt, stop, restart, or otherwise control a human- or other-agent-
   owned process. If no relevant game is running, record that and continue the
   routed source/log investigation; do not claim runtime proof.

5. DEV LOOP IS THE LIVE BUILD/RUN AUTHORITY: For live gameplay or debugging
   fixes, run `python devscripts/dev-loop.py` from the repository root using
   the appropriate profile. Use its status/control path to reuse an existing
   loop; do not launch a competing loop, manually run a stale executable, or
   substitute `build_agent.py` for live runtime evidence. If a cold build is
   genuinely required by the change, record why, preserve unrelated running
   processes, and still validate the resulting behavior through the dev-loop
   runtime journal. Agents are authorized and encouraged to restart the
   checkout-owned daemon and dev-loop dashboard GUI when a source edit, stale
   process, wedged control path, or live proof requires it. Follow the routed
   restart skill, preserve unrelated processes and healthy game pairs unless
   the scenario requires replacement, then verify the GUI/daemon and exact
   runtime journal before claiming proof.

6. DLL CHANGE NOTES: Before the first edit to any whole-game DLL compiler
   input, queue a note with `python devscripts/build-note.py --changes
   "short description" --files path/to/file`. Queue one note for each
   independent agent change before saving its compiler inputs. If several
   agents' changes land in one DLL attempt, each note must remain attached to
   that attempt; the dev-loop consumes all pending notes together. Never wait
   for the build to add the note afterward. The final build result and its
   notes must be traceable from the dev-loop `events.jsonl` and the per-attempt
   `.dev/build-timings/` summary.

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
- For behavior, tuning, latency, or tooling changes, state a falsifiable
  hypothesis and record real before/after numbers with pinned run IDs,
  generation/SHA, units, sample size, and journal paths. Follow
  `docs/workflows/measurement-hypothesis-driven-improvement.md`.
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

- Resource cleanup preference: at task/phase boundaries and before ending a
  session, periodically review MiMITA EXEs started for agent work. When an
  agent-owned client or runtime is no longer needed, stop it through its
  supported dev-loop or runtime-registry control and verify it exited; preserve
  its journal and artifacts. Do not stop a process unless checkout ownership
  and active use are verified. Keep processes needed by active tasks, open proof
  or explicit pins, game pairs still needed by an active task, and human- or
  other-agent-owned sessions. If ownership or use is unclear, leave it running
  and report why.
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
- Header fan-out: before editing a shared header, run
  `python devscripts/dependency_graph.py --affected <header>` and follow
  `docs/architecture/headers/header-hygiene.md`. Record the affected-source
  count in the build note. Over 50 affected sources needs a short fanout plan;
  over 100 needs human/root review. Do not add unrelated types to a common
  header, and never add project headers to `src/pch.h`.
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
- Never hand-edit production application source or config. Deploy committed,
  pushed code through the website-only procedure in
  `docs/operations/website-agent-deployment.md`.
- **Standing user authorization (2026-11-22, issue
  `issue-260667351e04`):** any AI agent with working GitHub and
  `ssh mimita-vps` authentication may implement, commit, push, and deploy
  routine MiMITA website changes without asking for per-deployment approval.
  This applies only to the active shared checkout branch, the website release
  allowlist, and the checks and rollback procedure documented for mimita.fun.
  The user explicitly requested that authenticated agents be able to fix the
  website end to end. Verify the pushed commit SHA and release contents
  yourself, record them, preserve pre-existing untracked data, and restart only
  the affected website service.
- This standing authorization does not cover database/schema/data migrations,
  deleting or overwriting user data or backups, changing credentials or
  account permissions, domain/DNS/billing changes, unrelated VPS services, or
  bypassing a failed validation. Stop and ask before those actions or when the
  deployment target, authentication, rollback, or source state is uncertain.
- Use existing OS/Git/SSH authentication only; never request or copy secrets
  into chat, source files, command output, or logs. An agent without the
  required authenticated access may finish local work and must report the
  specific access setup needed; it may not claim deployment.

## What belongs elsewhere

The router is the single source for relative paths to detailed documentation.
Detailed specifications, architecture rules, network tracing, build and EXE
procedures, deployment commands, asset rules, JSON inventories, focused review
checklists, regression history, release instructions, and examples belong in
the routed documents, not in this file.

This compact instruction set includes the live dev-loop rule above so that
build identity, runtime ownership, and user-visible evidence remain coupled.
