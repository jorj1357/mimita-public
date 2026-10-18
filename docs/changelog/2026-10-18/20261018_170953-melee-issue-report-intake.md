# Melee issue report intake

- Result: PASS_WITH_HUMAN_REVIEW
- Local timestamp: 2026-10-18 13:09 EDT (America/New_York); recorded at 2026-10-18T17:09:53Z.
- Branch: `20261011zt`; no commit created.
- Pre-existing work: `docs/specs/gamemodes/supersmashbrosmelee.md` was already modified before this session. Its earlier edits were preserved; this session appended only the report at lines 828–840.

## Change

- `docs/specs/gamemodes/supersmashbrosmelee.md:828-840`
- Prior text at EOF: none.
- Exact addition:

```markdown
### 2026-10-18 13:09 America/New_York — Jorj Melee issue report

- Crouching does not work as intended. A server position error was also seen;
  its cause is unknown.
- Right mouse currently aims in during Melee. Desired behavior: disable aiming
  for this mode so right mouse does not change the field of view or move the
  right arm.
- Shield and grab need visible effects comparable to the freeze effect. The
  shield, grab, and every attack need a visible hitbox during their active
  behavior.
- Holding jump does not work reliably. The reported desired behavior is for
  held-jump input to be handled on the client because the server feels too slow;
  the implementation and authority details still need investigation.
```

The entry captures reported symptoms and intent. It does not claim these behaviors were reproduced or prescribe a final network design.

## Review and validation

- Read: `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/workflows/runtime-scenario-validation.md`, and `docs/specs/gamemodes/gamemodes.md`.
- Documentation checker: the router references `docs/doc-review-09-03-2026.md`, which is missing at that path. Continued with the available focused documentation checker; no change to routing was in scope.
- `git diff --check -- docs/specs/gamemodes/supersmashbrosmelee.md` found trailing whitespace in earlier pre-existing informal notes at lines 805–826; the new lines 828–840 are clean.
- No code, build, runtime, or gameplay validation was requested or performed. Human reproduction and acceptance remain outstanding.
