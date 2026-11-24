# Measurement, hypotheses, and recursive self-improvement

Status: gold behavior standard for behavior, tuning, latency, and tooling work.

Purpose: make every attempted improvement falsifiable and checkable with real
numbers. A build proves compilation. It does not prove loading, behavior,
quality, or a player-visible outcome.

Related:

- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/change-to-evidence-latency.md`
- `docs/specs/dev-loop/agent-self-improvement.md`
- `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/architecture/headers/header-hygiene.md`
- `docs/features/npc-behavior.md`

## 1. Core rule

No behavior or quality claim without:

1. a stated, falsifiable hypothesis;
2. a pinned before measurement;
3. a pinned after measurement;
4. the exact journal path for both;
5. the real numbers, units, sample size, and date.

"I changed it and it feels better" is a starting observation, not proof. A
journal event without a matching executable/DLL identity is diagnostic only.

## 2. Hypothesis record template

Every nontrivial attempt should be recordable in this shape:

```text
Hypothesis ID:
Date (UTC):
Agent / owner:
Issue / context:
Underlying goal:
Hypothesis (falsifiable):
Single change being tested:
Baseline:
  run_id:
  game_generation + DLL SHA-256:
  executable SHA-256:
  mode / map / profile:
  duration or rounds:
  journal paths:
  metric values:
Expected effect:
  metric:
  from:
  to:
  tolerance / noise:
Result:
  run_id:
  game_generation + DLL SHA-256:
  journal paths:
  metric values:
  delta:
  sample size:
  pass / fail:
Confounders and limitations:
Decision: keep / revert / iterate
Next hypothesis:
```

A hypothesis must be disprovable. "Make NPCs better" is not a hypothesis.
"Elite movement will raise bhop/min from 2.0 to at least 4.0 and reduce TTK
from 2.4 s to at most 1.7 s without reducing defuse completions" is.

## 3. Measurement rules

- Pin the exact executable, game DLL generation/SHA, mode, map, profile,
  duration, and journal.
- Keep the scenario comparable. Compare rounds with rounds, not one 20-second
  sample with a 20-minute match.
- Report raw units and calibrated units when scale is uncertain.
- Report mean, median, p90, max, counts, and sample size. A single aggregate is
  not enough.
- Change one variable at a time. If several must change, use separate builds or
  explicitly flag the comparison as confounded.
- Pre-register the acceptance threshold before seeing the result.
- Record negative and inconclusive results. They are data, not failure to hide.
- Do not lower a threshold after seeing the result.
- Keep evidence classes separate: source, build, runtime journal, visual/human.
- A header split or refactor is not behavior proof even when it compiles.

## 4. Gold examples from the NPC/FCS work

### Phase 0 baseline and measurement instrument

- Baseline run `20261123_164421`, gen564.
- Added raw + calibrated movement metrics and an offline analyzer.
- CT 11.83 m/s, dash 17.52/min, bhop 2.03/min, stuck 21.90/min; T 10.15 m/s,
  dash 10.85/min, bhop 1.30/min, stuck 15.46/min.
- Lesson: without this baseline, the later "more movement tech" claim would be
  unfalsifiable.

### Phase 1 CT defuse

- Hypothesis: preferring a living NPC defuser and holding the interaction would
  turn "assigned but never completed" into completed defuses.
- Before: gen564 had plants but no completed defuse.
- After: run `20261123_172947`, gen567: 4 T plants and 4 CT NPC defuses.
- Lesson: the before/after event chain, not the source diff, proved the result.

### Phase 2 elite profile and movement

- Before: gen564 TTK mean 2.40 s, median 0.84 s; CT dash 17.52/min, bhop
  2.03/min, air 24.8%.
- After: run `20261123_205333`, gen571: TTK mean 1.08 s, median 0.45 s; CT dash
  25.43/min, bhop 3.76/min, air 26.1%; T dash 22.04/min, bhop 3.21/min; 3/3
  CT defuses.
- Also recorded the misses: air target 30% not met; CT stuck 21.42/min and T
  stuck 18.89/min were not reduced; CT near-wall/blocked motion increased.
- Lesson: a win and a regression can coexist. Report both.

### Header latency

- Hypothesis: moving NPC-only debug flags out of `src/config.h` would reduce an
  NPC flag edit from 284 affected sources to a small set.
- Before: 284 affected sources for `src/config.h`.
- After: 6 affected sources for `src/debug/config-npc.h`; gen571 build succeeded
  in 829.795 s.
- Lesson: header work is measured with fanout and build time, not gameplay
  metrics; a short non-regression run confirms behavior stayed neutral.

### Readiness bug

- Observed: failed run `20261123_182722` was in phase 4 intermission and had
  server tick stalls up to 4,494 ms.
- Fix hypothesis: a canonical `network.mode.active` event plus a simulation-time
  budget and server-health gate would classify slow servers correctly.
- Result: gen571 run `20261123_205333` reached online through
  `network.mode.active`; the failure class changed from false "no active mode"
  to explicit `server_stalled` evidence.
- Lesson: measure the failure itself; a bad test gate can look like a gameplay
  regression.

## 5. Recursive AI self-improvement loop

1. Observe a user report, journal anomaly, or metric plateau.
2. State the underlying goal, not only the symptom.
3. Form one falsifiable hypothesis.
4. Make the smallest change that tests it.
5. Measure before/after on a pinned run.
6. Compare the real numbers and decide keep/revert/iterate.
7. Record the evidence in the issue, feature dossier, regression, or changelog.
8. Update the specification or workflow when the evidence changes the contract.
9. Propose the next hypothesis from the new data.
10. Have another agent review high-impact or contradictory evidence.

The shared issue, feature dossier, regression records, and changelog are the
long-term memory. The loop is not complete until the evidence is written where
the next agent will find it.

## 6. Header and latency hypotheses

Header work uses the same method with different metrics:

```text
Hypothesis: splitting <header> into <parts> will reduce affected sources from A
to B and one-header build time from T to U without changing gameplay metrics.

Measure:
  direct fan-in,
  affected sources before/after,
  real build seconds from .dev/build-timings,
  compile/link iterations,
  non-regression run: TTK, kills/round, stuck, defuse, dash/bhop/air.
```

Keep the split only when the compile metric improves and behavior stays within
the agreed tolerance. Do not mix a header split with a behavior change in one
build; otherwise the result is not attributable.

Prioritize by **edit frequency × fanout**. NPC headers are edited often;
`structured-log.h` is high-fanout but low-frequency. That is why NPC headers
come first.

## 7. Anti-patterns

- Build success claimed as behavior proof.
- One cherry-picked run or one short window.
- Several variables changed in one build.
- No pinned artifact, generation, or journal path.
- Wrong or unstated unit scale.
- Reporting only the metric that improved.
- Hiding or deleting a negative result.
- Moving the acceptance threshold to match the result.
- Synthetic tests replacing the real runtime path for user-visible behavior.
- A test-gate failure treated as a gameplay failure without checking server
  health and simulation progress.

## 8. Minimum evidence checklist

- [ ] Goal and falsifiable hypothesis.
- [ ] Exact issue or feature context.
- [ ] Baseline run ID, generation/SHA, mode/map, journal.
- [ ] Pre-registered expected metric and threshold.
- [ ] After run ID, generation/SHA, journal.
- [ ] Real before/after numbers with units and sample size.
- [ ] Confounders and limitations.
- [ ] Decision and next hypothesis.
- [ ] Evidence recorded in the right issue/feature/regression/changelog.

## 9. Relationship to completion

A behavior task is not complete because it compiles, because a file changed, or
because one metric moved. It is complete when the pinned evidence shows the
expected outcome, the regressions were checked, and the result is recorded where
another agent can reproduce and challenge it.
