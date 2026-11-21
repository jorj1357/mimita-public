# Fun Counter Strike NPC regression intake

- User report: recorded verbatim once in coordination context `ctx-601820524b48` and linked to `issue-be1adb51b6ac`.
- Issue: added and triaged high priority as `issue-be1adb51b6ac`; linked follow-up task `fcs-npc-regression-repro` requires pinned live reproduction and movement/report tracing.
- Regression history: appended the reported recurrence and explicit evidence limits to `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md`.
- Desired contract checked: `docs/specs/gamemodes/funcounterstrike.md`, section 12 specifies stable Terrorist site commitment and CT site defense with perception-gated team reports.
- Existing evidence: the Oct. 4 Counter-Strike regression records a similar spawn escape/targeting concern and explicitly says live CT-spawn escape acceptance remained pending. It does not establish this Fun Counter Strike behavior as previously runtime-proven.
- Current runtime check: `python devscripts/dev-loop.py --status` reported active run `20261120_232200` with launch identity `sandbox`, build generation 416 had failed for low disk space. This is not valid FCS behavior evidence. No build or test was requested or run.
- Result: regression report and acceptance criteria are now tracked. Root cause, current FCS reproduction, proof of the historical good baseline, code fix, and live acceptance remain open. Do not increase global speed/dash tuning before measuring net progress and tracing the first failing movement or report stage.
- Workflow: read `docs/ROUTER.md`, NPC/FCS routed specs, movement ownership docs, issue intake workflow, coordination/build authority docs, and `docs/skills/spec-behavior-review-v1.md` plus `docs/skills/documentation-checker-v1.md`. Documentation review found the user report aligns with section 12; the requested historical baseline remains user-reported rather than proven.
