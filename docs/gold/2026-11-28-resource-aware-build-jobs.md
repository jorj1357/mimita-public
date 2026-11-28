# Gold reference: resource-aware build jobs keep the PC usable

- Timestamp: `2026-11-28`
- Audience: MiMITA developers and AI agents working on builds or the dev loop
- Status: user-endorsed direction; specific before/after responsiveness is not measured here
- Related policy: [`../specs/dev-loop/build-preflight-and-admission.md`](../specs/dev-loop/build-preflight-and-admission.md)
- Related budget: [`../specs/dev-loop/build-performance-budget.md`](../specs/dev-loop/build-performance-budget.md)
- Related implementation: `devscripts/build_scheduler.py`

## User feedback

> can u put a gold reference agent note  that  resource aware build jobs making pc not die when  builds  bc it used to  like dead stop bc so muchbuilds  but this is good

The user is recognizing resource-aware build jobs because previous build activity could make their PC effectively stop responding. This is useful product feedback: build throughput matters, and so does keeping the development machine usable while work runs.

## Agent guidance

- Preserve and improve memory-aware admission and bounded build concurrency. Do not treat maximum parallel jobs as the goal.
- Reuse the existing scheduler's resource checks rather than adding a competing resource probe.
- When resources are constrained, make the wait/admission reason visible and resume when capacity is available.
- Keep build speed and host usability as separate outcomes. Do not claim the scheduler alone prevents freezes without matched measurements or direct user confirmation.
- When investigating pressure, record host CPU and available memory alongside the build phase and relevant competing processes, following the resource-measurement guidance.

## Evidence boundary

The current build policy documents resource admission, and the scheduler has an existing available-memory gate. Historical build records include severe host pressure during builds, which supports why this direction matters. This note records the user's positive feedback; it does not establish a measured before/after reduction in freezes or host resource use.

## Keep agent-owned runtimes from becoming leftovers

A MiMITA process should have a clear owner, purpose, run ID, journal, and release event. At task or phase end, agents should review the processes they started and stop only their own verified runs once the proof/task/release event says they are no longer needed. Use the supported dev-loop or runtime-registry stop path, then verify the process exited and preserve its journal and artifacts.

Never stop a human-owned or another agent's active run. If ownership or use is unclear, inspect the run manifest, coordinator/runtime registry, executable identity, and journal; ask the owner or leave it running and report the uncertainty. Treat unregistered MiMITA processes as an observability/ownership gap to resolve, not automatic permission to kill them. This follows the runtime registry and process lifecycle contracts.

## CPU issue progress snapshot

As of the latest detailed entries in `issue-ed3ac775283d` (2026-11-27), the work has produced substantial diagnosis, but no verified CPU optimization result:

- Direct per-thread sampling across four MiMITA processes measured a combined ~40.8% of whole-host CPU on a 12-logical-CPU machine while host CPU readings were saturated. This was a multi-process workload, not a controlled single-process baseline.
- Runtime timers found high NPC simulation cost at 19 NPCs, but a zero-NPC run still showed about 6.2 ms/tick unattributed inside NPC simulation and a 19.13 ms average server tick. NPC cost is a major lead, not the only known cost.
- A separate discussion identifies the client's 999 FPS cap as a strong CPU lead. It remains a candidate for a matched test, not a proven fix.
- Earlier code candidate generation 642 had successful build evidence, but its pinned runtime/CPU comparison was still pending in the issue. A successful build alone is not a CPU improvement.

Next useful evidence is a controlled, pinned before/after run that records process and host CPU, mode/map, player/NPC counts, frame cap/FPS, tick timings, and journal paths. Preserve the existing 60 Hz simulation and global VSync policy while investigating.

## Collaborate like contributors to a shared repository

Use the helpful parts of a Git collaboration model: make intended changes visible, coordinate ownership before touching shared files, keep changes reviewable, preserve other contributors' work, record why a change was made, and attach evidence so another agent can continue it. Treat agents as collaborators with independent work and shared responsibility for a coherent checkout.

In this checkout, that means following the current coordinator, edit leases, issue discussions, dev-loop build authority, and one-source-tree policy. Git is an analogy for collaboration habits; it does not grant permission to create branches/worktrees or override the repository's current shared-checkout rules.
