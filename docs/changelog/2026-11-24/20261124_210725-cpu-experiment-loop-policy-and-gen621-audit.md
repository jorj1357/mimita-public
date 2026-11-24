# CPU experiment loop policy and gen621 runtime audit

**Date:** 2026-11-24 UTC  
**Issue:** `issue-ed3ac775283d`  
**Change type:** Collaboration-spec update and read-only runtime audit; no MiMITA game source changed.

## User direction

The user wants the primary resource goal to reduce MiMITA CPU use through
repeated code changes, with every CPU-effort build proving its effect in the
running game and its `events.jsonl` evidence. The active goal already contains
this requirement and remains active.

## Collaboration-spec change

Updated `docs/specs/dev-loop/agent-self-improvement.md` with a scientific
improvement loop: separate observations/inferences/unknowns, state one
falsifiable hypothesis, pin the same workload and artifact before/after,
change one variable, retain negative results, and make keep/revert/iterate
decisions from evidence. After a resolution or plateau, agents should list up
to three distinct, testable next hypotheses in the same issue, without
automatically opening duplicate issues or starting unowned work. For explicit
iterative optimization goals, each build requested for that goal must include
one named goal-relevant code change; if no safe candidate is supported, the
iteration is measurement improvement and must not be reported as a CPU win.

Before the edit, the surrounding text ended:

> and linked artifacts alone.

After the edit, a new `## Scientific improvement loop` section follows that
sentence. The addition begins:

> Agents treat each nontrivial change as an experiment.

and ends:

> and record that pass as an instrumentation step, not as a resource reduction.

Also updated `docs/specs/dev-loop/agent-coordination.md` under
`### Proactive issue intake` to route iterative optimization work through the
falsifiable one-change/matched-evidence loop and to keep up to three follow-up
hypotheses in the same issue.

Before that edit, the section ended:

> normal coordination and propose evidence-based solutions over time.

After that sentence, the new paragraph begins:

> For iterative optimization work, agents use the falsifiable, one-variable
> experiment loop in [agent self-improvement](agent-self-improvement.md).

## Runtime audit

Read-only inspection of run `20261124_205956` at 2026-11-24 21:06 UTC found
server PID 9988 reporting Juggernaut Horde on `dust2cyberiav4`, 67 NPCs, and
zero players. Host game DLL generation was 621, while
`npc.phase-window` reported module generation 620. Server tick windows showed
about 51–67 ms average NPC work, with individual ticks over 100 ms. The
one-second NPC phase windows accumulated roughly 620–681 ms across 945–1350
updates; reported wall-time categories included collision 63–112 ms, physics
134–152 ms, decision 62–123 ms, and sensing 17–61 ms. These are elapsed scope
timings, not CPU-time percentages.

Client PID 22208 in the same run ID reported Bloodtest, one NPC, and one player;
its frame slices had 11/24, 11/18, and 7/14 frames above 4.167 ms, with p95
frame time between 27 and 68 ms. The server reported zero players. This is not
a matched client/server workload despite the shared run ID and root status
listing both processes. The generation mismatch and mode/player mismatch make
this unsuitable for a before/after CPU claim. No OS CPU sample, profiler
command, process control, build, or game-source edit was performed.

## Next discriminating work

1. After the runtime owner releases the run, establish one fresh,
   same-mode client/server pair and verify mode, player/NPC counts, process
   IDs, and host/NPC module generation from fresh journals.
2. Measure thread CPU in the NPC update path before choosing an optimization;
   current collision/physics/decision scope values are wall time only.
3. Evaluate distinct candidates only when supported by that profile: reduce
   repeated collision/physics work while preserving contacts; reduce repeated
   perception/decision/candidate-gather work while preserving outcomes; or
   quantify the whole-machine cost of multiple live runs and prevent abandoned
   owned runs from remaining active.

No optimization result is claimed. Every CPU-effort build will use one named
source change and matched runtime before/after evidence, with the current
240-FPS per-frame and 60-Hz server requirements and gameplay correctness as
separate gates.
