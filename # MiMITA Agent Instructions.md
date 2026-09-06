# MiMITA Agent Instructions

Favor deleting code over adding code when both solutions achieve the same result.

## Documentation

Before starting repository work, read `docs/ROUTER.md` and follow the documents
it routes.

Specifications define desired behavior. Code is an implementation. When code
disagrees with a current specification, explain the disagreement and align the
implementation with the specification when safely possible.

Do not duplicate detailed specifications, architecture rules, workflows,
operations, or review checklists in this file.

## Engineering Defaults

- Search the repository before making assumptions.
- Prefer the smallest correct change.
- Reuse existing owners, functions, configuration, and tests.
- Prefer deleting duplicate code over adding parallel implementations.
- Preserve unrelated user changes.
- Treat uncertain conclusions as hypotheses until supported by code, logs, or tests.
- Keep gameplay behavior, configuration ownership, and diagnostics in their
  documented subsystem owners.

## Completion

Follow `docs/operations/task-completion/task-completion.md` for validation,
focused skills, changelogs, regression records, and human review.

When code changes, follow `docs/operations/build-and-exe/build-and-exe.md`.

When VPS or production work is involved, follow
`docs/operations/vps-deployment/vps-deployment.md`.

## Safety

Do not perform destructive actions or production changes unless they are clearly
within the requested task and allowed by the relevant operation document.

Historical and archived documents are background reference only. They do not
override current specifications, architecture, workflows, operations documents,
skills, or regression records.