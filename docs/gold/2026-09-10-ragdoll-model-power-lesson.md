// 2026-09-10T22:19:14Z
/* purpose
* preserve the human's lesson that choosing a capable AI model materially changed ragdoll progress
* record the ragdoll progress assessment exactly as the human framed it
* keep model selection and physics evidence in separate claims
* this file does NOT claim a controlled comparison of AI models
* this file does NOT claim ragdoll acceptance is complete
* this file does NOT replace repository specifications or validation
*/

# Gold lesson: choose a strong enough model for hard solver work

- Timestamp: `2026-09-10T22:19:14Z` (2026-09-10 18:19 EDT)
- Audience: future AI agents and the human selecting an implementation model
- Related records:
  - `docs/gold/2026-09-10-ragdoll-rigid-body-thought-process.md`
  - `docs/changelog/2026-09-10/20260910_220958-ragdoll-activation-rest-bind.md`
  - `docs/regressions/regressions-v1.md`
- Human progress framing: ragdoll mode progressed from roughly “5% implemented”
  to roughly “95% implemented” during the day’s work. Those percentages are the
  human’s qualitative assessment, not measured test coverage or feature
  completion.

## What the human observed

The human concluded that model capability meaningfully affected the ragdoll
outcome. Earlier work used `mimo v2.5`, selected partly for lower cost, and its
performance was judged insufficient for the remaining physics, binding, joint,
and regression work. Work shifted to `deepseek v4.1 flash`, and the human
associates that shift with substantially better diagnosis, smaller but correct
changes, and clearer evidence.

This is recorded as a human lesson rather than a controlled experiment. The
repository does not have a side-by-side benchmark in which both models started
from the same commit, received the same prompt, and were scored against the
same acceptance checklist.

## Practical lesson

Choose a model strong enough for the causal reasoning required by the task.
Price alone is not the correct selection criterion when the work requires:

- tracing an input-to-output chain across animation, skeleton binding, physics,
  constraints, and rendering;
- distinguishing position, orientation, and reference-frame defects;
- preserving unrelated concurrent work while making the smallest patch;
- recording source, build, runtime, and human-review evidence separately.

The practical rule adopted here is: match the powerful repository work with a
powerful model, especially for solver behavior, regressions, and visual truth.

## What still needs verification

- The model hypothesis has not been tested by rerunning the same task with the
  comparison model.
- The recent ragdoll corrections still require human runtime acceptance,
  including activation limb orientation, grabs, reach/stretch behavior, and
  corpse spawning.
