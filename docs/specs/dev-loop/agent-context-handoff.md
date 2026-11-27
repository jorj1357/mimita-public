# Agent context handoff and process autonomy

- **Status:** active policy
- **Recorded:** 2026-11-27
- **Owner:** dev-loop / agent coordination
- **Related:** `AGENTS.md` gates 0b and 5,
  `docs/specs/dev-loop/agent-coordination.md`

## Purpose

Long tasks can outlive one agent session. A session that stops because its
context is nearly full must not strand the tree with half-applied edits, an
unpublished build, or no owner for the remaining work. The same rule lets an
agent run enough checkout-owned MiMITA processes to actually finish a task
without asking the user for permission on every step.

## Process autonomy

- An agent may start as many checkout-owned MiMITA EXEs and isolated test runs
  as its task needs.
- Use the root dev-loop for builds and runs. Do not start a second loop or call a
  compiler directly.
- Keep within configured CPU/memory limits. Prefer short, bounded test runs.
- Record every run ID and build ID in the issue, activity entry, or handoff.
- Clean up only the agent's own processes at task/phase boundaries. Preserve
  human-owned and other-agent-owned runs.
- Do not stop, adopt, kill, or relaunch another agent's run unless the user
  explicitly asks or the coordination tools prove it abandoned.

## Queued work

When a plan lists implementation items, execute them in order without asking
for per-item confirmation. Continue through build, runtime proof, documentation,
and the next queued item. Stop only when:

- a decision materially changes scope, behavior, or shared authority;
- the tree would be left unbuildable;
- another agent's active lease blocks the exact file and a bounded question has
  no answer after one short window (then take the lease carefully per
  `agent-coordination.md`).

## Context handoff trigger

Start the handoff when any of these is true:

- the context window is roughly 70% or more used;
- the model reports that context is full or nearly full;
- the session can no longer hold the open files and evidence it needs;
- the remaining work is larger than the reliable remaining context.

Do not wait until the last turn. Leave enough room to write the handoff, wake
the next agent, and verify the tree.

## Required handoff document

Path: `docs/handoffs/YYYY-MM-DD-<task>-handoff.md`.

It must contain:

1. **Task and session identity** — agent name, date, branch/checkout, current
   generation and build ID.
2. **Current state** — what is implemented, what is only source, what is proven
   in runtime, and what is still queued.
3. **Evidence** — pinned run IDs, generation/SHA, journal paths, event names,
   before/after numbers, screenshot/artifact paths. Separate build from runtime
   proof.
4. **Open items** — ordered next steps, blockers, and acceptance criteria.
5. **File ownership** — exact files this agent changed, files owned by other
   active agents, and files the next agent may edit.
6. **Build/run instructions** — the root dev-loop commands, current build
   self-repair runbook state, and any cache quarantine.
7. **Coordination state** — active agents, leases, issues, unanswered questions.
8. **Known limits** — what was not proven, what needs human review, and what
   must not be claimed.

## Waking the next agent

- Open a **new top-level OpenCode chat**, not a subagent. Use the durable wake
  adapter:
  `python devscripts/agent-wake.py add --agent <agent> --when manual --adapter
  opencode --prompt "Read docs/handoffs/... and continue ..."` followed by
  `python devscripts/agent-wake.py run --once`.
- The OpenCode adapter launches `opencode run ... --auto --title "wake <id>"`,
  which is a separate chat/session. A `subagent` call does not satisfy this
  requirement; it shares the parent session and is only for bounded parallel
  work.
- Give the new chat the handoff path and a short instruction: read `AGENTS.md`,
  `docs/ROUTER.md`, and the handoff; continue the queued items; preserve the
  listed file ownership.
- The new agent becomes the continuation owner. It must run `agent-coord sync`,
  inspect the board, and claim/continue the task.
- If the wake fails, leave the handoff and a coordination note for the next
  activation; do not leave the tree unbuildable.

## Tree state at handoff

- Save all intended edits. Do not leave a compiler input half-written.
- If a build is in progress, let it finish or record the attempt ID and state.
- Do not revert another agent's work.
- The handoff is not a substitute for a build: compile the tree before ending
  when the remaining context allows. If it does not, record the exact build
  state and the next command.

## Acceptance

- A fresh agent can read only `AGENTS.md`, the router, and the handoff and
  continue without re-discovering the task.
- Every claimed result has a run/build ID and journal/artifact path.
- The handoff names active file owners and the next safe file to edit.
- The previous agent's processes are either cleaned up or explicitly handed
  over with PIDs/run IDs.
