# Changelog: proposed compact AGENTS draft

- Timestamp: 2026-09-09T12:04:08Z
- Branch: not changed or inferred
- Commits: none created
- Status: draft only; the real `AGENTS.md` was not modified

## Pre-existing changes

The working tree already contained modifications to:

- `docs/architecture/code-ownership/code-ownership.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/regressions/2026-09-09/duel-specific-gamemode-ownership-REG.md`

Those files were not edited in this session.

## Change made

Created `AGENTS-POSSIBLE.md` beside `AGENTS.md` as a 79-line proposed compact
replacement. It contains:

- a mandatory router-chain gate;
- a build-versus-runtime evidence gate;
- a stale-binary/data-path gate;
- compact authority, evidence, completion, architecture, runtime-safety,
  production, and secret rules;
- an explicit statement that the router is the single map for detailed
  documentation paths;
- an explicit statement that the real `AGENTS.md` remains unchanged.

The draft intentionally does not reproduce the router's task-routing table or
the detailed content of specifications, workflows, operations, architecture
documents, skills, regressions, or examples.

## Reasoning

The user asked for a planning artifact before changing `AGENTS.md`. The draft
therefore demonstrates the target shape without deleting or moving existing
rules. It preserves the critical gates while delegating detailed paths and
procedures to the router and its routed documents.

## Validation

- Read the existing `docs/ROUTER.md` and current `AGENTS.md` before drafting.
- Confirmed the draft is present and measures 79 lines.
- Ran `git diff --check`. It reported a pre-existing trailing-whitespace issue
  at `docs/specs/gamemodes/gamemodes.md:265`; no new issue was introduced by
  the draft.
- No code build or runtime test was applicable.

## Human review still needed

Review the draft's compact rule set and decide which detailed sections should
be migrated before rewriting `AGENTS.md`. In particular, decide whether
release policy, website design guidance, completion notifications, and the
network-feature workflow should be added or revised before the real migration.
