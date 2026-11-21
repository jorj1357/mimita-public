# Edit-to-working latency investigation scope

## Issue update

- Preserved the user's latest report verbatim as a linked user context and an
  append-only issue quote on `issue-136d4553ff92`.
- Reused the existing active improvement issue, “Account for every DLL build
  stage and investigate end-to-end speed,” rather than creating a duplicate.
- Added an agent suggestion that extends its investigation to the complete
  edit-to-first-visible-result path, using the NPC aggression example.
- Put the investigation order in the suggestion: first measure and fix
  invalidation/caching and required dependency fan-out; then compare DLL reload,
  EXE build paths, and other measured approaches; treat external libraries as
  a later option and assess concrete candidates with platform, compiler,
  license, integration, and prototype evidence.
- Required acceptance to distinguish build completion, same-process
  activation, and first visible/runtime result, while tracking rollback and
  state safety.

## Evidence and limits

- `issue-136d4553ff92` readiness reports state `triaged`, two linked tasks, and
  no missing required issue fields.
- This session updated issue context and investigation guidance only. No code,
  compiler, build configuration, runtime, or external application was changed.
- No build or runtime test was run. No latency result or behavior proof is
  claimed.
