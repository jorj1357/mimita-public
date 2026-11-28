# RPG issue dedupe and verbatim context recovery

- Wake `wake-15ed1c56cbcb` for high-priority context `ctx-86635dce35df` ("canu put this all as issue in teh issue tracker?").
- Verified the user report was already captured as `issue-78c8b724ed0e` (Author persistent RPG worlds and reusable in-engine content), with dependency slices `issue-565902eed8a7`, `issue-2ecdfa48ec44`, `issue-29115dfd03db`, `issue-5251f107d7c3`.
- Found and closed duplicate `issue-263445630d0c` (identical intake created 30s earlier, no linked context) as a duplicate of `issue-78c8b724ed0e`. Proof exemption recorded because a duplicate is not a behavior claim.
- Found the brain context stored only the follow-up line, not the original vision message. Recovered the verbatim text from codex session rollout `rollout-2026-11-28T12-43-16-01a22341-0da2-75a0-8a9b-c5b68da90362.jsonl` line 13 and appended it as issue quote #80 on the canonical issue.
- Also reported duplicate voice-chat intake pair `issue-6e082da43cac` / `issue-29b3665c767d` as an issue suggestion on the context-linked canonical `issue-29b3665c767d`, left for its owner to consolidate.
- Evidence status: issue-tracker routing only. No source, build, or runtime validation performed or claimed.
