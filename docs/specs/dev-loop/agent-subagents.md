// 2026-11-20
/* purpose
* specify parent agents, cohorts, subagents, delegation, budgets, and shared build/test pointers
* let one owner split work without turning every subagent into a separate build authority
* this file DOES NOT grant build authority or policy authority to subagents
* this file DOES NOT permit nested uncontrolled process/build fan-out
*/

# Agent subagents and cohorts

## Terms

- **Parent agent**: owns an issue/task and its final decision.
- **Subagent**: named worker delegated a bounded task by a parent.
- **Cohort**: a group of agents/subagents sharing a task group and build/test
  pointers.

Example:

```text
map-editor (parent, cohort: map-editor)
  - map-editor-chunks
  - map-editor-materials
  - map-editor-teleporters
  - map-editor-outliner
```

The cohort shares a preview/integration pointer. Subagents do not each need a
separate EXE or full build stream.

## Registration

Agent record additions:

```text
parent
cohort
capabilities
state
budget
```

Suggested capabilities:

```text
build-request
code-edit
scenario
runtime-test
map-editor
network
physics
docs
review
```

Capabilities are advisory routing hints, not permission grants.

## Delegation

A parent creates a subagent with:

- name
- parent
- cohort
- task/issue
- allowed paths or subsystems
- expected deliverable
- proof expectation
- budget/deadline
- required reviewer

The subagent reports:

- files changed
- build/runtime used
- events/proof
- blockers
- remaining work
- recommendation

The parent owns the final decision and integration request.

## Authority

Subagents may:

- edit only their delegated paths;
- propose preview builds;
- run isolated tests through the root dev-loop;
- create findings/notes;
- ask other agents;
- escalate to the parent.

Subagents may not:

- grant themselves build authority;
- change `AGENTS.md`, specs, build policy, or production;
- create unlimited nested subagents;
- approve their own proof;
- merge another agent's files;
- kill unowned processes.

## Cohorts and build pointers

A cohort has one pointer:

```text
build/game/cohorts/<cohort>/current.json
```

- All subagents see the cohort pointer.
- A subagent may request a preview overlay containing its own changes.
- Preview proof cannot close an issue.
- The parent requests the integration build that closes the issue.
- A cohort pointer is retained while any runtime/proof references it.

## Budgets

Each parent specifies:

- max active subagents
- max concurrent file leases
- max concurrent runtimes
- max preview builds
- deadline/token/time budget
- required reviewer count

The system refuses new subagents or builds when the budget is exhausted and
reports the reason on the board.

## Activity and blockers

Subagent activity rolls up to the parent/cohort:

- phase
- task/issue
- paths
- build/generation
- runtime/journal
- blocker
- next step
- wait reason

The parent sees the rollup through `agent-coord board` and `sync`.

## Conflict handling

- Subagents use normal edit leases and shared-file regions.
- Sibling conflicts go to the parent.
- Cohort conflicts go to the task owner.
- A shared header conflict may require a split or a patch proposal.
- No silent overwrite or auto-merge.

## Nested depth

- Default maximum nesting: parent -> subagent.
- Subagent -> sub-subagent requires parent approval.
- Every nested level increases the activity/rollup and budget requirements.
- Runaway delegation is capped and visible.

## Evidence

Subagent proof must identify:

- subagent
- parent/cohort
- task/issue
- build request/generation
- runtime/run
- journals
- matched events
- final result

The parent must link the subagent evidence into the issue trace.

## Acceptance

- A parent can delegate bounded work and see subagent activity.
- Subagents share the cohort pointer but cannot silently create separate build
  authority.
- Subagent edits use normal leases and conflicts.
- Budget exhaustion is visible and stops new fan-out.
- Parent-owned integration proof is required to close the issue.
- A subagent can escalate a blocker without stalling the cohort.
- Nested depth and concurrent subagents are bounded.

## Open review questions

- Should subagents be allowed to run their own preview builds by default?
- Should a cohort pointer be created at cohort creation or first build?
- What is the default maximum active subagent count?
- Can a parent delegate issue closure, or only task completion?
