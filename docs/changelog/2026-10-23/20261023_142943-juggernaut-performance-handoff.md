# Juggernaut 240 FPS contributor handoff

Time: 2026-10-23T14:29:43Z (10:29:43 AM America/New_York)
Branch: `20261011zt`
HEAD at start: `cbfa4b0d`
Commit created: no

## Purpose and result

Added a contributor handoff to `docs/specs/open-gameplay-issues.md` summarizing the best available Juggernaut performance evidence, distinguishing measured results from likely explanations, and giving a safe ordered plan for the next live test and optimization pass.

The handoff says the 240 FPS goal is not met. It labels the client frame path as the leading area to profile, not a proven root cause. It limits the synchronous audio file-read finding to startup/first-use stalls and explicitly says it does not explain late-run cost or establish a memory leak. It also notes the source journal referenced by the prior session record is no longer present in this checkout, so the numbers need fresh verification.

## Exact document change

File: `docs/specs/open-gameplay-issues.md`, new section begins at line 161.

Before insertion, the issue-index table was followed by:

```markdown
## Triage for the 2026-10-16 play report
```

After insertion, that heading follows the new section, which begins:

```markdown
## Juggernaut 240 FPS performance investigation handoff — 2026-10-23

**Audience:** a contributor continuing live Juggernaut performance work. This
handoff records the current evidence and investigation order. It does not
identify a proven single root cause, approve a threading redesign, or claim the
240 FPS target has been met.
```

The new handoff records the session-reported debug build 2887 measurement (6,300 frames over game time 22–82.3 seconds; 9.320 ms average; 6,034/6,300 over 4.17 ms), the later 67-NPC server sample, the stack-traced audio cache file-read path, private-byte growth that is not proof of a leak, and the intermittent journal quieting. Its next-contributor sequence is: establish a safe release worker limit, capture a fresh 60-second mode-9/Fighter run, record thread/GPU/frame pacing and memory evidence, compare across deaths and the prior 50–60 second window, then optimize only the measured owner. The target is fewer than 1% of frames over 4.17 ms, ideally zero, with debug and release reported separately.

The existing inbox and issue-index edits around this insertion were already uncommitted before this change; they were preserved and not attributed to this session.

## Sources and review

- `docs/ROUTER.md`, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/regressions/README.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md` were reviewed.
- Performance specification supports the 4.17 ms threshold and requires per-thread critical-path profiling before multi-core redesign.
- Documentation review: audience and scope are stated; existing owner documents are linked; claims are qualified; no new requirement was silently decided.
- Regression review: no new regression record was added because this handoff records an unresolved performance target miss and investigation, not a confirmed fixed/reintroduced behavior regression.

## Validation and limits

- The expected source run `logs/2026-10-23/20261023_053635/events.jsonl` is absent from the current checkout. Its reported values are preserved from the prior session changelog, not freshly recomputed here.
- The linked prior changelog and performance/debug-logging/workflow owner documents exist. `git diff --check -- docs/specs/open-gameplay-issues.md` passed.
- No code, tests, or runtime state changed in this session; no build or live run was performed.
- Human/play acceptance and the strict 240 FPS target remain open.
