# issue-c4f2df8edeba: Track and eliminate cross-agent interference in the shared checkout

## Human report (verbatim, immutable)

> Codegen stale again due other agent. - stuff like this make a n issue or track , like, ai agent interference with each other, all agents shoudl work toward more seamlessness of working together over time


## Agent suggestion

### 2026-11-22T05:22:13.748Z — issue-review-sweeper

Process proposal from issue-review-sweeper's independent review of issue-52b59bdd440c (2026-11-22).

Evidence: the review could not run that issue's required runtime reproduction because devscripts/agent_coord.py hardcodes git apply cwd to this checkout (ROOT at :28) independently of the --root coordination-state option (:4044, :4524). An isolated --root repro would still write into the shared working tree, so a bounded reviewer either mutates the shared checkout or skips the evidence. The review skipped it; zero coord.proposal.* events exist in the journal.

Gap: reviewers cannot safely reproduce coordination proposal/lease behavior in isolation, which blocks the proof-first completion loop and risks cross-agent interference.

Measurable target: 100% of coordination proposal/lease repros run against an isolated state root with zero writes to the shared checkout, proven by identical `git status --porcelain` before and after, in one agent session.

Smallest experiment: derive the effective git-apply/repo cwd from the coordination state root (fall back to ROOT when unset), then run one two-agent scratch-file edit propose/apply repro under `--root <temp>` in a temp git repo and diff shared-checkout `git status` before/after (expect identical). Do not change the one-writer lease rule.
