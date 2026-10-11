# Expanded dev-loop launch UX plan

- Time: 2026-10-11T16:02:41.287Z
- Result: PASS
- Scope: documentation only; no source, configuration, firewall, process, or
  launch behavior was changed.
- Changed file: `docs/jorj-docs/20261011plan.md:53-215`

## What changed

Expanded the `Dev-loop launch UX` section from informal notes into an
observable behavior contract covering:

- current AI/dev-loop behavior and the user-visible focus-stealing failure;
- continued use of the real published `.exe`;
- `--unlock-cursor` startup behavior and preservation of the `L` toggle;
- hidden/headless server and support-console expectations;
- the distinction between `ai-headless`, `ai-visual`, and `human` launches;
- foreground-window and text-box-focus requirements;
- repeated Windows Firewall prompt behavior and private/local network safety;
- exact current-versus-required acceptance rows;
- runtime evidence requirements through dev-loop status and `events.jsonl`;
- a bounded verification sequence after implementation.

The plan explicitly does not authorize silently enabling public network access,
skipping the real executable, deleting the dev-loop evidence path, or forcing
human visual tests to be invisible.

## Documentation checks

- `git diff --check -- docs/jorj-docs/20261011plan.md`: passed.
- Read `docs/skills/documentation-checker-v1.md`: the updated section has an
  explicit purpose and a “What this request does not mean” boundary.
- `docs/doc-review-09-03-2026.md` was routed by `docs/ROUTER.md` but is not
  present in this checkout; no replacement review document was invented.
- The repository-wide TODO scan found existing documentation TODOs and they
  were intentionally not edited outside this request. Relevant examples are:
  - `docs/specs/dev-loop/dev-loop.md:1`: “todo explain what”; suggested
    follow-up: define the dev-loop purpose and authority in a dedicated spec.
  - `docs/specs/dev-loop/dev-loop.md:16`: “todo explain like what the point of
    hti”; suggested follow-up: explain the idea-to-runtime feedback loop.
  - `docs/jorj-docs/misc-bugs.md:1`: “todo explain”; suggested follow-up:
    replace it with a bug-entry template and evidence fields.

No regression record was added because this session changed documentation and
did not establish a new runtime regression.
